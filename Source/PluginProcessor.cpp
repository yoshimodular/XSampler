#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SfzOverride.h"
#include <regex>

namespace ParamID
{
    static constexpr auto masterGain      = "master_gain";
    static constexpr auto tuneGlobal      = "tune_global";
    static constexpr auto pitchbendRange  = "pitchbend_range";
    static constexpr auto octaveTranspose = "octave_transpose";
    static constexpr auto startOffset     = "start_offset";
    static constexpr auto analogAmount    = "analog_amount";
    static constexpr auto doublerEnabled  = "doubler_enabled";
    static constexpr auto voiceMode       = "voice_mode";
    static constexpr auto legatoEnabled   = "legato_enabled";
    static constexpr auto portamentoTime  = "portamento_time";
    static constexpr auto portamentoSync  = "portamento_sync";
    static constexpr auto portamentoRate  = "portamento_rate";
    static constexpr auto fingeredPort    = "fingered_portamento";
    static constexpr auto filterType      = "filter_type";
    static constexpr auto filterCutoff    = "filter_cutoff";
    static constexpr auto filterResonance = "filter_resonance";
    static constexpr auto volAttack       = "vol_attack";
    static constexpr auto volDecay        = "vol_decay";
    static constexpr auto volSustain      = "vol_sustain";
    static constexpr auto volRelease      = "vol_release";
    static constexpr auto filterAttack    = "filter_attack";
    static constexpr auto filterDecay     = "filter_decay";
    static constexpr auto filterSustain   = "filter_sustain";
    static constexpr auto filterRelease   = "filter_release";
    static constexpr auto lfoEnabled      = "lfo_enabled";
    static constexpr auto lfoWaveform     = "lfo_waveform";
    static constexpr auto lfoRate         = "lfo_rate";
    static constexpr auto lfoDepth        = "lfo_depth";
    static constexpr auto lfoDelay        = "lfo_delay";
    static constexpr auto lfoTarget       = "lfo_target";
    static constexpr auto velToVolume     = "velocity_to_volume";
    static constexpr auto velToFilter     = "velocity_to_filter";
    static constexpr auto filterEnvAmount = "filter_env_amount";
    static constexpr auto sampleStart     = "sample_start";
    static constexpr auto tempoBpm        = "tempo_bpm";
    static constexpr auto tempoSync       = "tempo_sync";

    static constexpr auto arpEnabled = "arp_enabled";
    static constexpr auto arpHold    = "arp_hold";
    static constexpr auto arpMode    = "arp_mode";
    static constexpr auto arpRate    = "arp_rate";
    static constexpr auto arpOctaves = "arp_octaves";
    static constexpr auto arpGate    = "arp_gate";
}

