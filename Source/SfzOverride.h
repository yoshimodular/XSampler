#pragma once

#include <juce_core/juce_core.h>

// Structural parameters that require an overlay rebuild. Smooth knobs
// (cutoff, resonance, ADSR, LFO rate/depth/delay, vel-tracks, analog,
// sample start, filter env amount) are CC-driven and update in real
// time without a reload.
struct XSamplerSfzParams
{
    bool mono       { false };
    bool legato     { false };
    bool doubler    { false };
    bool lfoActive  { false };  // set true when lfo_depth > 0
    int  filterType { 0 };      // 0=LP, 1=HP, 2=BP
    int  lfoWave    { 0 };      // 0..4
    int  lfoTarget  { 0 };      // 0=Pitch, 1=Filter, 2=Volume
};

// CC numbers for HDCC-driven, real-time parameter modulation.
namespace XSamplerCC
{
    // Pitch / filter
    constexpr int Tune          = 110; // -100..+100 c
    constexpr int Cutoff        = 111; // 20..20480 Hz (log)
    constexpr int Resonance     = 112; // 0..24 dB

    // Vol ADSR
    constexpr int VolAttack     = 113;
    constexpr int VolDecay      = 114;
    constexpr int VolSustain    = 115;
    constexpr int VolRelease    = 116;

    // Filter ADSR
    constexpr int FltAttack     = 117;
    constexpr int FltDecay      = 118;
    constexpr int FltSustain    = 119;
    constexpr int FltRelease    = 120;
    constexpr int FltEnvAmount  = 104; // -1..+1 → fileg_depth ±4800 c

    // LFO 1
    constexpr int LfoRate       = 121;
    constexpr int LfoDepth      = 122;  // routed by overlay to current target
    constexpr int LfoDelay      = 123;

    // Velocity tracking
    constexpr int AmpVelTrack   = 126;  // 0..100 %
    constexpr int FilVelTrack   = 127;  // 0..4800 c

    // Analog (random pitch + delay)
    constexpr int PitchRandom   = 100;  // 0..10 c (subtle, analog-style)
    constexpr int DelayRandom   = 101;  // 0..3 ms

    // Sample start
    constexpr int SampleStart   = 105;  // 0..1 → offset 0..88200 samples
}

juce::String buildSfzWithOverride (const juce::File& originalSfz,
                                   const XSamplerSfzParams& p);
