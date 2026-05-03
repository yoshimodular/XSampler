#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

class Arpeggiator
{
public:
    enum Mode { Up = 0, Down, UpDown, Random, AsPlayed };
    enum Rate { Quarter = 0, Eighth, EighthTriplet, Sixteenth, SixteenthTriplet, ThirtySecond };

    void prepare (double sampleRate);
    void reset();

    // Configuration (any thread; values latched at the next step boundary).
    void setEnabled (bool e)    noexcept { enabled = e; }
    void setHold    (bool h)    noexcept { hold = h; }
    void setMode    (int m)     noexcept { mode = (Mode) juce::jlimit (0, 4, m); }
    void setRate    (int r)     noexcept { rate = (Rate) juce::jlimit (0, 5, r); }
    void setOctaves (int n)     noexcept { octaves = juce::jlimit (1, 4, n); }
    void setGate    (float g)   noexcept { gate = juce::jlimit (0.05f, 1.0f, g); }
    void setBpm     (double b)  noexcept { bpm = b > 1.0 ? b : 120.0; }

    bool isEnabled() const noexcept { return enabled; }

    // Consume `in`, write arp output (or pass-through when disabled) into `out`.
    // Always clears `out` first.
    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples);

private:
    void rebuildPattern();
    int  samplesPerStep() const;
    int  stepsPerBeat() const;

    double sr { 44100.0 };
    double bpm { 120.0 };

    bool enabled { false };
    bool hold    { false };
    Mode mode    { Up };
    Rate rate    { Sixteenth };
    int  octaves { 1 };
    float gate   { 0.5f };

    // Currently-held physical keys (note → velocity).
    std::array<juce::uint8, 128> heldVel {};
    int numHeld { 0 };

    // Latched notes when hold is on (cleared when a new key starts after release).
    std::array<juce::uint8, 128> latchedVel {};
    int numLatched { 0 };
    bool waitingForFirstKey { true };

    // Pattern (note numbers).
    std::vector<int>     pattern;
    std::vector<juce::uint8> patternVel;
    int patternIndex { 0 };
    int upDownDir   { +1 };

    // Active arp note (currently sounding via emitted noteOn).
    int  activeNote     { -1 };
    int  samplesUntilOff{ 0 };
    int  samplesUntilNextStep { 0 };

    juce::Random random;
};