namespace
{
    // Only structural changes require an SFZ overlay rebuild. Everything
    // else is driven by HDCC, instantly and seamlessly.
    // Changes to these IDs require an SFZ overlay rebuild. Everything else
    // is HDCC-driven and applies seamlessly.
    const juce::StringArray& structuralParams()
    {
        static const juce::StringArray ids {
            ParamID::filterType,
            ParamID::lfoWaveform,
            ParamID::lfoTarget,
            ParamID::voiceMode,
            ParamID::legatoEnabled,
            ParamID::doublerEnabled
        };
        return ids;
    }
    // These are still rebuild-required, but get applied IMMEDIATELY (with
    // a brief click on active voices) rather than waiting for silence,
    // because the user expects them to take effect on toggle.
    const juce::StringArray& urgentStructuralParams()
    {
        static const juce::StringArray ids {
            ParamID::voiceMode,
            ParamID::doublerEnabled,
            ParamID::legatoEnabled
        };
        return ids;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout XSamplerAudioProcessor::createLayout()
{
    using FloatParam  = juce::AudioParameterFloat;
    using IntParam    = juce::AudioParameterInt;
    using BoolParam   = juce::AudioParameterBool;
    using ChoiceParam = juce::AudioParameterChoice;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    using Range = juce::NormalisableRange<float>;
    // Helper: skewed range biased toward the lower end (typical for time/freq).
    auto skewLog = [] (float a, float b, float skew = 0.3f) {
        return Range (a, b, 0.0f, skew);
    };

    // -------- Global ------------------------------------------------------
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::masterGain, 1 },      "Master Gain",       Range (0.0f, 1.0f),                                     0.8f));
    layout.add (std::make_unique<IntParam>    (juce::ParameterID { ParamID::tuneGlobal, 1 },      "Tune Global",       -100, 100, 0));
    layout.add (std::make_unique<IntParam>    (juce::ParameterID { ParamID::pitchbendRange, 1 },  "Pitchbend Range",   1, 24, 12));
    layout.add (std::make_unique<IntParam>    (juce::ParameterID { ParamID::octaveTranspose, 1 }, "Octave Transpose",  -3, 3, 0));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::sampleStart, 1 },     "Sample Start",      Range (0.0f, 1.0f),                                     0.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::analogAmount, 1 },    "Analog / Doubler Amount", Range (0.0f, 1.0f, 0.01f),                          0.0f));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::doublerEnabled, 1 },  "Doubler Mode",      false));

    // -------- Voicing ----------------------------------------------------
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::voiceMode, 1 },       "Voice Mode",        juce::StringArray { "Poly", "Mono" }, 0));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::legatoEnabled, 1 },   "Legato",            false));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::portamentoTime, 1 },  "Portamento Time",   skewLog (0.0f, 20.0f, 0.3f),                             0.0f));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::portamentoSync, 1 },  "Portamento Sync",   false));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::portamentoRate, 1 },  "Portamento Rate",   juce::StringArray { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar" }, 4));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::fingeredPort, 1 },    "Fingered Portamento", false));

    // -------- Filter -----------------------------------------------------
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::filterType, 1 },      "Filter Type",       juce::StringArray { "LP", "HP", "BP" }, 0));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterCutoff, 1 },    "Filter Cutoff",     skewLog (20.0f, 20000.0f),                              8000.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterResonance, 1 }, "Filter Resonance",  Range (0.0f, 1.0f),                                     0.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterEnvAmount, 1 }, "Filter Env Amount", Range (-1.0f, 1.0f),                                    0.0f));

    // -------- Volume ADSR + Velocity → Volume ---------------------------
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::volAttack, 1 },       "Vol Attack",        skewLog (0.001f, 10.0f),                                0.01f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::volDecay, 1 },        "Vol Decay",         skewLog (0.001f, 10.0f),                                0.1f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::volSustain, 1 },      "Vol Sustain",       Range (0.0f, 1.0f),                                     0.8f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::volRelease, 1 },      "Vol Release",       skewLog (0.001f, 20.0f),                                0.3f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::velToVolume, 1 },     "Velocity → Volume", Range (0.0f, 1.0f),                                     0.8f));

    // -------- Filter ADSR + Velocity → Filter ---------------------------
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterAttack, 1 },    "Filter Attack",     skewLog (0.001f, 10.0f),                                0.01f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterDecay, 1 },     "Filter Decay",      skewLog (0.001f, 10.0f),                                0.1f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterSustain, 1 },   "Filter Sustain",    Range (0.0f, 1.0f),                                     0.8f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterRelease, 1 },   "Filter Release",    skewLog (0.001f, 20.0f),                                0.3f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::velToFilter, 1 },     "Velocity → Filter", Range (0.0f, 1.0f),                                     0.0f));

    // -------- LFO --------------------------------------------------------
    // No explicit LFO On/Off — depth=0 means inactive.
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::lfoWaveform, 1 },     "LFO Waveform",      juce::StringArray { "Sine", "Tri", "Saw", "Sq", "Random" }, 0));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::lfoRate, 1 },         "LFO Rate",          skewLog (0.01f, 20.0f),                                 2.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::lfoDepth, 1 },        "LFO Depth",         Range (0.0f, 1.0f),                                     0.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::lfoDelay, 1 },        "LFO Delay",         skewLog (0.0f, 4.0f, 0.4f),                             0.0f));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::lfoTarget, 1 },       "LFO Target",        juce::StringArray { "Pitch", "Filter", "Volume" }, 0));

    // -------- Tempo ------------------------------------------------------
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::tempoBpm, 1 },        "Tempo (BPM)",       Range (30.0f, 300.0f),                                  120.0f));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::tempoSync, 1 },       "Sync to Host",      true));

    // -------- Arpeggiator ----------------------------------------------
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::arpEnabled, 1 }, "Arp",        false));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::arpHold, 1 },    "Arp Hold",   false));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::arpMode, 1 },    "Arp Mode",   juce::StringArray { "Up", "Down", "UpDown", "Random", "AsPlayed" }, 0));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::arpRate, 1 },    "Arp Rate",   juce::StringArray { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 3));
    layout.add (std::make_unique<IntParam>    (juce::ParameterID { ParamID::arpOctaves, 1 }, "Arp Octaves", 1, 4, 1));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::arpGate, 1 },    "Arp Gate",   Range (0.05f, 1.0f),                                                 0.5f));

    return layout;
}

