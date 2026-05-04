#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SfzOverride.h"

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
    static constexpr auto outputWidth     = "output_width";
    static constexpr auto velToVolume     = "velocity_to_volume";
    static constexpr auto velToFilter     = "velocity_to_filter";

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
    const juce::StringArray& structuralParams()
    {
        static const juce::StringArray ids {
            ParamID::filterType,
            ParamID::lfoWaveform,
            ParamID::lfoEnabled,
            ParamID::voiceMode
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

    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::masterGain, 1 },      "Master Gain",       juce::NormalisableRange<float> (0.0f, 1.0f),    0.8f));
    layout.add (std::make_unique<IntParam>    (juce::ParameterID { ParamID::tuneGlobal, 1 },      "Tune Global",       -100, 100, 0));
    layout.add (std::make_unique<IntParam>    (juce::ParameterID { ParamID::pitchbendRange, 1 },  "Pitchbend Range",   1, 24, 12));
    layout.add (std::make_unique<IntParam>    (juce::ParameterID { ParamID::octaveTranspose, 1 }, "Octave Transpose",  -3, 3, 0));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::startOffset, 1 },     "Start Offset",      juce::NormalisableRange<float> (0.0f, 1.0f),    0.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::analogAmount, 1 },    "Analog Amount",     juce::NormalisableRange<float> (0.0f, 1.0f),    0.0f));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::doublerEnabled, 1 },  "Doubler",           false));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::voiceMode, 1 },       "Voice Mode",        juce::StringArray { "Poly", "Mono" }, 0));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::legatoEnabled, 1 },   "Legato",            false));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::portamentoTime, 1 },  "Portamento Time",   juce::NormalisableRange<float> (0.0f, 5.0f),    0.0f));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::fingeredPort, 1 },    "Fingered Portamento", false));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::filterType, 1 },      "Filter Type",       juce::StringArray { "LP", "HP", "BP" }, 0));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterCutoff, 1 },    "Filter Cutoff",     juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.3f), 8000.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterResonance, 1 }, "Filter Resonance",  juce::NormalisableRange<float> (0.0f, 1.0f),    0.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::volAttack, 1 },       "Vol Attack",        juce::NormalisableRange<float> (0.001f, 10.0f, 0.0f, 0.3f),  0.01f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::volDecay, 1 },        "Vol Decay",         juce::NormalisableRange<float> (0.001f, 10.0f, 0.0f, 0.3f),  0.1f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::volSustain, 1 },      "Vol Sustain",       juce::NormalisableRange<float> (0.0f, 1.0f),    0.8f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::volRelease, 1 },      "Vol Release",       juce::NormalisableRange<float> (0.001f, 20.0f, 0.0f, 0.3f), 0.3f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterAttack, 1 },    "Filter Attack",     juce::NormalisableRange<float> (0.001f, 10.0f, 0.0f, 0.3f),  0.01f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterDecay, 1 },     "Filter Decay",      juce::NormalisableRange<float> (0.001f, 10.0f, 0.0f, 0.3f),  0.1f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterSustain, 1 },   "Filter Sustain",    juce::NormalisableRange<float> (0.0f, 1.0f),    0.8f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::filterRelease, 1 },   "Filter Release",    juce::NormalisableRange<float> (0.001f, 20.0f, 0.0f, 0.3f), 0.3f));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::lfoEnabled, 1 },      "LFO Enabled",       false));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::lfoWaveform, 1 },     "LFO Waveform",      juce::StringArray { "Sine", "Tri", "Saw", "Sq", "Random" }, 0));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::lfoRate, 1 },         "LFO Rate",          juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.3f),  2.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::lfoDepth, 1 },        "LFO Depth",         juce::NormalisableRange<float> (0.0f, 1.0f),    0.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::lfoDelay, 1 },        "LFO Delay",         juce::NormalisableRange<float> (0.0f, 4.0f),    0.0f));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::lfoTarget, 1 },       "LFO Target",        juce::StringArray { "Pitch", "Filter", "Volume" }, 0));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::outputWidth, 1 },     "Output Width",      juce::NormalisableRange<float> (0.0f, 1.0f),    1.0f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::velToVolume, 1 },     "Velocity → Volume", juce::NormalisableRange<float> (0.0f, 1.0f),    0.8f));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::velToFilter, 1 },     "Velocity → Filter", juce::NormalisableRange<float> (0.0f, 1.0f),    0.0f));

    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::arpEnabled, 1 }, "Arp",        false));
    layout.add (std::make_unique<BoolParam>   (juce::ParameterID { ParamID::arpHold, 1 },    "Arp Hold",   false));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::arpMode, 1 },    "Arp Mode",   juce::StringArray { "Up", "Down", "UpDown", "Random", "AsPlayed" }, 0));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { ParamID::arpRate, 1 },    "Arp Rate",   juce::StringArray { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 3));
    layout.add (std::make_unique<IntParam>    (juce::ParameterID { ParamID::arpOctaves, 1 }, "Arp Octaves", 1, 4, 1));
    layout.add (std::make_unique<FloatParam>  (juce::ParameterID { ParamID::arpGate, 1 },    "Arp Gate",   juce::NormalisableRange<float> (0.05f, 1.0f), 0.5f));

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
    CACHE (pStartOffset,     startOffset);
    CACHE (pAnalogAmount,    analogAmount);
    CACHE (pDoublerEnabled,  doublerEnabled);
    CACHE (pVoiceMode,       voiceMode);
    CACHE (pLegatoEnabled,   legatoEnabled);
    CACHE (pPortamentoTime,  portamentoTime);
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
    CACHE (pLfoEnabled,      lfoEnabled);
    CACHE (pLfoWaveform,     lfoWaveform);
    CACHE (pLfoRate,         lfoRate);
    CACHE (pLfoDepth,        lfoDepth);
    CACHE (pLfoDelay,        lfoDelay);
    CACHE (pLfoTarget,       lfoTarget);
    CACHE (pOutputWidth,     outputWidth);
    CACHE (pVelToVolume,     velToVolume);
    CACHE (pVelToFilter,     velToFilter);
    CACHE (pArpEnabled,      arpEnabled);
    CACHE (pArpHold,         arpHold);
    CACHE (pArpMode,         arpMode);
    CACHE (pArpRate,         arpRate);
    CACHE (pArpOctaves,      arpOctaves);
    CACHE (pArpGate,         arpGate);
    #undef CACHE

    for (const auto& id : structuralParams())
        apvts.addParameterListener (id, this);

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

    // 30 ms ramps for both — short enough to track gestures, long enough to
    // hide step changes from automation.
    const double smoothSecs = 0.03;
    gainSmooth.reset  (sampleRate, smoothSecs);
    widthSmooth.reset (sampleRate, smoothSecs);
    gainSmooth.setCurrentAndTargetValue  (pMasterGain  != nullptr ? pMasterGain->load()  : 0.8f);
    widthSmooth.setCurrentAndTargetValue (pOutputWidth != nullptr ? pOutputWidth->load() : 1.0f);

    keyboardState.reset();
    arp.prepare (sampleRate);
    lastCC.fill (-1.0f);
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

