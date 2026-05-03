#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    return layout;
}

XSamplerAudioProcessor::XSamplerAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "XSamplerState", createLayout())
{
    synth = std::make_unique<sfz::Sfizz>();

    pMasterGain        = apvts.getRawParameterValue (ParamID::masterGain);
    pTuneGlobal        = apvts.getRawParameterValue (ParamID::tuneGlobal);
    pPitchbendRange    = apvts.getRawParameterValue (ParamID::pitchbendRange);
    pOctaveTranspose   = apvts.getRawParameterValue (ParamID::octaveTranspose);
    pStartOffset       = apvts.getRawParameterValue (ParamID::startOffset);
    pAnalogAmount      = apvts.getRawParameterValue (ParamID::analogAmount);
    pDoublerEnabled    = apvts.getRawParameterValue (ParamID::doublerEnabled);
    pVoiceMode         = apvts.getRawParameterValue (ParamID::voiceMode);
    pLegatoEnabled     = apvts.getRawParameterValue (ParamID::legatoEnabled);
    pPortamentoTime    = apvts.getRawParameterValue (ParamID::portamentoTime);
    pFingeredPort      = apvts.getRawParameterValue (ParamID::fingeredPort);
    pFilterType        = apvts.getRawParameterValue (ParamID::filterType);
    pFilterCutoff      = apvts.getRawParameterValue (ParamID::filterCutoff);
    pFilterResonance   = apvts.getRawParameterValue (ParamID::filterResonance);
    pVolAttack         = apvts.getRawParameterValue (ParamID::volAttack);
    pVolDecay          = apvts.getRawParameterValue (ParamID::volDecay);
    pVolSustain        = apvts.getRawParameterValue (ParamID::volSustain);
    pVolRelease        = apvts.getRawParameterValue (ParamID::volRelease);
    pFilterAttack      = apvts.getRawParameterValue (ParamID::filterAttack);
    pFilterDecay       = apvts.getRawParameterValue (ParamID::filterDecay);
    pFilterSustain     = apvts.getRawParameterValue (ParamID::filterSustain);
    pFilterRelease     = apvts.getRawParameterValue (ParamID::filterRelease);
    pLfoEnabled        = apvts.getRawParameterValue (ParamID::lfoEnabled);
    pLfoWaveform       = apvts.getRawParameterValue (ParamID::lfoWaveform);
    pLfoRate           = apvts.getRawParameterValue (ParamID::lfoRate);
    pLfoDepth          = apvts.getRawParameterValue (ParamID::lfoDepth);
    pLfoDelay          = apvts.getRawParameterValue (ParamID::lfoDelay);
    pLfoTarget         = apvts.getRawParameterValue (ParamID::lfoTarget);
    pOutputWidth       = apvts.getRawParameterValue (ParamID::outputWidth);
    pVelToVolume       = apvts.getRawParameterValue (ParamID::velToVolume);
    pVelToFilter       = apvts.getRawParameterValue (ParamID::velToFilter);

    // Reserve a sane default voice count; will be reconfigured in prepareToPlay.
    synth->setNumVoices (32);
}

XSamplerAudioProcessor::~XSamplerAudioProcessor() = default;

void XSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    const juce::ScopedLock sl (synthLock);
    synth->setSampleRate (static_cast<float> (sampleRate));
    synth->setSamplesPerBlock (samplesPerBlock);
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

    const juce::ScopedLock sl (synthLock);
    const bool ok = synth->loadSfzFile (file.getFullPathName().toStdString());
    if (ok)
    {
        currentSfzFile = file;
        sfzLoaded.store (true, std::memory_order_release);
        apvts.state.setProperty ("sfzPath", file.getFullPathName(), nullptr);
    }
    return ok;
}

juce::File XSamplerAudioProcessor::getCurrentSfzFile() const
{
    return currentSfzFile;
}