XSamplerAudioProcessor::XSamplerAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "XSamplerState", createLayout())
{
    synth = std::make_unique<sfz::Sfizz>();
    synth->setSampleRate (44100.0f);
    synth->setSamplesPerBlock (512);

    #define CACHE(field, id) field = apvts.getRawParameterValue (ParamID::id)
    CACHE (pMasterGain,      masterGain);
    CACHE (pTuneGlobal,      tuneGlobal);
    CACHE (pPitchbendRange,  pitchbendRange);
    CACHE (pOctaveTranspose, octaveTranspose);
    CACHE (pSampleStart,     sampleStart);
    CACHE (pAnalogAmount,    analogAmount);
    CACHE (pDoublerEnabled,  doublerEnabled);
    CACHE (pVoiceMode,       voiceMode);
    CACHE (pLegatoEnabled,   legatoEnabled);
    CACHE (pPortamentoTime,  portamentoTime);
    CACHE (pPortamentoSync,  portamentoSync);
    CACHE (pPortamentoRate,  portamentoRate);
    CACHE (pFingeredPort,    fingeredPort);
    CACHE (pFilterType,      filterType);
    CACHE (pFilterCutoff,    filterCutoff);
    CACHE (pFilterResonance, filterResonance);
    CACHE (pVolAttack,       volAttack);
    CACHE (pVolDecay,        volDecay);
    CACHE (pVolSustain,      volSustain);
    CACHE (pVolRelease,      volRelease);
    CACHE (pFilterAttack,    filterAttack);
    CACHE (pFilterDecay,     filterDecay);
    CACHE (pFilterSustain,   filterSustain);
    CACHE (pFilterRelease,   filterRelease);
    CACHE (pLfoWaveform,     lfoWaveform);
    CACHE (pLfoRate,         lfoRate);
    CACHE (pLfoDepth,        lfoDepth);
    CACHE (pLfoDelay,        lfoDelay);
    CACHE (pLfoTarget,       lfoTarget);
    CACHE (pVelToVolume,     velToVolume);
    CACHE (pVelToFilter,     velToFilter);
    CACHE (pFilterEnvAmount, filterEnvAmount);
    CACHE (pTempoBpm,        tempoBpm);
    CACHE (pTempoSync,       tempoSync);
    CACHE (pArpEnabled,      arpEnabled);
    CACHE (pArpHold,         arpHold);
    CACHE (pArpMode,         arpMode);
    CACHE (pArpRate,         arpRate);
    CACHE (pArpOctaves,      arpOctaves);
    CACHE (pArpGate,         arpGate);
    #undef CACHE

    for (const auto& id : structuralParams())
        apvts.addParameterListener (id, this);
    // lfo_depth is "soft-structural": rebuild only when its zero/non-zero
    // state changes (handled in parameterChanged).
    apvts.addParameterListener (ParamID::lfoDepth, this);

    synth->setNumVoices (32);
    lastNumVoices = 32;
    lastCC.fill (-1.0f); // ensure first flushParamCCs sends everything

    startTimerHz (20);
}

