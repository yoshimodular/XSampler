#pragma once

#include <juce_core/juce_core.h>

// Only the truly structural parameters require an overlay rebuild. Everything
// else is driven by CC modulation defined inside the overlay and updated in
// real time via sfz::Sfizz::hdcc().
struct XSamplerSfzParams
{
    bool mono       { false };
    int  filterType { 0 };  // 0=LP, 1=HP, 2=BP
    int  lfoWave    { 0 };  // 0..4
    bool lfoEnabled { false };
};

// CC numbers used to drive sfizz's parameter modulation in real time.
// The overlay declares one `_oncc{N}` slot per parameter; the processor
// sends `hdcc(N, value)` whenever the value changes. No engine reload
// needed for these — changes are instant and seamless.
namespace XSamplerCC
{
    // Pitch / filter
    constexpr int Tune        = 110;  // -100..+100 cents
    constexpr int Cutoff      = 111;  // 20..20480 Hz (log)
    constexpr int Resonance   = 112;  // 0..24 dB

    // Vol ADSR
    constexpr int VolAttack   = 113;
    constexpr int VolDecay    = 114;
    constexpr int VolSustain  = 115;
    constexpr int VolRelease  = 116;

    // Filter ADSR
    constexpr int FltAttack   = 117;
    constexpr int FltDecay    = 118;
    constexpr int FltSustain  = 119;
    constexpr int FltRelease  = 120;

    // LFO 1 — single LFO declared with all 3 targets always wired up.
    // The processor sends the user-set depth on exactly one of the three
    // depth CCs (per `lfo_target`) and zero on the other two, so changing
    // target / enabled / depth is instant.
    constexpr int LfoRate         = 121;  // 0..20 Hz
    constexpr int LfoDepthPitch   = 122;  // 0..1200 cents
    constexpr int LfoDelay        = 123;  // 0..4 s
    constexpr int LfoDepthCutoff  = 124;  // 0..4800 cents
    constexpr int LfoDepthVolume  = 125;  // 0..24 dB

    // Velocity tracking
    constexpr int AmpVelTrack     = 126;  // 0..100 %
    constexpr int FilVelTrack     = 127;  // 0..4800 cents

    // Analog amount (random pitch + delay)
    constexpr int PitchRandom     = 102;  // 0..25 cents
    constexpr int DelayRandom     = 103;  // 0..5 ms
}

juce::String buildSfzWithOverride (const juce::File& originalSfz,
                                   const XSamplerSfzParams& p);