void XSamplerAudioProcessor::parameterChanged (const juce::String&, float)
{
    overlayDirty.store (true, std::memory_order_release);
    lastChangeMs.store (juce::Time::getMillisecondCounter(), std::memory_order_release);
}

void XSamplerAudioProcessor::timerCallback()
{
    if (! overlayDirty.load (std::memory_order_acquire)) return;

    // Defer rebuilds until the engine is silent so a structural reload never
    // cuts an active voice. After 2 s of waiting, force the rebuild — by
    // then a brief click is preferable to never applying the change.
    const auto now   = juce::Time::getMillisecondCounter();
    const auto since = now - lastChangeMs.load (std::memory_order_acquire);
    if (since < 60) return;

    int activeVoices = 0;
    {
        const juce::ScopedLock sl (synthLock);
        activeVoices = synth->getNumActiveVoices();
    }
    const bool forced = since > 2000;
    if (activeVoices != 0 && ! forced) return;

    overlayDirty.store (false, std::memory_order_release);
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
    sp.mono       = pVoiceMode->load()   >= 0.5f;
    sp.filterType = (int) pFilterType->load();
    sp.lfoWave    = (int) pLfoWaveform->load();
    sp.lfoEnabled = pLfoEnabled->load() >= 0.5f;

    const juce::String combined = buildSfzWithOverride (currentSfzFile, sp);
    if (combined.isEmpty()) return;

    {
        const juce::ScopedLock sl (synthLock);
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

        // Push the current CC state into sfizz right now so any noteOn
        // that arrives before the next processBlock already sees correct
        // ADSR / cutoff / velocity values.
        lastCC.fill (-1.0f);
        flushParamCCs (true);
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

    // LFO 1: only send LFO CCs when LFO is actually declared in the
    // overlay — sending to undeclared opcodes still trips an internal
    // sfizz path that silences the synth in 1.2.3.
    const bool  lfoOn    = pLfoEnabled->load() >= 0.5f;
    if (lfoOn)
    {
        send (11, XSamplerCC::LfoRate,  pLfoRate->load()  * 0.05f);
        send (12, XSamplerCC::LfoDelay, pLfoDelay->load() * 0.25f);

        const float lfoDepth = pLfoDepth->load();
        const int   lfoTgt   = (int) pLfoTarget->load();
        send (13, XSamplerCC::LfoDepthPitch,  lfoTgt == 0 ? lfoDepth : 0.0f);
        send (14, XSamplerCC::LfoDepthCutoff, lfoTgt == 1 ? lfoDepth : 0.0f);
        send (15, XSamplerCC::LfoDepthVolume, lfoTgt == 2 ? lfoDepth : 0.0f);
    }

    // Velocity tracking & analog amount (CC-routed; live too).
    send (16, XSamplerCC::AmpVelTrack, pVelToVolume->load());
    send (17, XSamplerCC::FilVelTrack, pVelToFilter->load());
    send (18, XSamplerCC::PitchRandom, pAnalogAmount->load());
    send (19, XSamplerCC::DelayRandom, pAnalogAmount->load());
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

void XSamplerAudioProcessor::applyStereoWidth (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2) return;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        const float w    = widthSmooth.getNextValue();
        const float gain = gainSmooth.getNextValue();
        const float mid  = 0.5f * (L[i] + R[i]);
        const float side = 0.5f * (L[i] - R[i]) * w;
        L[i] = (mid + side) * gain;
        R[i] = (mid - side) * gain;
    }
}

void XSamplerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Tempo for arp.
    double bpm = 120.0;
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

    // Update smoothing targets.
    gainSmooth.setTargetValue  (pMasterGain->load());
    widthSmooth.setTargetValue (pOutputWidth->load());

    const juce::ScopedLock sl (synthLock);

    flushParamCCs (false);

    const int   octaveShift    = (int) pOctaveTranspose->load() * 12;
    const float bendRangeSemis = pPitchbendRange->load();

    for (const auto meta : arpedMidi)
    {
        const auto msg    = meta.getMessage();
        const int  sample = meta.samplePosition;

        if (msg.isNoteOn())
        {
            const int note = juce::jlimit (0, 127, msg.getNoteNumber() + octaveShift);
            synth->noteOn (sample, note, msg.getVelocity());
        }
        else if (msg.isNoteOff())
        {
            const int note = juce::jlimit (0, 127, msg.getNoteNumber() + octaveShift);
            synth->noteOff (sample, note, msg.getVelocity());
        }
        else if (msg.isPitchWheel())
        {
            const int raw   = msg.getPitchWheelValue();
            const int delta = raw - 8192;
            const float scale = bendRangeSemis * (1.0f / 12.0f);
            int scaled = 8192 + (int) (delta * scale);
            scaled = juce::jlimit (0, 16383, scaled);
            synth->pitchWheel (sample, scaled);
        }
        else if (msg.isController())
        {
            // Forward host CCs; reserve our own slots (102..127) for internal
            // use only — those are never echoed in here because the user
            // can't send them via the keyboard component.
            synth->cc (sample, msg.getControllerNumber(), msg.getControllerValue());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            synth->allSoundOff();
        }
    }

    midi.clear();

    float* outs[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    synth->renderBlock (outs, (size_t) numSamples, 1);

    applyStereoWidth (buffer);
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
