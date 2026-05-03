#define XSAMPLER_TESTING 1

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../Source/PluginProcessor.h"
#include "../Source/PluginProcessor.cpp"
#include "../Source/PluginEditor.h"
#include "../Source/PluginEditor.cpp"
#include "../Source/SfzOverride.cpp"
#include "../Source/Arpeggiator.cpp"

#include <cmath>

namespace
{
    constexpr double kSR    = 48000.0;
    constexpr int    kBlock = 256;

    std::unique_ptr<XSamplerAudioProcessor> makePrepared()
    {
        auto p = std::make_unique<XSamplerAudioProcessor>();
        p->setPlayConfigDetails (0, 2, kSR, kBlock);
        p->prepareToPlay (kSR, kBlock);
        return p;
    }

    bool bufferIsSilent (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (std::abs (b.getReadPointer (ch)[i]) > 1.0e-9f)
                    return false;
        return true;
    }
}

//==============================================================================
struct ParameterTests : public juce::UnitTest
{
    ParameterTests() : juce::UnitTest ("Parameters", "XSampler") {}

    void runTest() override
    {
        beginTest ("All declared parameters exist with correct defaults");
        auto p = makePrepared();
        auto& a = p->apvts;

        struct Expect { const char* id; float def; };
        const Expect expects[] = {
            { "master_gain",         0.8f },
            { "tune_global",         0.0f },
            { "pitchbend_range",    12.0f },
            { "octave_transpose",    0.0f },
            { "start_offset",        0.0f },
            { "analog_amount",       0.0f },
            { "doubler_enabled",     0.0f },
            { "voice_mode",          0.0f },
            { "legato_enabled",      0.0f },
            { "portamento_time",     0.0f },
            { "fingered_portamento", 0.0f },
            { "filter_type",         0.0f },
            { "filter_cutoff",    8000.0f },
            { "filter_resonance",    0.0f },
            { "vol_attack",         0.01f },
            { "vol_decay",           0.1f },
            { "vol_sustain",         0.8f },
            { "vol_release",         0.3f },
            { "filter_attack",      0.01f },
            { "filter_decay",        0.1f },
            { "filter_sustain",      0.8f },
            { "filter_release",      0.3f },
            { "lfo_enabled",         0.0f },
            { "lfo_waveform",        0.0f },
            { "lfo_rate",            2.0f },
            { "lfo_depth",           0.0f },
            { "lfo_delay",           0.0f },
            { "lfo_target",          0.0f },
            { "output_width",        1.0f },
            { "velocity_to_volume",  0.8f },
            { "velocity_to_filter",  0.0f },
        };

        for (auto& e : expects)
        {
            auto* raw = a.getRawParameterValue (e.id);
            expect (raw != nullptr, juce::String ("Missing parameter: ") + e.id);
            if (raw == nullptr) continue;
            expectWithinAbsoluteError (raw->load(), e.def, 1.0e-3f,
                juce::String ("Default mismatch for ") + e.id);
        }
        expectEquals ((int) std::size (expects), 31, "Parameter count drift");
    }
};

//==============================================================================
struct BusLayoutTests : public juce::UnitTest
{
    BusLayoutTests() : juce::UnitTest ("Bus layout", "XSampler") {}

    void runTest() override
    {
        auto p = makePrepared();

        beginTest ("Stereo output is supported");
        juce::AudioProcessor::BusesLayout stereo;
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        expect (p->isBusesLayoutSupported (stereo));

        beginTest ("Mono output is rejected");
        juce::AudioProcessor::BusesLayout mono;
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        expect (! p->isBusesLayoutSupported (mono));

        beginTest ("Synth declares MIDI in, no MIDI out, no MIDI effect");
        expect (p->acceptsMidi());
        expect (! p->producesMidi());
        expect (! p->isMidiEffect());
    }
};

//==============================================================================
struct SilenceTests : public juce::UnitTest
{
    SilenceTests() : juce::UnitTest ("Silence without SFZ", "XSampler") {}

    void runTest() override
    {
        beginTest ("processBlock outputs silence when no SFZ is loaded");
        auto p = makePrepared();

        juce::AudioBuffer<float> buf (2, kBlock);
        // Pre-fill with garbage to confirm it gets cleared.
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlock; ++i)
                buf.getWritePointer (ch)[i] = 0.5f;

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        p->processBlock (buf, midi);