XSamplerAudioProcessor::~XSamplerAudioProcessor()
{
    stopTimer();
    for (const auto& id : structuralParams())
        apvts.removeParameterListener (id, this);
    apvts.removeParameterListener (ParamID::lfoDepth, this);
}

void XSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const juce::ScopedLock sl (synthLock);
    synth->setSampleRate (static_cast<float> (sampleRate));
    synth->setSamplesPerBlock (samplesPerBlock);

    const bool mono = pVoiceMode != nullptr && pVoiceMode->load() >= 0.5f;
    const int target = mono ? 1 : 32;
    if (target != lastNumVoices)
    {
        synth->setNumVoices (target);
        lastNumVoices = target;
    }

    gainSmooth.reset (sampleRate, 0.03);
    gainSmooth.setCurrentAndTargetValue (pMasterGain != nullptr ? pMasterGain->load() : 0.8f);

    keyboardState.reset();
    arp.prepare (sampleRate);
    lastCC.fill (-1.0f);
    heldNoteVel.fill (0);

    portaLastNote        = -1;
    portaCurrentSemis    = 0.0f;
    portaSemisPerSample  = 0.0f;
    portaSamplesRemaining = 0;
    portaCurrentBendValue = 8192;
    userPitchBendValue    = 8192;
}

void XSamplerAudioProcessor::releaseResources() {}

bool XSamplerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

bool XSamplerAudioProcessor::loadSfzFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const juce::String text = file.loadFileAsString();
    if (text.isEmpty())
        return false;

    currentSfzFile = file;
    currentSfzText = text;
    apvts.state.setProperty ("sfzPath", file.getFullPathName(), nullptr);

    rebuildAndApplyOverlay();
    return sfzLoaded.load (std::memory_order_acquire);
}

juce::File XSamplerAudioProcessor::getCurrentSfzFile() const
{
    return currentSfzFile;
}

juce::StringArray XSamplerAudioProcessor::getMissingSamples() const
{
    return missingSamples;
}

void XSamplerAudioProcessor::parameterChanged (const juce::String& id, float newValue)
{
    if (id == ParamID::lfoDepth)
    {
        const bool nowActive = newValue > 0.0001f;
        if (nowActive == lfoActiveCached) return;
    }

    // Legato only makes sense in Mono. If the user (or a preset) tries
    // to enable it while in Poly, snap it straight back to OFF.
    if (id == ParamID::legatoEnabled && newValue >= 0.5f)
    {
        const bool mono = pVoiceMode != nullptr && pVoiceMode->load() >= 0.5f;
        if (! mono)
        {
            if (auto* prm = apvts.getParameter (ParamID::legatoEnabled))
                prm->setValueNotifyingHost (0.0f);
            return; // don't propagate as a structural change
        }
    }

    // Switching back to Poly forces legato off so the gate stays consistent.
    if (id == ParamID::voiceMode && newValue < 0.5f) // Poly
    {
        if (auto* prm = apvts.getParameter (ParamID::legatoEnabled))
            if (prm->getValue() >= 0.5f)
                prm->setValueNotifyingHost (0.0f);
    }

    overlayDirty.store (true, std::memory_order_release);
    lastChangeMs.store (juce::Time::getMillisecondCounter(), std::memory_order_release);
    if (urgentStructuralParams().contains (id))
        overlayUrgent.store (true, std::memory_order_release);
}

void XSamplerAudioProcessor::timerCallback()
{
    if (! overlayDirty.load (std::memory_order_acquire)) return;

    const auto now   = juce::Time::getMillisecondCounter();
    const auto since = now - lastChangeMs.load (std::memory_order_acquire);
    if (since < 60) return;

    const bool urgent = overlayUrgent.load (std::memory_order_acquire);

    if (! urgent)
    {
        // Wait until silent so the click of a reload is hidden.
        int activeVoices = 0;
        {
            const juce::ScopedLock sl (synthLock);
            activeVoices = synth->getNumActiveVoices();
        }
        const bool forced = since > 2000;
        if (activeVoices != 0 && ! forced) return;
    }

    overlayDirty.store  (false, std::memory_order_release);
    overlayUrgent.store (false, std::memory_order_release);
    rebuildAndApplyOverlay();
}