void XSamplerAudioProcessor::applyParametersToSfizz()
{
    // Voice count: 1 if mono, else 32. (sfizz handles mono via SFZ opcodes; we
    // approximate by limiting voice count when mono is requested.)
    const bool mono = pVoiceMode->load() >= 0.5f;
    synth->setNumVoices (mono ? 1 : 32);

    // TODO: pitchbend_range — sfizz reads bend_up/bend_down from the SFZ file.
    //       Apply via a runtime SFZ override or scale pitchwheel ourselves.
    // TODO: tune_global cents — sfizz has no direct global cents API; could be
    //       applied by adjusting tuning frequency relative to A4.
    // TODO: start_offset, analog_amount, doubler, legato, portamento, filter,
    //       envelopes, LFO, velocity curves — all need runtime SFZ overrides
    //       or generated <region> wrappers around the loaded instrument.
}

void XSamplerAudioProcessor::applyStereoWidth (juce::AudioBuffer<float>& buffer, float width)
{
    if (buffer.getNumChannels() < 2)
        return;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    const int n = buffer.getNumSamples();
    const float w = juce::jlimit (0.0f, 1.0f, width);

    for (int i = 0; i < n; ++i)
    {
        const float mid  = 0.5f * (L[i] + R[i]);
        const float side = 0.5f * (L[i] - R[i]) * w;
        L[i] = mid + side;
        R[i] = mid - side;
    }
}

void XSamplerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    buffer.clear();

    if (! sfzLoaded.load (std::memory_order_acquire))
    {
        midi.clear();
        return;
    }

    const juce::ScopedLock sl (synthLock);

    applyParametersToSfizz();

    const int octaveShift = static_cast<int> (pOctaveTranspose->load()) * 12;
    const float bendRangeSemis = pPitchbendRange->load();

    for (const auto meta : midi)
    {
        const auto msg    = meta.getMessage();
        const int  sample = meta.samplePosition;

        if (msg.isNoteOn())
        {
            int note = juce::jlimit (0, 127, msg.getNoteNumber() + octaveShift);
            synth->noteOn (sample, note, msg.getVelocity());
        }
        else if (msg.isNoteOff())
        {
            int note = juce::jlimit (0, 127, msg.getNoteNumber() + octaveShift);
            synth->noteOff (sample, note, msg.getVelocity());
        }
        else if (msg.isPitchWheel())
        {
            // JUCE pitchwheel is 0..16383, centred at 8192. sfizz expects the
            // same 14-bit range. Scale by ratio of requested bend range to
            // SFZ default (12 semis) so the user's pitchbend_range maps
            // through correctly even when the SFZ file uses defaults.
            const int raw   = msg.getPitchWheelValue(); // 0..16383
            const int delta = raw - 8192;               // -8192..+8191
            const float scale = bendRangeSemis / 12.0f;
            int scaled = 8192 + static_cast<int> (delta * scale);
            scaled = juce::jlimit (0, 16383, scaled);
            synth->pitchWheel (sample, scaled);
        }
        else if (msg.isController())
        {
            synth->cc (sample, msg.getControllerNumber(), msg.getControllerValue());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            synth->allSoundOff();
        }
    }

    midi.clear();

    float* outs[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    synth->renderBlock (outs, static_cast<size_t> (numSamples), 2);

    const float gain = pMasterGain->load();
    buffer.applyGain (gain);

    applyStereoWidth (buffer, pOutputWidth->load());
}

juce::AudioProcessorEditor* XSamplerAudioProcessor::createEditor()
{
    return new XSamplerAudioProcessorEditor (*this);
}

void XSamplerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (currentSfzFile.existsAsFile())
        state.setProperty ("sfzPath", currentSfzFile.getFullPathName(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void XSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid())
        {
            apvts.replaceState (tree);
            const auto path = tree.getProperty ("sfzPath").toString();
            if (path.isNotEmpty())
            {
                juce::File f (path);
                if (f.existsAsFile())
                    loadSfzFile (f);
            }
        }
    }
}

#ifndef XSAMPLER_TESTING
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XSamplerAudioProcessor();
}
#endif