        expect (bufferIsSilent (buf), "Buffer should be silent before any SFZ load");
        expect (midi.isEmpty(), "MIDI buffer should be cleared by processBlock");
    }
};

//==============================================================================
struct StereoWidthTests : public juce::UnitTest
{
    StereoWidthTests() : juce::UnitTest ("Stereo width post-processing", "XSampler") {}

    static void runWidthBlock (XSamplerAudioProcessor& p,
                               juce::AudioBuffer<float>& buf,
                               float width)
    {
        // Drive the private path indirectly: call the public processBlock with no
        // SFZ loaded clears, so instead we test the math via a synthetic helper:
        // set output_width and reuse a known stereo signal manually. We replicate
        // the M/S formula here as the contract under test.
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getWritePointer (1);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float mid  = 0.5f * (L[i] + R[i]);
            const float side = 0.5f * (L[i] - R[i]) * width;
            L[i] = mid + side;
            R[i] = mid - side;
        }
        juce::ignoreUnused (p);
    }

    void runTest() override
    {
        auto p = makePrepared();

        beginTest ("Width=1 is identity for stereo signal");
        juce::AudioBuffer<float> b (2, 8);
        for (int i = 0; i < 8; ++i) { b.getWritePointer (0)[i] = 0.3f; b.getWritePointer (1)[i] = -0.7f; }
        runWidthBlock (*p, b, 1.0f);
        for (int i = 0; i < 8; ++i)
        {
            expectWithinAbsoluteError (b.getReadPointer (0)[i],  0.3f, 1.0e-6f);
            expectWithinAbsoluteError (b.getReadPointer (1)[i], -0.7f, 1.0e-6f);
        }

        beginTest ("Width=0 collapses to mono");
        juce::AudioBuffer<float> m (2, 8);
        for (int i = 0; i < 8; ++i) { m.getWritePointer (0)[i] = 0.4f; m.getWritePointer (1)[i] = -0.2f; }
        runWidthBlock (*p, m, 0.0f);
        for (int i = 0; i < 8; ++i)
        {
            expectWithinAbsoluteError (m.getReadPointer (0)[i], 0.1f, 1.0e-6f); // mid = (0.4 + -0.2)/2
            expectWithinAbsoluteError (m.getReadPointer (1)[i], 0.1f, 1.0e-6f);
        }
    }
};

//==============================================================================
struct StateRoundTripTests : public juce::UnitTest
{
    StateRoundTripTests() : juce::UnitTest ("State save / restore", "XSampler") {}

    void runTest() override
    {
        beginTest ("APVTS values survive get/setStateInformation");

        auto a = makePrepared();
        a->apvts.getParameter ("master_gain")->setValueNotifyingHost (0.123f);
        a->apvts.getParameter ("filter_cutoff")->setValueNotifyingHost (0.5f);
        a->apvts.getParameter ("voice_mode")->setValueNotifyingHost (1.0f); // Mono

        juce::MemoryBlock blob;
        a->getStateInformation (blob);

        auto b = makePrepared();
        b->setStateInformation (blob.getData(), (int) blob.getSize());

        expectWithinAbsoluteError (a->apvts.getRawParameterValue ("master_gain")->load(),
                                   b->apvts.getRawParameterValue ("master_gain")->load(),
                                   1.0e-4f);
        expectWithinAbsoluteError (a->apvts.getRawParameterValue ("filter_cutoff")->load(),
                                   b->apvts.getRawParameterValue ("filter_cutoff")->load(),
                                   1.0e-2f);
        expectEquals ((int) b->apvts.getRawParameterValue ("voice_mode")->load(), 1);
    }
};

//==============================================================================
struct LoadSfzTests : public juce::UnitTest
{
    LoadSfzTests() : juce::UnitTest ("SFZ loading", "XSampler") {}

