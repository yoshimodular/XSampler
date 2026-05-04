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
        switch (idx)
        {
            case 0: return 1; // Sine
            case 1: return 0; // Tri
            case 2: return 7; // Saw
            case 3: return 3; // Square
            case 4: return 8; // Stepped
            default: return 1;
        }
    }

    const char* lfoTargetOpcode (int t)
    {
        switch (t)
        {
            case 1: return "lfo01_cutoff_oncc";
            case 2: return "lfo01_volume_oncc";
            default: return "lfo01_pitch_oncc";
        }
    }

    int lfoTargetMaxDepth (int t)
    {
        switch (t)
        {
            case 1: return 4800; // cents on cutoff
            case 2: return 24;   // dB on volume
            default: return 1200; // cents on pitch
        }
    }

    const juce::StringArray& strippedOpcodes()
    {
        static const juce::StringArray names {
            "cutoff", "resonance", "fil_type",
            "ampeg_attack", "ampeg_decay", "ampeg_sustain", "ampeg_release",
            "fileg_attack", "fileg_decay", "fileg_sustain", "fileg_release", "fileg_depth",
            "amp_veltrack", "fil_veltrack",
            "tune", "polyphony", "trigger",
            "pitch_random", "delay_random",
            "offset", "pan"
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

    // Returns the offset of the first SFZ section header (`<region>`,
    // `<group>`, `<master>`, `<control>`) in `text`, or -1 if none.
    int firstSectionHeaderOffset (const juce::String& text)
    {
        int best = -1;
        for (auto h : { "<region>", "<group>", "<master>", "<control>" })
        {
            int idx = text.indexOf (h);
            if (idx >= 0 && (best < 0 || idx < best)) best = idx;
        }
        return best;
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

    // Tune (CC110: -100..+100 c)
    g << "tune=-100\n"
      << "tune_oncc"     << XSamplerCC::Tune       << "=200\n";

    // Filter
    g << "fil_type="     << sfzFilterType (p.filterType) << "\n"
      << "cutoff=20\n"
      << "cutoff_oncc"   << XSamplerCC::Cutoff     << "=12000\n"
      << "resonance=0\n"
      << "resonance_oncc"<< XSamplerCC::Resonance  << "=24\n";

    // Volume ADSR
    g << "ampeg_attack=0\n"   << "ampeg_attack_oncc"  << XSamplerCC::VolAttack  << "=10\n"
      << "ampeg_decay=0\n"    << "ampeg_decay_oncc"   << XSamplerCC::VolDecay   << "=10\n"
      << "ampeg_sustain=0\n"  << "ampeg_sustain_oncc" << XSamplerCC::VolSustain << "=100\n"
      << "ampeg_release=0\n"  << "ampeg_release_oncc" << XSamplerCC::VolRelease << "=20\n";

    // Filter ADSR + bipolar env amount via CC104.
    // Base depth = -4800 c, CC104 modulates ±9600 c so 0.5 = no modulation,
    // 0.0 = -4800 c (env darkens), 1.0 = +4800 c (env brightens).
    g << "fileg_attack=0\n"   << "fileg_attack_oncc"  << XSamplerCC::FltAttack  << "=10\n"
      << "fileg_decay=0\n"    << "fileg_decay_oncc"   << XSamplerCC::FltDecay   << "=10\n"
      << "fileg_sustain=0\n"  << "fileg_sustain_oncc" << XSamplerCC::FltSustain << "=100\n"
      << "fileg_release=0\n"  << "fileg_release_oncc" << XSamplerCC::FltRelease << "=20\n"
      << "fileg_depth=-4800\n"
      << "fileg_depth_oncc"   << XSamplerCC::FltEnvAmount << "=9600\n";

    // LFO 1 — only declared when depth > 0 (sfizz silences output if LFO
    // is declared with all-zero modulation depths).
    if (p.lfoActive)
    {
        g << "lfo01_freq=0\n"
          << "lfo01_freq_oncc"  << XSamplerCC::LfoRate << "=20\n"
          << "lfo01_delay=0\n"
          << "lfo01_delay_oncc" << XSamplerCC::LfoDelay << "=4\n"
          << "lfo01_wave="      << sfzLfoWave (p.lfoWave) << "\n"
          << lfoTargetOpcode (p.lfoTarget)
          << XSamplerCC::LfoDepth << "="
          << lfoTargetMaxDepth (p.lfoTarget) << "\n";
    }

    // Velocity tracking
    g << "amp_veltrack=0\n" << "amp_veltrack_oncc" << XSamplerCC::AmpVelTrack << "=100\n"
      << "fil_veltrack=0\n" << "fil_veltrack_oncc" << XSamplerCC::FilVelTrack << "=4800\n";

    // Analog: subtle pitch + delay randomisation per voice. Maxes are
    // tuned for "old analog synth" behaviour, not glitch.
    g << "pitch_random=0\n" << "pitch_random_oncc" << XSamplerCC::PitchRandom << "=10\n"
      << "delay_random=0\n" << "delay_random_oncc" << XSamplerCC::DelayRandom << "=0.003\n";

    // Sample start (offset in samples). 88200 ≈ 2 s at 44.1k — long enough
    // to be useful for short percussive samples without aliasing for long
    // sustained ones.
    g << "offset=0\n"
      << "offset_oncc" << XSamplerCC::SampleStart << "=88200\n";

    // Legato (per-region trigger). With polyphony=1 + trigger=legato a held
    // sequence shares envelopes; without legato every note retriggers.
    if (p.legato && p.mono)
        g << "trigger=legato\n";

    g << "\n";

    // -------- Region body ----------------------------------------------
    if (! p.doubler)
    {
        g << original;
    }
    else
    {
        // Doubler mode: split the original at the first section header and
        // emit two copies with hard-panned <group> wrappers and opposite
        // detune offsets via CC100. The user's analog-amount knob drives
        // CC100 — we re-purpose pitch_random_oncc as the detune knob, but
        // the per-group `tune_oncc100` adds a deterministic ±detune so the
        // two voices diverge predictably. Result: classic L/R doubling
        // without per-voice plumbing.
        const int splitAt = firstSectionHeaderOffset (original);
        if (splitAt < 0)
        {
            g << original;
        }
        else
        {
            const juce::String preamble = original.substring (0, splitAt);
            const juce::String body     = original.substring (splitAt);

            g << preamble;
            g << "<group>\n"
              << "pan=-100\n"
              << "tune_oncc" << XSamplerCC::PitchRandom << "=-25\n"
              << body << "\n";
            g << "<group>\n"
              << "pan=100\n"
              << "tune_oncc" << XSamplerCC::PitchRandom << "=25\n"
              << "delay_oncc" << XSamplerCC::DelayRandom << "=0.005\n"
              << body << "\n";
        }
    }

    return g;
}