void XSamplerAudioProcessor::flushOverlayNow()
{
    overlayDirty.store (false, std::memory_order_release);
    rebuildAndApplyOverlay();
}

void XSamplerAudioProcessor::rebuildAndApplyOverlay()
{
    if (! currentSfzFile.existsAsFile() || currentSfzText.isEmpty())
        return;

    XSamplerSfzParams sp;
    sp.mono       = pVoiceMode->load()      >= 0.5f;
    sp.legato     = pLegatoEnabled->load()  >= 0.5f;
    sp.doubler    = pDoublerEnabled->load() >= 0.5f;
    sp.filterType = (int) pFilterType->load();
    sp.lfoWave    = (int) pLfoWaveform->load();
    sp.lfoTarget  = (int) pLfoTarget->load();
    sp.lfoActive  = pLfoDepth->load() > 0.0001f;
    lfoActiveCached = sp.lfoActive;

    const juce::String combined = buildSfzWithOverride (currentSfzFile, sp);
    if (combined.isEmpty()) return;

    // Scan the combined source for sample paths and record any that don't
    // exist on disk. UI / hosts can surface this list to the user.
    {
        juce::StringArray missing;
        const std::regex re ("(^|[\\s\\t])sample=([^\\s\\r\\n]+)");
        const std::string s = combined.toStdString();
        for (auto it  = std::sregex_iterator (s.begin(), s.end(), re),
                  end = std::sregex_iterator(); it != end; ++it)
        {
            const auto& m = *it;
            juce::String path (m[2].str());
            if (! juce::File::isAbsolutePath (path)) continue; // unresolved
            if (path.startsWith ("*")) continue;               // sfizz oscillator
            if (! juce::File (path).existsAsFile())
                missing.addIfNotAlreadyThere (path);
        }
        missingSamples = std::move (missing);
    }

    {
        const juce::ScopedLock sl (synthLock);

        // Flush any active voices before reload — sfizz internal note
        // bookkeeping won't survive a region-set change cleanly otherwise
        // (caused hung notes when the doubler was toggled mid-play).
        synth->allSoundOff();

        const bool ok = synth->loadSfzString (
            currentSfzFile.getFullPathName().toStdString(),
            combined.toStdString());
        sfzLoaded.store (ok, std::memory_order_release);

        const int targetVoices = sp.mono ? 1 : 32;
        if (targetVoices != lastNumVoices)
        {
            synth->setNumVoices (targetVoices);
            lastNumVoices = targetVoices;
        }

        lastCC.fill (-1.0f);
        flushParamCCs (true);

        // Re-trigger every key the user is currently holding so the change
        // never feels like it dropped notes.
        synth->pitchWheel (0, 8192);
        const int octaveShift = (int) pOctaveTranspose->load() * 12;
        int lastReNoteOn = -1;
        for (int n = 0; n < 128; ++n)
        {
            if (heldNoteVel[(size_t) n] > 0)
            {
                const int shifted = juce::jlimit (0, 127, n + octaveShift);
                synth->noteOn (0, shifted, heldNoteVel[(size_t) n]);
                lastReNoteOn = n;
            }
        }
        // Reset portamento engine — the rebuild created a clean slate.
        portaCurrentSemis     = 0.0f;
        portaSamplesRemaining = 0;
        portaLastNote         = lastReNoteOn;
        portaCurrentBendValue = 8192;
    }
}