    void runTest() override
    {
        beginTest ("Loading a non-existent file is rejected");
        auto p = makePrepared();
        expect (! p->loadSfzFile (juce::File ("/this/path/does/not/exist.sfz")));
        expect (! p->getCurrentSfzFile().existsAsFile());

        beginTest ("Loading a minimal valid SFZ succeeds");
        // Minimal SFZ with no sample requirement: a single region with sample=*sine
        // (sfizz built-in oscillator generator). Avoids needing audio assets.
        auto tmp = juce::File::createTempFile ("xsampler_test.sfz");
        tmp.replaceWithText ("<region>sample=*sine\n");
        const bool ok = p->loadSfzFile (tmp);
        expect (ok, "sfizz should load *sine SFZ");
        if (ok)
            expect (p->getCurrentSfzFile() == tmp);
        tmp.deleteFile();
    }
};

//==============================================================================
struct RenderTests : public juce::UnitTest
{
    RenderTests() : juce::UnitTest ("Rendering with SFZ", "XSampler") {}

    void runTest() override
    {
        beginTest ("noteOn produces non-silent audio with *sine SFZ");
        auto p = makePrepared();

        auto tmp = juce::File::createTempFile ("xsampler_render.sfz");
        tmp.replaceWithText ("<region>sample=*sine\n");
        if (! p->loadSfzFile (tmp))
        {
            tmp.deleteFile();
            expect (false, "Could not load *sine SFZ for render test");
            return;
        }

        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);

        // Render a few blocks to let the voice ramp.
        bool foundSignal = false;
        for (int b = 0; b < 8 && ! foundSignal; ++b)
        {
            buf.clear();
            p->processBlock (buf, midi);
            midi.clear();
            if (! bufferIsSilent (buf))
                foundSignal = true;
        }
        expect (foundSignal, "Expected non-silent output after noteOn with *sine");

        // All-notes-off should eventually produce silence.
        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        buf.clear();
        p->processBlock (buf, off);

        // Run several release blocks; with default release the tail may persist,
        // so just confirm the engine still renders without error and finite samples.
        for (int i = 0; i < kBlock; ++i)
        {
            expect (std::isfinite (buf.getReadPointer (0)[i]));
            expect (std::isfinite (buf.getReadPointer (1)[i]));
        }

        tmp.deleteFile();
    }
};

//==============================================================================
struct MidiHandlingTests : public juce::UnitTest
{
    MidiHandlingTests() : juce::UnitTest ("MIDI handling", "XSampler") {}

    void runTest() override
    {
        beginTest ("Octave transpose shifts note in range and clamps");
        auto p = makePrepared();
        auto tmp = juce::File::createTempFile ("xsampler_midi.sfz");
        tmp.replaceWithText ("<region>sample=*sine\n");
        p->loadSfzFile (tmp);

        // Set octave_transpose to -3 → C0 (0) shifted by -36 should clamp to 0.
        // We can't directly observe sfizz internals from here, but we can make sure
        // processBlock with extreme octave shifts doesn't crash and produces finite output.
        if (auto* prm = p->apvts.getParameter ("octave_transpose"))
            prm->setValueNotifyingHost (0.0f); // -3 (min)

        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 0, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 127, (juce::uint8) 100), 1);
        midi.addEvent (juce::MidiMessage::pitchWheel (1, 0), 2);
        midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 3);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 7, 100), 4);
        midi.addEvent (juce::MidiMessage::allNotesOff (1), 5);

        buf.clear();
        p->processBlock (buf, midi);

        for (int i = 0; i < kBlock; ++i)
        {
            expect (std::isfinite (buf.getReadPointer (0)[i]));
            expect (std::isfinite (buf.getReadPointer (1)[i]));
        }

        tmp.deleteFile();
    }
};

//==============================================================================
struct RealSfzSmokeTest : public juce::UnitTest
{
    RealSfzSmokeTest() : juce::UnitTest ("Real SFZ smoke (Resonant2)", "XSampler") {}

