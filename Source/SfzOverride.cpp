#include "SfzOverride.h"
#include <regex>

namespace
{
    juce::String fmt (float v, int decimals = 4)
    {
        return juce::String (v, decimals);
    }

    const char* sfzFilterType (int t)
    {
        switch (t)
        {
            case 0: return "lpf_2p";
            case 1: return "hpf_2p";
            case 2: return "bpf_2p";
            default: return "lpf_2p";
        }
    }

    // sfizz LFO wave indices (sfizz docs):
    // 0=triangle, 1=sine, 2=pulse75, 3=square, 4=pulse25, 5=pulse12.5,
    // 6=ramp, 7=saw, 8=stepped (random-ish).
    int sfzLfoWave (int idx)
    {
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
}

namespace
{
    // SFZ opcodes we control globally — strip them from the original source
    // so per-region values can't override our <global>. Suffixes like
    // _cc7 / _oncc7 / _curvecc7 / _smoothcc7 are also stripped.
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

    // Remove `name[=...]` tokens (and common suffix variants) from text.
    juce::String stripOpcode (const juce::String& text, const juce::String& name)
    {
        // Token boundary: not letter/digit/underscore. Match name and any
        // _xxx suffix until '='.
        const std::regex re (
            "(^|[\\s\\t])" + name.toStdString() + "(_[a-z0-9_]+)?=\\S+",
            std::regex_constants::icase);
        return juce::String (std::regex_replace (text.toStdString(), re, "$1"));
    }

    juce::String stripLfoOpcodes (const juce::String& text)
    {
        // Strip lfoNN_* opcodes entirely (LFO 01..32 typical).
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
    g << "// XSampler runtime override — generated, do not edit\n";
    g << "<global>\n";

    if (p.tuneCents != 0)
        g << "tune=" << p.tuneCents << "\n";

    if (p.mono)
        g << "polyphony=1\n";

    // Filter ----------------------------------------------------------------
    g << "fil_type=" << sfzFilterType (p.filterType) << "\n";
    g << "cutoff=" << fmt (p.filterCutoff, 2) << "\n";
    // Resonance: 0..1 → 0..24 dB.
    g << "resonance=" << fmt (p.filterResonance * 24.0f, 2) << "\n";

    // Amp envelope ---------------------------------------------------------
    g << "ampeg_attack="  << fmt (p.volA) << "\n";
    g << "ampeg_decay="   << fmt (p.volD) << "\n";
    g << "ampeg_sustain=" << fmt (p.volS * 100.0f, 2) << "\n"; // %
    g << "ampeg_release=" << fmt (p.volR) << "\n";

    // Filter envelope ------------------------------------------------------
    g << "fileg_attack="  << fmt (p.fltA) << "\n";
    g << "fileg_decay="   << fmt (p.fltD) << "\n";
    g << "fileg_sustain=" << fmt (p.fltS * 100.0f, 2) << "\n"; // %
    g << "fileg_release=" << fmt (p.fltR) << "\n";
    // Default depth: 2 octaves. Without a depth knob, this gives the env
    // a useful range. TODO: expose as user parameter.
    g << "fileg_depth=2400\n";

    // LFO 1 ----------------------------------------------------------------
    if (p.lfoEnabled && p.lfoDepth > 0.0f)
    {
        g << "lfo01_freq="  << fmt (p.lfoRate, 4) << "\n";
        g << "lfo01_wave="  << sfzLfoWave (p.lfoWave) << "\n";
        g << "lfo01_delay=" << fmt (p.lfoDelay, 4) << "\n";

        switch (p.lfoTarget)
        {
            case 0: // Pitch — depth in cents (0..1200)
                g << "lfo01_pitch=" << fmt (p.lfoDepth * 1200.0f, 2) << "\n";
                break;
            case 1: // Filter cutoff — depth in cents (0..4800)
                g << "lfo01_cutoff=" << fmt (p.lfoDepth * 4800.0f, 2) << "\n";
                break;
            case 2: // Volume — depth in dB (0..24)
                g << "lfo01_volume=" << fmt (p.lfoDepth * 24.0f, 2) << "\n";
                break;
            default: break;
        }
    }

    // Velocity tracking ----------------------------------------------------
    // amp_veltrack: 0..100 (% of velocity affecting amplitude)
    g << "amp_veltrack=" << fmt (p.velToVolume * 100.0f, 2) << "\n";
    // fil_veltrack in cents — 0..4800
    g << "fil_veltrack=" << fmt (p.velToFilter * 4800.0f, 2) << "\n";

    // Analog amount → tune + sample-start randomisation
    if (p.analogAmount > 0.0f)
    {
        g << "pitch_random=" << fmt (p.analogAmount * 25.0f, 2) << "\n"; // cents
        g << "delay_random=" << fmt (p.analogAmount * 0.005f, 5) << "\n"; // s
    }

    g << "\n";
    g << original;
    return g;
}
