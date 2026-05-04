#pragma once

#include <juce_core/juce_core.h>

// Structural parameters that require an overlay rebuild. Smooth knobs
// (cutoff, resonance, ADSR, LFO rate/depth/delay, vel-tracks, analog,
// sample start, filter env amount) are CC-driven and update in real
// time without a reload.
struct XSamplerSfzParams
{
    bool mono              { false };
    bool legato            { false };
    bool doubler           { false };
    // Per-section "active" flags. The overlay only declares (and strips
    // user opcodes for) a section when it's active. At defaults nothing
    // is declared, so playback matches vanilla bit-for-bit.
    bool ampegActive       { false };  // any vol ADSR knob non-default
    bool filterActive      { false };  // cutoff < 19k or any reso/env/vel knob >0
    bool lfoActive         { false };  // lfo_depth > 0
    bool ampVelTrackActive { false };  // velocity_to_volume != 1.0 (sfizz default)
    bool filVelTrackActive { false };  // velocity_to_filter > 0
    bool analogActive      { false };  // analog_amount > 0
    bool sampleStartActive { false };  // sample_start > 0
    bool tuneActive        { false };  // tune_global != 0
    int  filterType        { 0 };
    int  lfoWave           { 0 };
    int  lfoTarget         { 0 };
};

// CC numbers used for HDCC-driven, real-time parameter modulation.
//
// IMPORTANT — these are above the standard MIDI CC range (0..127) on
// purpose. Real-world SFZ banks (e.g. the Aliexpress Erhu) reserve
// CCs in the 100..127 area for their own instrument logic (Unison
// Vol on CC100, Mono Switch on CC105, Humanize on CC117 …). Using
// those would silently break the instrument. Sfizz supports internal
// CC indices well past 127, so we reserve 200+ for ourselves and
// stay clear of anything the bank might touch.
namespace XSamplerCC
{
    // Pitch / filter
    constexpr int Tune          = 200;
    constexpr int Cutoff        = 201;
    constexpr int Resonance     = 202;

    // Vol ADSR
    constexpr int VolAttack     = 203;
    constexpr int VolDecay      = 204;
    constexpr int VolSustain    = 205;
    constexpr int VolRelease    = 206;

    // Filter ADSR
    constexpr int FltAttack     = 207;
    constexpr int FltDecay      = 208;
    constexpr int FltSustain    = 209;
    constexpr int FltRelease    = 210;
    constexpr int FltEnvAmount  = 211;

    // LFO 1 (declared as lfo99_* to stay clear of any user LFO).
    constexpr int LfoRate       = 212;
    constexpr int LfoDepth      = 213;
    constexpr int LfoDelay      = 214;

    // Velocity tracking
    constexpr int AmpVelTrack   = 215;
    constexpr int FilVelTrack   = 216;

    // Analog (random pitch + delay)
    constexpr int PitchRandom   = 217;
    constexpr int DelayRandom   = 218;

    // Sample start
    constexpr int SampleStart   = 219;
}

juce::String buildSfzWithOverride (const juce::File& originalSfz,
                                   const XSamplerSfzParams& p);

// Snapshot of values extracted from an SFZ file's authored opcodes.
// All fields are optional; -1 / NaN signals "not specified, use default".
struct XSamplerSfzDefaults
{
    float ampegAttack   { -1.0f };  // seconds, sfizz default = 0
    float ampegDecay    { -1.0f };  // seconds, sfizz default = 0
    float ampegSustain  { -1.0f };  // 0..100 %, sfizz default = 100
    float ampegRelease  { -1.0f };  // seconds, sfizz default = 0.001
    float filegAttack   { -1.0f };
    float filegDecay    { -1.0f };
    float filegSustain  { -1.0f };
    float filegRelease  { -1.0f };
    float filegDepth    { -1.0e9f };// cents, sfizz default = 0
    float cutoff        { -1.0f };  // Hz; -1 = no filter
    float resonance     { -1.0e9f };// dB
    int   filType       { -1 };     // 0=LP, 1=HP, 2=BP; -1 = none
    float ampVelTrack   { -1.0f };  // 0..100, sfizz default = 100
    float filVelTrack   { -1.0e9f };// cents, sfizz default = 0
};

// Parse the SFZ source (after inlining + path absolutising) and extract
// representative defaults. Picks the first <global>-level value for each
// opcode, falling back to the first <region> value, then leaves the field
// at its sentinel if absent.
XSamplerSfzDefaults extractDefaults (const juce::File& originalSfz);