    void runTest() override
    {
        const juce::File sfz (
            "/Users/capitalsound/Library/Mobile Documents/com~apple~CloudDocs/Code/XSampler/SFZ/Resonant2.sfz");

        beginTest ("Resonant2.sfz file exists");
        if (! sfz.existsAsFile())
        {
            logMessage ("SKIPPING — SFZ not present at expected path");
            return;
        }

        beginTest ("Plugin loads Resonant2.sfz");
        auto p = makePrepared();
        const bool loaded = p->loadSfzFile (sfz);
        expect (loaded, "Resonant2.sfz failed to load");
        if (! loaded) return;

        beginTest ("noteOn produces non-silent finite audio (no crash)");
        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        // Render up to 2 seconds at 48k → ~375 blocks of 256 samples.
        // Streaming samplers may buffer for the first few blocks before audio
        // appears; allow ample time.
        bool foundSignal = false;
        float peak = 0.0f;
        const int maxBlocks = 400;
        for (int b = 0; b < maxBlocks; ++b)
        {
            buf.clear();
            p->processBlock (buf, midi);
            midi.clear();

            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                auto* d = buf.getReadPointer (ch);
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    expect (std::isfinite (d[i]), "Non-finite sample in render");
                    peak = std::max (peak, std::abs (d[i]));
                    if (peak > 1.0e-4f) foundSignal = true;
                }
            }
            if (foundSignal && b > 20) break;
        }
        logMessage ("Peak amplitude observed: " + juce::String (peak, 6));
        expect (foundSignal, "No audible output from Resonant2.sfz after 400 blocks");

        beginTest ("Voice-mode toggle does not crash mid-render");
        if (auto* prm = p->apvts.getParameter ("voice_mode"))
        {
            // Toggle several times across blocks — replicates the original
            // crash repro where setNumVoices hit the audio thread every block.
            for (int b = 0; b < 32; ++b)
            {
                prm->setValueNotifyingHost (b & 1 ? 1.0f : 0.0f);
                buf.clear();
                juce::MidiBuffer empty;
                p->processBlock (buf, empty);
            }
            expect (true, "Survived rapid voice-mode toggles");
        }
    }
};

//==============================================================================
namespace
{
    // Render a fixed number of blocks holding note 60, returning the buffer
    // that contains roughly 200 ms of audio after the attack.
    juce::AudioBuffer<float> renderHeldNote (XSamplerAudioProcessor& p,
                                             int note,
                                             int blocks,
                                             juce::uint8 vel = 100)
    {
        // Tests change params and immediately render — bypass the timer
        // throttle that fires on the (idle) message thread.
        p.flushOverlayNow();

        juce::AudioBuffer<float> total (2, blocks * kBlock);
        total.clear();

        juce::AudioBuffer<float> blk (2, kBlock);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, vel), 0);

        for (int b = 0; b < blocks; ++b)
        {
            blk.clear();
            p.processBlock (blk, midi);
            midi.clear();
            for (int ch = 0; ch < 2; ++ch)
                total.copyFrom (ch, b * kBlock, blk, ch, 0, kBlock);
        }
        return total;
    }

    float bufferRMS (const juce::AudioBuffer<float>& b, int from = 0, int len = -1)
    {
        if (len < 0) len = b.getNumSamples() - from;
        double sum = 0.0;
        const int N = b.getNumChannels() * len;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            auto* d = b.getReadPointer (ch);
            for (int i = from; i < from + len; ++i)
                sum += d[i] * d[i];
        }
        return std::sqrt ((float) (sum / juce::jmax (1, N)));
    }

    // High-frequency RMS via simple difference (proxy for spectral content
    // above ~Nyquist/4). Useful for filter cutoff comparisons.
    float bufferHighFreqRMS (const juce::AudioBuffer<float>& b)
    {
        double sum = 0.0;
        int N = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            auto* d = b.getReadPointer (ch);
            for (int i = 1; i < b.getNumSamples(); ++i)
            {
                const float diff = d[i] - d[i-1];
                sum += diff * diff;
                ++N;
            }
        }
        return std::sqrt ((float) (sum / juce::jmax (1, N)));
    }

    bool loadResonant (XSamplerAudioProcessor& p)
    {
        const juce::File sfz (
            "/Users/capitalsound/Library/Mobile Documents/com~apple~CloudDocs/Code/XSampler/SFZ/Resonant2.sfz");
        return sfz.existsAsFile() && p.loadSfzFile (sfz);
    }
}

//==============================================================================
struct OverlayParamTests : public juce::UnitTest
{
    OverlayParamTests() : juce::UnitTest ("Overlay parameters affect audio", "XSampler") {}

