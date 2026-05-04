#include "SfzOverride.h"
#include <regex>

namespace
{
    const char* sfzFilterType (int t)
    {
        switch (t)
        {
            case 1: return "hpf_2p";
            case 2: return "bpf_2p";
            default: return "lpf_2p";
        }
    }

    int sfzLfoWave (int idx)
    {
        // sfizz LFO wave: 0=tri, 1=sine, 2=pulse75, 3=square, 7=saw, 8=stepped
        switch (idx)
        {
            case 0: return 1; // Sine
            case 1: return 0; // Tri
            case 2: return 7; // Saw
            case 3: return 3; // Square
            case 4: return 8; // Stepped (random)
            default: return 1;
        }
    }

    const juce::StringArray& strippedOpcodes()
    {
        static const juce::StringArray names {
            "cutoff", "resonance", "fil_type",
            "ampeg_attack", "ampeg_decay", "ampeg_sustain", "ampeg_release",
            "fileg_attack", "fileg_decay", "fileg_sustain", "fileg_release", "fileg_depth",
            "amp_veltrack", "fil_veltrack",
            "tune", "polyphony",
            "pitch_random", "delay_random"
        };
        return names;
    }

    juce::String stripOpcode (const juce::String& text, const juce::String& name)
    {
        const std::regex re (
            "(^|[\\s\\t])" + name.toStdString() + "(_[a-z0-9_]+)?=\\S+",
            std::regex_constants::icase);
        return juce::String (std::regex_replace (text.toStdString(), re, "$1"));
    }

    juce::String stripLfoOpcodes (const juce::String& text)
    {
        const std::regex re ("(^|[\\s\\t])lfo[0-9]+_[a-z0-9_]*=\\S+",
                             std::regex_constants::icase);
        return juce::String (std::regex_replace (text.toStdString(), re, "$1"));
    }
}

juce::String buildSfzWithOverride (const juce::File& originalSfz,
                                   const XSamplerSfzParams& p)
{
    if (! originalSfz.existsAsFile())
        return {};

    juce::String original = originalSfz.loadFileAsString();
    for (const auto& op : strippedOpcodes())
        original = stripOpcode (original, op);
    original = stripLfoOpcodes (original);

    juce::String g;
    g << "// XSampler runtime overlay — generated, do not edit\n";
    g << "<global>\n";

    if (p.mono)
        g << "polyphony=1\n";

    // -------- Tune (CC110: -100..+100 c) -------------------------------
    g << "tune=-100\n";
    g << "tune_oncc"     << XSamplerCC::Tune       << "=200\n";

    // -------- Filter ---------------------------------------------------
    g << "fil_type="     << sfzFilterType (p.filterType) << "\n";
    g << "cutoff=20\n";
    g << "cutoff_oncc"   << XSamplerCC::Cutoff     << "=12000\n";  // ~10 oct
    g << "resonance=0\n";
    g << "resonance_oncc"<< XSamplerCC::Resonance  << "=24\n";

    // -------- Volume ADSR ---------------------------------------------
    g << "ampeg_attack=0\n"   << "ampeg_attack_oncc"  << XSamplerCC::VolAttack  << "=10\n";
    g << "ampeg_decay=0\n"    << "ampeg_decay_oncc"   << XSamplerCC::VolDecay   << "=10\n";
    g << "ampeg_sustain=0\n"  << "ampeg_sustain_oncc" << XSamplerCC::VolSustain << "=100\n";
    g << "ampeg_release=0\n"  << "ampeg_release_oncc" << XSamplerCC::VolRelease << "=20\n";

    // -------- Filter ADSR ---------------------------------------------
    g << "fileg_attack=0\n"   << "fileg_attack_oncc"  << XSamplerCC::FltAttack  << "=10\n";
    g << "fileg_decay=0\n"    << "fileg_decay_oncc"   << XSamplerCC::FltDecay   << "=10\n";
    g << "fileg_sustain=0\n"  << "fileg_sustain_oncc" << XSamplerCC::FltSustain << "=100\n";
    g << "fileg_release=0\n"  << "fileg_release_oncc" << XSamplerCC::FltRelease << "=20\n";
    g << "fileg_depth=2400\n";

    // -------- LFO 1 — only declared when LFO is enabled. Even an
    // "inactive" LFO declaration silences sfizz output in 1.2.3. We pay
    // an overlay rebuild on lfo_enabled toggles (deferred until silent).
    if (p.lfoEnabled)
    {
        g << "lfo01_freq=0\n";
        g << "lfo01_freq_oncc"  << XSamplerCC::LfoRate        << "=20\n";
        g << "lfo01_delay=0\n";
        g << "lfo01_delay_oncc" << XSamplerCC::LfoDelay       << "=4\n";
        g << "lfo01_wave="      << sfzLfoWave (p.lfoWave)     << "\n";
        g << "lfo01_pitch_oncc" << XSamplerCC::LfoDepthPitch  << "=1200\n";
        g << "lfo01_cutoff_oncc"<< XSamplerCC::LfoDepthCutoff << "=4800\n";
        g << "lfo01_volume_oncc"<< XSamplerCC::LfoDepthVolume << "=24\n";
    }

    // -------- Velocity tracking ---------------------------------------
    g << "amp_veltrack=0\n";
    g << "amp_veltrack_oncc" << XSamplerCC::AmpVelTrack << "=100\n";
    g << "fil_veltrack=0\n";
    g << "fil_veltrack_oncc" << XSamplerCC::FilVelTrack << "=4800\n";

    // -------- Analog amount (random pitch + delay) --------------------
    g << "pitch_random=0\n";
    g << "pitch_random_oncc" << XSamplerCC::PitchRandom << "=25\n";
    g << "delay_random=0\n";
    g << "delay_random_oncc" << XSamplerCC::DelayRandom << "=0.005\n";

    g << "\n";
    g << original;
    return g;
}
