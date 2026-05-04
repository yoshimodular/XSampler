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
            case 1: return 4800;
            case 2: return 24;
            default: return 1200;
        }
    }

    // Opcodes we ALWAYS strip from the user's source — they're mapped to
    // a macro knob and stripping is essential so per-region values don't
    // shadow our <global> CC routing.
    //
    // We deliberately DO NOT strip these (they're often authored
    // intentionally and we don't necessarily replace them):
    //   * `trigger`  — only stripped when our own legato wraps regions
    //   * `offset`   — user's deliberate offsets stay; our CC adds on top
    //   * `pan`      — only stripped when our doubler wraps regions
    //   * `tune`     — kept; users rarely set per-region tune, and we
    //                   apply ours via tune_oncc on top of any existing.
    const juce::StringArray& alwaysStripped()
    {
        static const juce::StringArray names {
            "cutoff", "resonance", "fil_type",
            "ampeg_attack", "ampeg_decay", "ampeg_sustain", "ampeg_release",
            "fileg_attack", "fileg_decay", "fileg_sustain", "fileg_release", "fileg_depth",
            "amp_veltrack", "fil_veltrack",
            "polyphony",
            "pitch_random", "delay_random",
            "bend_up", "bend_down"
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

    // Index of the first SFZ region/group/master/global header (NOT control).
    // Our overlay's <global> block is prepended here so any `<control>` that
    // the user authored at the top of the file stays in the preamble where
    // sfizz expects it.
    int firstNonControlHeader (const juce::String& text)
    {
        int best = -1;
        for (auto h : { "<region>", "<group>", "<master>", "<global>" })
        {
            int idx = text.indexOf (h);
            if (idx >= 0 && (best < 0 || idx < best)) best = idx;
        }
        return best;
    }

    int firstRegionHeader (const juce::String& text)
    {
        int best = -1;
        for (auto h : { "<region>", "<group>", "<master>" })
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

    for (const auto& op : alwaysStripped())
        original = stripOpcode (original, op);
    original = stripLfoOpcodes (original);

    // Conditional strips
    if (p.legato && p.mono)
        original = stripOpcode (original, "trigger");
    if (p.doubler)
        original = stripOpcode (original, "pan");

    // Build our <global> block.
    juce::String g;
    g << "// XSampler runtime overlay — generated, do not edit\n";
    g << "<global>\n";

    if (p.mono)
        g << "polyphony=1\n";

    // Internal pitch-bend range (24 semis fixed). The processor maps the
    // user's pitchbend_range onto this.
    g << "bend_up=2400\nbend_down=-2400\n";

    // Tune: declared as additive only — no base value, so any per-region
    // user tune wins. CC110 sends 0..1 mapped to -100..+100 c.
    g << "tune_oncc"     << XSamplerCC::Tune       << "=200\n";
    g << "tune_curvecc"  << XSamplerCC::Tune       << "=2\n";  // bipolar curve

    // Filter
    g << "fil_type="     << sfzFilterType (p.filterType) << "\n";
    g << "cutoff=20\n"
      << "cutoff_oncc"   << XSamplerCC::Cutoff     << "=12000\n"
      << "resonance=0\n"
      << "resonance_oncc"<< XSamplerCC::Resonance  << "=24\n";

    // Volume ADSR
    g << "ampeg_attack=0\n"   << "ampeg_attack_oncc"  << XSamplerCC::VolAttack  << "=10\n"
      << "ampeg_decay=0\n"    << "ampeg_decay_oncc"   << XSamplerCC::VolDecay   << "=10\n"
      << "ampeg_sustain=0\n"  << "ampeg_sustain_oncc" << XSamplerCC::VolSustain << "=100\n"
      << "ampeg_release=0\n"  << "ampeg_release_oncc" << XSamplerCC::VolRelease << "=20\n";

    // Filter ADSR
    g << "fileg_attack=0\n"   << "fileg_attack_oncc"  << XSamplerCC::FltAttack  << "=10\n"
      << "fileg_decay=0\n"    << "fileg_decay_oncc"   << XSamplerCC::FltDecay   << "=10\n"
      << "fileg_sustain=0\n"  << "fileg_sustain_oncc" << XSamplerCC::FltSustain << "=100\n"
      << "fileg_release=0\n"  << "fileg_release_oncc" << XSamplerCC::FltRelease << "=20\n"
      << "fileg_depth=-4800\n"
      << "fileg_depth_oncc"   << XSamplerCC::FltEnvAmount << "=9600\n";

    // LFO
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

    // Velocity tracking (additive — user's region values still win as the
    // base; our CC modulation rides on top).
    g << "amp_veltrack_oncc" << XSamplerCC::AmpVelTrack << "=100\n"
      << "fil_veltrack_oncc" << XSamplerCC::FilVelTrack << "=4800\n";

    // Analog (random pitch + delay) — additive on top of any user values.
    g << "pitch_random_oncc" << XSamplerCC::PitchRandom << "=10\n"
      << "delay_random_oncc" << XSamplerCC::DelayRandom << "=0.003\n";

    // Sample start — additive on top of user offsets.
    g << "offset_oncc" << XSamplerCC::SampleStart << "=4410\n";

    g << "\n";

    // -------- Splice the overlay into the right place ------------------
    // If the user's file has a `<control>` block, our <global> goes AFTER
    // it (control must come before global per SFZ spec). Otherwise it
    // goes at the top.
    const int hdrPos = firstNonControlHeader (original);

    juce::String preamble, body;
    if (hdrPos >= 0)
    {
        preamble = original.substring (0, hdrPos);
        body     = original.substring (hdrPos);
    }
    else
    {
        preamble = original;
        body     = juce::String();
    }

    juce::String out;

    // Doubler / legato need the region body duplicated. For simple modes
    // we just stitch [preamble] + [our overlay] + [body].
    if (p.legato && p.mono)
    {
        const int rPos = firstRegionHeader (body);
        if (rPos < 0)
        {
            out << preamble << g << "trigger=legato\n" << body;
        }
        else
        {
            const juce::String beforeRegions = body.substring (0, rPos);
            const juce::String regionBlock   = body.substring (rPos);
            out << preamble << g << beforeRegions
                << "<group>\ntrigger=first\n"  << regionBlock << "\n"
                << "<group>\ntrigger=legato\n" << regionBlock << "\n";
        }
    }
    else if (p.doubler)
    {
        const int rPos = firstRegionHeader (body);
        if (rPos < 0)
        {
            out << preamble << g << body;
        }
        else
        {
            const juce::String beforeRegions = body.substring (0, rPos);
            const juce::String regionBlock   = body.substring (rPos);
            out << preamble << g << beforeRegions
                << "<group>\npan=-100\ntune_oncc"  << XSamplerCC::PitchRandom << "=-25\n"
                << regionBlock << "\n"
                << "<group>\npan=100\ntune_oncc"   << XSamplerCC::PitchRandom << "=25\n"
                << "delay_oncc"                    << XSamplerCC::DelayRandom << "=0.005\n"
                << regionBlock << "\n";
        }
    }
    else
    {
        out << preamble << g << body;
    }

    return out;
}