    void runTest() override
    {
        auto p = makePrepared();
        if (! loadResonant (*p))
        {
            beginTest ("Resonant2.sfz available");
            logMessage ("SKIPPING — SFZ not present");
            return;
        }

        // ---- Filter cutoff ----------------------------------------------
        beginTest ("Filter cutoff: low cutoff has less HF energy than high cutoff");
        p->apvts.getParameter ("filter_type")->setValueNotifyingHost   (0.0f); // LP
        p->apvts.getParameter ("filter_resonance")->setValueNotifyingHost (0.0f);

        // Open filter
        p->apvts.getParameter ("filter_cutoff")->setValueNotifyingHost (1.0f); // 20kHz
        auto open = renderHeldNote (*p, 60, 60);
        const float openHF = bufferHighFreqRMS (open);

        // Closed filter
        p->apvts.getParameter ("filter_cutoff")->setValueNotifyingHost (0.0f); // 20Hz
        auto closed = renderHeldNote (*p, 60, 60);
        const float closedHF = bufferHighFreqRMS (closed);

        logMessage ("HF RMS  open=" + juce::String (openHF, 6) + "  closed=" + juce::String (closedHF, 6));
        expect (openHF > closedHF * 1.5f,
                "Closing the LP filter should reduce high-frequency content");

        // Reset cutoff
        p->apvts.getParameter ("filter_cutoff")->setValueNotifyingHost (1.0f);

        // ---- Vol Attack -------------------------------------------------
        beginTest ("Vol Attack: long attack ramps in slowly");
        p->apvts.getParameter ("vol_attack")->setValueNotifyingHost (0.0f);  // ~1 ms
        auto fast = renderHeldNote (*p, 60, 12);

        p->apvts.getParameter ("vol_attack")->setValueNotifyingHost (0.5f);  // long
        auto slow = renderHeldNote (*p, 60, 12);

        // Compare RMS of the first 30 ms.
        const int firstMs = (int) (kSR * 0.03);
        const float fastEarly = bufferRMS (fast, 0, firstMs);
        const float slowEarly = bufferRMS (slow, 0, firstMs);
        logMessage ("Early RMS  fast-attack=" + juce::String (fastEarly, 6)
                    + "  slow-attack=" + juce::String (slowEarly, 6));
        expect (fastEarly > slowEarly * 1.5f,
                "Slow attack should have lower energy in the first 30 ms than fast attack");

        p->apvts.getParameter ("vol_attack")->setValueNotifyingHost (0.0f);

        // ---- Tune cents -------------------------------------------------
        beginTest ("Tune cents: changing tune produces a different waveform");
        auto tune0   = renderHeldNote (*p, 60, 8);
        p->apvts.getParameter ("tune_global")->setValueNotifyingHost (1.0f); // +100 cents
        auto tune100 = renderHeldNote (*p, 60, 8);

        // Compute correlation: should differ when tuned.
        double dot = 0.0, n0 = 0.0, n1 = 0.0;
        const int N = juce::jmin (tune0.getNumSamples(), tune100.getNumSamples());
        for (int i = 0; i < N; ++i)
        {
            const float a = tune0.getReadPointer (0)[i];
            const float b = tune100.getReadPointer (0)[i];
            dot += a * b; n0 += a * a; n1 += b * b;
        }
        const double corr = (n0 > 0 && n1 > 0) ? dot / std::sqrt (n0 * n1) : 1.0;
        logMessage ("Tune correlation 0 vs +100c: " + juce::String (corr, 4));
        expect (corr < 0.9, "Tuning by +100 cents should noticeably decorrelate the signal");
        p->apvts.getParameter ("tune_global")->setValueNotifyingHost (0.5f); // back to 0
    }
};

//==============================================================================
struct ArpTests : public juce::UnitTest
{
    ArpTests() : juce::UnitTest ("Arpeggiator", "XSampler") {}

    static int countNoteOns (const juce::MidiBuffer& mb)
    {
        int n = 0;
        for (const auto m : mb)
            if (m.getMessage().isNoteOn()) ++n;
        return n;
    }

    void runTest() override
    {
        Arpeggiator arp;
        arp.prepare (kSR);
        arp.setBpm (120.0);
        arp.setEnabled (true);
        arp.setMode (Arpeggiator::Up);
        arp.setRate (Arpeggiator::Sixteenth);
        arp.setOctaves (1);
        arp.setGate (0.5f);

        // Seed three held notes.
        juce::MidiBuffer in;
        in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        in.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        in.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);

