#include "Arpeggiator.h"

void Arpeggiator::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void Arpeggiator::reset()
{
    heldVel.fill (0);    numHeld = 0;
    latchedVel.fill (0); numLatched = 0;
    waitingForFirstKey = true;
    pattern.clear();
    patternVel.clear();
    patternIndex = 0;
    upDownDir    = +1;
    activeNote          = -1;
    samplesUntilOff     = 0;
    samplesUntilNextStep = 0;
}

int Arpeggiator::stepsPerBeat() const
{
    switch (rate)
    {
        case Quarter:           return 1;
        case Eighth:            return 2;
        case EighthTriplet:     return 3;
        case Sixteenth:         return 4;
        case SixteenthTriplet:  return 6;
        case ThirtySecond:      return 8;
    }
    return 4;
}

int Arpeggiator::samplesPerStep() const
{
    const double secsPerBeat = 60.0 / bpm;
    return juce::jmax (16, (int) std::round (secsPerBeat * sr / stepsPerBeat()));
}

void Arpeggiator::rebuildPattern()
{
    pattern.clear();
    patternVel.clear();

    // Source set: physically held keys, or latched set if hold is on.
    const auto& srcVel  = (hold && numLatched > 0) ? latchedVel : heldVel;
    const int   srcCount = (hold && numLatched > 0) ? numLatched : numHeld;
    if (srcCount == 0) return;

    std::vector<std::pair<int, juce::uint8>> notes;
    notes.reserve ((size_t) srcCount);
    for (int n = 0; n < 128; ++n)
        if (srcVel[(size_t) n] > 0)
            notes.emplace_back (n, srcVel[(size_t) n]);

    if (notes.empty()) return;

    // Order according to mode.
    switch (mode)
    {
        case Up:
        case UpDown:
            std::sort (notes.begin(), notes.end(),
                [] (auto& a, auto& b) { return a.first < b.first; });
            break;
        case Down:
            std::sort (notes.begin(), notes.end(),
                [] (auto& a, auto& b) { return a.first > b.first; });
            break;
        case Random:
        case AsPlayed:
            // Random shuffles each step boundary; AsPlayed keeps pitch-sorted
            // by default (we don't track press order yet — TODO).
            std::sort (notes.begin(), notes.end(),
                [] (auto& a, auto& b) { return a.first < b.first; });
            break;
    }

    // Multi-octave expansion.
    const int oct = juce::jlimit (1, 4, octaves);
    for (int o = 0; o < oct; ++o)
        for (auto& nv : notes)
        {
            const int shifted = nv.first + 12 * o;
            if (shifted >= 0 && shifted <= 127)
            {
                pattern.push_back (shifted);
                patternVel.push_back (nv.second);
            }
        }

    if (mode == UpDown && pattern.size() > 1)
    {
        // Mirror without repeating the apex.
        for (int i = (int) pattern.size() - 2; i > 0; --i)
        {
            pattern.push_back (pattern[(size_t) i]);
            patternVel.push_back (patternVel[(size_t) i]);
        }
    }
}

void Arpeggiator::process (const juce::MidiBuffer& in,
                           juce::MidiBuffer& out,
                           int numSamples)
{
    out.clear();

    if (! enabled)
    {
        // Pass-through. Also keep heldVel coherent so toggling on mid-perf
        // immediately sees the user's keys.
        for (const auto meta : in)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())  { heldVel[(size_t) m.getNoteNumber()] = (juce::uint8) m.getVelocity(); ++numHeld; }
            else if (m.isNoteOff()) { if (heldVel[(size_t) m.getNoteNumber()] > 0) --numHeld; heldVel[(size_t) m.getNoteNumber()] = 0; }
            out.addEvent (m, meta.samplePosition);
        }
        return;
    }

    // Track keys + maintain latch.
    for (const auto meta : in)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            if (heldVel[(size_t) n] == 0) ++numHeld;
            heldVel[(size_t) n] = (juce::uint8) m.getVelocity();

            if (hold)
            {
                if (waitingForFirstKey)
                {
                    // First note of a new phrase — wipe previous latch.
                    latchedVel.fill (0);
                    numLatched = 0;
                    waitingForFirstKey = false;
                }
                if (latchedVel[(size_t) n] == 0) ++numLatched;
                latchedVel[(size_t) n] = (juce::uint8) m.getVelocity();
            }
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            if (heldVel[(size_t) n] > 0) --numHeld;
            heldVel[(size_t) n] = 0;

            if (hold && numHeld == 0)
                waitingForFirstKey = true; // arm new phrase on next press
        }
    }

    // Decide active set this block.
    rebuildPattern();

    // Stop any sounding arp note if the source is empty (and not holding).
    if (pattern.empty())
    {
        if (activeNote >= 0)
        {
            out.addEvent (juce::MidiMessage::noteOff (1, activeNote), 0);
            activeNote = -1;
            samplesUntilOff = 0;
        }
        // Reset step timing so when notes return, the first step fires fast.
        samplesUntilNextStep = 0;
        return;
    }

    const int stepLen = samplesPerStep();

    for (int i = 0; i < numSamples; ++i)
    {
        // Note-off tick.
        if (activeNote >= 0 && samplesUntilOff > 0)
        {
            if (--samplesUntilOff == 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (1, activeNote), i);
                activeNote = -1;
            }
        }

        // Step boundary.
        if (samplesUntilNextStep <= 0)
        {
            if (activeNote >= 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (1, activeNote), i);
                activeNote = -1;
            }

            int idx;
            if (mode == Random)
                idx = random.nextInt ((int) pattern.size());
            else
            {
                if (patternIndex >= (int) pattern.size()) patternIndex = 0;
                idx = patternIndex++;
            }

            const int note = pattern[(size_t) idx];
            const auto vel = patternVel[(size_t) idx];
            out.addEvent (juce::MidiMessage::noteOn (1, note, vel), i);
            activeNote      = note;
            samplesUntilOff = juce::jmax (1, (int) (gate * stepLen));
            samplesUntilNextStep = stepLen;
        }
        else
        {
            --samplesUntilNextStep;
        }
    }
}