void XSamplerAudioProcessor::flushParamCCs (bool forceAll)
{
    if (forceAll) lastCC.fill (-1.0f);

    auto send = [this] (int idx, int cc, float v)
    {
        v = juce::jlimit (0.0f, 1.0f, v);
        if (std::abs (v - lastCC[(size_t) idx]) > 1.0e-5f)
        {
            synth->hdcc (0, cc, v);
            lastCC[(size_t) idx] = v;
        }
    };

    // Tune: -100..+100  →  0..1
    send (0,  XSamplerCC::Tune,       (pTuneGlobal->load() + 100.0f) * (1.0f / 200.0f));

    // Cutoff: 20..20480 Hz log
    {
        const float c = juce::jlimit (20.0f, 20480.0f, pFilterCutoff->load());
        const float n = std::log2 (c / 20.0f) * (1.0f / 10.0f); // 10 octaves
        send (1, XSamplerCC::Cutoff, n);
    }

    send (2,  XSamplerCC::Resonance,  pFilterResonance->load());

    // Vol ADSR
    send (3,  XSamplerCC::VolAttack,  pVolAttack->load()  * 0.1f);
    send (4,  XSamplerCC::VolDecay,   pVolDecay->load()   * 0.1f);
    send (5,  XSamplerCC::VolSustain, pVolSustain->load());
    send (6,  XSamplerCC::VolRelease, pVolRelease->load() * 0.05f);

    // Filter ADSR
    send (7,  XSamplerCC::FltAttack,  pFilterAttack->load()  * 0.1f);
    send (8,  XSamplerCC::FltDecay,   pFilterDecay->load()   * 0.1f);
    send (9,  XSamplerCC::FltSustain, pFilterSustain->load());
    send (10, XSamplerCC::FltRelease, pFilterRelease->load() * 0.05f);

    // Filter envelope amount: -1..+1 → CC 0..1 (0.5 = neutral, 0 = -4800c,
    // 1 = +4800c, both signs supported by the bipolar overlay declaration).
    send (11, XSamplerCC::FltEnvAmount, (pFilterEnvAmount->load() + 1.0f) * 0.5f);

    // LFO: only send when active (overlay declared). Sending CCs for
    // undeclared opcodes is harmless but wastes work.
    if (lfoActiveCached)
    {
        send (12, XSamplerCC::LfoRate,  pLfoRate->load()  * 0.05f);
        send (13, XSamplerCC::LfoDelay, pLfoDelay->load() * 0.25f);
        send (14, XSamplerCC::LfoDepth, pLfoDepth->load());
    }

    // Velocity tracking
    send (15, XSamplerCC::AmpVelTrack, pVelToVolume->load());
    send (16, XSamplerCC::FilVelTrack, pVelToFilter->load());

    // Analog amount: drives subtle random pitch + delay (only meaningful
    // when doubler is OFF; the doubler overlay re-uses the same CCs to
    // detune L vs R, so the slider feeds both modes from the same knob).
    const float analog = pAnalogAmount->load();
    send (17, XSamplerCC::PitchRandom, analog);
    send (18, XSamplerCC::DelayRandom, analog);

    // Sample start (offset)
    send (19, XSamplerCC::SampleStart, pSampleStart->load());
}

void XSamplerAudioProcessor::applyArpSettingsFromParams()
{
    arp.setEnabled (pArpEnabled->load() >= 0.5f);
    arp.setHold    (pArpHold->load()    >= 0.5f);
    arp.setMode    ((int) pArpMode->load());
    arp.setRate    ((int) pArpRate->load());
    arp.setOctaves ((int) pArpOctaves->load());
    arp.setGate    (pArpGate->load());
}

float XSamplerAudioProcessor::computePortamentoSeconds (double bpm) const
{
    const bool sync = pPortamentoSync->load() >= 0.5f;
    if (! sync)
        return pPortamentoTime->load();

    // Sync rate. Indices: 0=1/32, 1=1/16T, 2=1/16, 3=1/8T, 4=1/8,
    // 5=1/4T, 6=1/4, 7=1/2, 8=1 bar (assumes 4/4).
    const double secsPerBeat = 60.0 / juce::jmax (1.0, bpm);
    static constexpr double inBeats[] = {
        0.125,    // 1/32  = 1/8 beat
        1.0 / 6,  // 1/16T
        0.25,    // 1/16
        1.0 / 3,  // 1/8T
        0.5,     // 1/8
        2.0 / 3,  // 1/4T
        1.0,     // 1/4
        2.0,     // 1/2
        4.0      // 1 bar
    };
    const int idx = juce::jlimit (0, 8, (int) pPortamentoRate->load());
    return (float) (inBeats[idx] * secsPerBeat);
}