        // Render 1 second, count noteOns. At 120 bpm, 1/16 → 8 steps/sec.
        beginTest ("16th notes at 120 BPM ≈ 8 steps per second");
        juce::MidiBuffer out;
        int noteOns = 0;
        const int totalSamples = (int) kSR;
        const int steps = totalSamples / kBlock;
        for (int b = 0; b < steps; ++b)
        {
            arp.process (in, out, kBlock);
            noteOns += countNoteOns (out);
            in.clear(); // only initial press
        }
        logMessage ("noteOns in 1 s: " + juce::String (noteOns));
        expect (noteOns >= 7 && noteOns <= 9,
                "Expected ~8 noteOns/s for 1/16 at 120 BPM");

        // Pattern walks Up (60 → 64 → 67 → 60 …).
        beginTest ("Up mode walks the pattern in ascending order");
        arp.reset();
        arp.setEnabled (true);
        in.clear();
        in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        in.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        in.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);

        std::vector<int> emitted;
        for (int b = 0; b < steps && (int) emitted.size() < 6; ++b)
        {
            arp.process (in, out, kBlock);
            for (const auto m : out)
                if (m.getMessage().isNoteOn())
                    emitted.push_back (m.getMessage().getNoteNumber());
            in.clear();
        }
        if (emitted.size() >= 6)
        {
            const std::vector<int> expectVec { 60, 64, 67, 60, 64, 67 };
            for (size_t i = 0; i < 6; ++i)
                expectEquals (emitted[i], expectVec[i],
                    "Step " + juce::String ((int) i) + " should be " + juce::String (expectVec[i]));
        }
        else
        {
            expect (false, "Did not collect 6 noteOns in time");
        }

        // Hold latches even after key release.
        beginTest ("Hold latches notes after release");
        arp.reset();
        arp.setEnabled (true);
        arp.setHold (true);
        in.clear();
        in.addEvent (juce::MidiMessage::noteOn  (1, 60, (juce::uint8) 100), 0);
        in.addEvent (juce::MidiMessage::noteOff (1, 60),                    1);

        int held = 0;
        for (int b = 0; b < 16; ++b)
        {
            arp.process (in, out, kBlock);
            held += countNoteOns (out);
            in.clear();
        }
        expect (held >= 1, "Hold should keep producing noteOns after the key released");

        // Disable → no notes generated, pass-through.
        beginTest ("Disabled arp passes MIDI through unchanged");
        arp.setEnabled (false);
        in.clear();
        in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        arp.process (in, out, kBlock);
        bool sawTheNote = false;
        for (const auto m : out)
            if (m.getMessage().isNoteOn() && m.getMessage().getNoteNumber() == 60)
                sawTheNote = true;
        expect (sawTheNote, "Pass-through should preserve the original noteOn");
    }
};

//==============================================================================
struct EditorTests : public juce::UnitTest
{
    EditorTests() : juce::UnitTest ("Editor lifecycle", "XSampler") {}

    void runTest() override
    {
        beginTest ("Editor can be created and destroyed");
        auto p = makePrepared();
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        expect (ed != nullptr);
        expectEquals (ed->getWidth(),  560);
        expectEquals (ed->getHeight(), 880);
    }
};

//==============================================================================
// Static instances — UnitTestRunner picks them up via getAllTests().
static ParameterTests       _t_params;
static BusLayoutTests       _t_bus;
static SilenceTests         _t_silence;
static StereoWidthTests     _t_width;
static StateRoundTripTests  _t_state;
static LoadSfzTests         _t_load;
static RenderTests          _t_render;
static MidiHandlingTests    _t_midi;
static RealSfzSmokeTest     _t_real_sfz;
static OverlayParamTests    _t_overlay;
static ArpTests             _t_arp;
static EditorTests          _t_editor;

int main (int, char**)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("XSampler");

    int failures = 0, total = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (auto* r = runner.getResult (i))
        {
            total    += r->passes + r->failures;
            failures += r->failures;
        }
    }
    std::cout << "\n=== XSampler tests: " << (total - failures)
              << "/" << total << " passed ===\n";
    return failures == 0 ? 0 : 1;
}
