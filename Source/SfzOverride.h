#pragma once

#include <juce_core/juce_core.h>

// Structural parameters that require an overlay rebuild. Smooth knobs
// (cutoff, resonance, ADSR, LFO rate/depth/delay, vel-tracks, analog,
// sample start, filter env amount) are CC-driven and update in real
// time without a reload.
struct XSamplerSfzParams
{
    bool mono           { false };
    bool legato         { false };
    bool doubler        { false };

    // Heavy sections — declaring them adds DSP that decorrelates audio
    // from vanilla even at neutral CC values. Boundary-gated.
    bool filterActive   { false };
    bool lfoActive      { false };
    int  filterType     { 0 };
    int  lfoWave        { 0 };
    int  lfoTarget      { 0 };

    // Static (parse-time) opcode values. sfizz does NOT honour _oncc
    // modulation for these — the value is locked when the SFZ parses.
    // We bake in the user's knob value (or the bank's authored value
    // populated into the knob) at every overlay rebuild. Knob changes
    // for these trigger a deferred structural rebuild.
    float ampVelTrack    { 100.0f };  // sfizz default = 100
    float filVelTrack    { 0.0f };    // sfizz default = 0
    float pitchRandom    { 0.0f };    // 0..N cents
    float delayRandom    { 0.0f };    // 0..N seconds
    float sampleOffset   { 0.0f };    // 0..N samples
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