// Internal pitch-bend range matches what we declare in the SFZ overlay
// (bend_up=2400 / bend_down=-2400 → ±24 semitones). The user's
// `pitchbend_range` is applied as a scaling factor on the wheel input,
// not on the synth's bend range.
constexpr float kInternalBendSemis = 24.0f;

int XSamplerAudioProcessor::semisToPitchwheel (float semis, float /*userBendRange*/) const
{
    const float clamped = juce::jlimit (-kInternalBendSemis, kInternalBendSemis, semis);
    const float norm    = clamped / kInternalBendSemis;     // -1..+1
    const int   bend    = 8192 + (int) std::round (norm * 8191.0f);
    return juce::jlimit (0, 16383, bend);
}

void XSamplerAudioProcessor::startPortamentoTo (int newNote, double sampleRate, double bpm)
{
    const float timeSec = computePortamentoSeconds (bpm);
    const bool  fingered = pFingeredPort->load() >= 0.5f;
    const bool  mono     = pVoiceMode->load()    >= 0.5f;

    // Portamento only meaningful in mono (sfizz pitchwheel is global).
    if (! mono || timeSec <= 1.0e-4f || portaLastNote < 0)
    {
        portaCurrentSemis    = 0.0f;
        portaSamplesRemaining = 0;
        portaSemisPerSample  = 0.0f;
        return;
    }

    if (fingered && heldNoteVel[(size_t) portaLastNote] == 0)
    {
        // Last note no longer physically held: skip glide.
        portaCurrentSemis    = 0.0f;
        portaSamplesRemaining = 0;
        portaSemisPerSample  = 0.0f;
        return;
    }

    portaCurrentSemis = (float) (portaLastNote - newNote);
    const int totalSamples = juce::jmax (32, (int) (timeSec * sampleRate));
    portaSamplesRemaining = totalSamples;
    portaSemisPerSample   = -portaCurrentSemis / (float) totalSamples;
}

void XSamplerAudioProcessor::emitPortamentoBend (int sampleOffset, float userBendRangeSemis)
{
    // The user's pitch-bend wheel is interpreted with respect to *their*
    // pitchbend_range (1..24), then summed with the portamento offset.
    // Both arrive in semitones and are rendered to the synth's fixed
    // 24-semi internal range.
    const float userSemis = ((userPitchBendValue - 8192) / 8191.0f) * userBendRangeSemis;
    const float total     = userSemis + portaCurrentSemis;
    const int   bend      = semisToPitchwheel (total, userBendRangeSemis);

    if (bend != portaCurrentBendValue)
    {
        synth->pitchWheel (sampleOffset, bend);
        portaCurrentBendValue = bend;
    }
}

void XSamplerAudioProcessor::applyMasterGain (juce::AudioBuffer<float>& buffer)
{
    const int chans = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    for (int i = 0; i < n; ++i)
    {
        const float gain = gainSmooth.getNextValue();
        for (int c = 0; c < chans; ++c)
            buffer.getWritePointer (c)[i] *= gain;
    }
}

void XSamplerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Tempo: host or user knob, depending on tempo_sync.
    const bool sync = pTempoSync->load() >= 0.5f;
    double bpm = pTempoBpm->load();
    if (sync)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;
    arp.setBpm (bpm);
    applyArpSettingsFromParams();

    juce::MidiBuffer arpedMidi;
    arp.process (midi, arpedMidi, numSamples);

    if (! sfzLoaded.load (std::memory_order_acquire))
    {
        midi.clear();
        return;
    }

    gainSmooth.setTargetValue (pMasterGain->load());

    const juce::ScopedLock sl (synthLock);

    flushParamCCs (false);

    const int   octaveShift    = (int) pOctaveTranspose->load() * 12;
    const float bendRangeSemis = pPitchbendRange->load();

    // ---- MIDI dispatch with sample-accurate portamento --------------
    int lastEmittedSample = -1;
    constexpr int kPortaStep = 64;  // sub-block resolution for glide

    auto advancePortamentoTo = [&] (int targetSample)
    {
        // Emit pitchwheel updates from lastEmittedSample+1 to targetSample
        // in steps of kPortaStep, advancing portaCurrentSemis.
        if (portaSamplesRemaining <= 0) return;

        int s = juce::jmax (0, lastEmittedSample + 1);
        while (s < targetSample && portaSamplesRemaining > 0)
        {
            const int adv = juce::jmin (kPortaStep,
                              juce::jmin (targetSample - s,
                                          portaSamplesRemaining));
            portaCurrentSemis    += portaSemisPerSample * (float) adv;
            portaSamplesRemaining -= adv;
            s                    += adv;
            if (portaSamplesRemaining <= 0) portaCurrentSemis = 0.0f;
            emitPortamentoBend (s, bendRangeSemis);
        }
        lastEmittedSample = targetSample;
    };

    // Initial bend at sample 0 (covers leftover state from previous block).
    emitPortamentoBend (0, bendRangeSemis);
    lastEmittedSample = 0;

    for (const auto meta : arpedMidi)
    {
        const auto msg    = meta.getMessage();
        const int  sample = meta.samplePosition;

        // Catch portamento up to this event's position before processing it.
        advancePortamentoTo (sample);

        if (msg.isNoteOn())
        {
            const int rawNote = juce::jlimit (0, 127, msg.getNoteNumber());
            const int note = juce::jlimit (0, 127, rawNote + octaveShift);
            heldNoteVel[(size_t) rawNote] = (juce::uint8) msg.getVelocity();

            // Trigger portamento BEFORE the noteOn so the bend lands first.
            startPortamentoTo (rawNote, currentSampleRate, bpm);
            emitPortamentoBend (sample, bendRangeSemis);

            synth->noteOn (sample, note, msg.getVelocity());
            portaLastNote = rawNote;
        }
        else if (msg.isNoteOff())
        {
            const int rawNote = juce::jlimit (0, 127, msg.getNoteNumber());
            const int note = juce::jlimit (0, 127, rawNote + octaveShift);
            heldNoteVel[(size_t) rawNote] = 0;
            synth->noteOff (sample, note, msg.getVelocity());
        }
        else if (msg.isPitchWheel())
        {
            // User wheel — fold into the bend we send (combined with porta).
            userPitchBendValue = msg.getPitchWheelValue();
            emitPortamentoBend (sample, bendRangeSemis);
        }
        else if (msg.isController())
        {
            synth->cc (sample, msg.getControllerNumber(), msg.getControllerValue());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            heldNoteVel.fill (0);
            portaCurrentSemis     = 0.0f;
            portaSamplesRemaining = 0;
            portaLastNote         = -1;
            portaCurrentBendValue = -1; // force re-emit
            synth->allSoundOff();
            synth->pitchWheel (sample, 8192);
        }
    }

    // Ramp portamento to the end of the block.
    advancePortamentoTo (numSamples);

    midi.clear();

    float* outs[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    synth->renderBlock (outs, (size_t) numSamples, 1);

    applyMasterGain (buffer);
}

juce::AudioProcessorEditor* XSamplerAudioProcessor::createEditor()
{
    return new XSamplerAudioProcessorEditor (*this);
}

void XSamplerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Only persist the loaded SFZ path. All knob values reset to their
    // declared defaults on every session load — that's deliberate while
    // the engine is stabilising, so the user always starts from a known
    // audible state.
    juce::ValueTree tree ("XSamplerSession");
    if (currentSfzFile.existsAsFile())
        tree.setProperty ("sfzPath", currentSfzFile.getFullPathName(), nullptr);

    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void XSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;

        const auto path = tree.getProperty ("sfzPath").toString();
        if (path.isNotEmpty())
        {
            juce::File f (path);
            if (f.existsAsFile()) loadSfzFile (f);
        }
    }
}

#ifndef XSAMPLER_TESTING
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XSamplerAudioProcessor();
}
#endif
