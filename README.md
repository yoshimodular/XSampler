# XSampler

An SFZ-driven sampler plugin for macOS, built on **JUCE 8** and the **sfizz** SFZ engine. Targets VST3, AU, and Standalone on Apple Silicon today; designed with a future Windows port in mind (no platform-specific code outside JUCE/sfizz themselves).

> **Status:** alpha 0.0.x. Engine is stable. Most musical parameters are wired through HDCC routing for real-time, glitch-free response. Active development on UX polish and structural feature work (legato, portamento, doubler, tempo).

---

## Table of contents

1. [What XSampler is](#what-xsampler-is)
2. [Architecture overview](#architecture-overview)
3. [Build / install / test](#build--install--test)
4. [Versioned binaries](#versioned-binaries)
5. [Parameter reference](#parameter-reference)
6. [How the SFZ overlay works](#how-the-sfz-overlay-works)
7. [How the HDCC routing works](#how-the-hdcc-routing-works)
8. [Portamento engine](#portamento-engine)
9. [Doubler engine](#doubler-engine)
10. [Analog mode](#analog-mode)
11. [LFO](#lfo)
12. [Filter envelope](#filter-envelope)
13. [Voice mode and legato](#voice-mode-and-legato)
14. [Tempo and arpeggiator](#tempo-and-arpeggiator)
15. [Session state](#session-state)
16. [Threading model](#threading-model)
17. [Known limitations / TODOs](#known-limitations--todos)
18. [Roadmap](#roadmap)

---

## What XSampler is

A sampler plugin that **plays user-supplied SFZ instruments** through [sfizz 1.2.3](https://github.com/sfztools/sfizz) and exposes a coherent set of macro controls on top: filter, ADSR per envelope, LFO, velocity tracking, mono/poly + legato, portamento (free or tempo-synced), arpeggiator, doubler, "analog" detune/jitter. Every macro is host-automatable, every change should respond seamlessly while audio is playing — no clicks, no dropouts.

The SFZ format is the source of truth. XSampler doesn't parse instruments itself; it asks sfizz to. What XSampler *does* is build an **overlay** — a synthesised `<global>` block that sits on top of the user's SFZ — declaring CC-modulation slots that the macros drive in real time.

Today there is no custom UI: a `juce::GenericAudioProcessorEditor` lists every parameter, with a SFZ file picker and a small piano keyboard above it.

---

## Architecture overview

```
┌──────────────────────────────────────────────────────────────────┐
│                    XSamplerAudioProcessor                        │
│                                                                  │
│   ┌──────────┐    ┌──────────┐    ┌──────────────┐               │
│   │  APVTS   │───▶│ Listener │───▶│ overlayDirty │               │
│   └──────────┘    └──────────┘    └──────────────┘               │
│         │                                  │                     │
│         │ raw param ptrs                   │ Timer @ 20 Hz       │
│         ▼                                  ▼                     │
│   ┌──────────────┐                  ┌──────────────────┐         │
│   │ flushParamCCs│                  │ rebuildOverlay() │         │
│   │  (audio)     │                  │ (host thread)    │         │
│   └──────┬───────┘                  └─────────┬────────┘         │
│          │                                    │                  │
│          ▼ hdcc()                             ▼ loadSfzString()  │
│   ┌────────────────────────────────────────────────────────┐     │
│   │                    sfz::Sfizz                          │     │
│   └────────────────────────────────────────────────────────┘     │
│          ▲                                    ▲                  │
│   ┌──────┴──────┐                      ┌──────┴────────┐         │
│   │ Arpeggiator │ ◀ ─ MIDI in          │ Portamento    │         │
│   │             │                      │ engine        │         │
│   └─────────────┘                      └───────────────┘         │
└──────────────────────────────────────────────────────────────────┘
```

Three things happen on parameter changes:

1. **Smooth changes (CC-routed)** — cutoff, resonance, ADSR, LFO rate/depth/delay, velocity tracks, sample start, analog amount, tune, filter env amount. The SFZ overlay declares one `_oncc{N}` slot per parameter, and `flushParamCCs()` (called at the start of every `processBlock`) sends `hdcc(0, N, value)` only for parameters whose values actually moved. **No engine reload, ever.**

2. **Structural changes (overlay rebuild)** — `filter_type`, `lfo_waveform`, `lfo_target`, `lfo_active` (depth crossing zero), `voice_mode`, `legato_enabled`, `doubler_enabled`. These need new SFZ opcodes baked into the source, so we re-emit a combined SFZ string and call `loadSfzString`. Sample data is cached by sfizz; only the SFZ source is re-parsed.

3. **Urgent vs. deferred rebuilds** — `voice_mode`, `legato_enabled`, `doubler_enabled` apply **immediately**, even if a voice is sounding (the user expects toggle response). The non-urgent rebuilds wait until `synth->getNumActiveVoices() == 0` so the change is hidden under silence; if that doesn't happen within 2 s, they apply anyway. **Either way, every rebuild does `allSoundOff` + re-trigger of every key currently held in `heldNoteVel[]`** — so the sound continues seamlessly through the structural change.

---

## Build / install / test

### Requirements

| | Version |
|---|---|
| macOS | 12.0+ (Monterey) |
| Architecture | Apple Silicon (arm64) only for now |
| Xcode Command Line Tools | required (`xcode-select --install`) |
| CMake | 3.22+ |
| Ninja | required by `build.sh` (full Xcode.app **not** required) |

```bash
brew install cmake ninja
```

### Build

```bash
git clone https://github.com/yoshimodular/XSampler.git
cd XSampler
./build.sh
```

This fetches JUCE 8.0.4 and sfizz 1.2.3 via `FetchContent`, applies two upstream patches automatically (see [Build notes](#build-notes)), and produces:

```
build/XSampler_artefacts/Release/
├── VST3/XSampler.vst3
├── AU/XSampler.component
└── Standalone/XSampler.app
```

### Install

```bash
cp -R build/XSampler_artefacts/Release/VST3/XSampler.vst3       ~/Library/Audio/Plug-Ins/VST3/
cp -R build/XSampler_artefacts/Release/AU/XSampler.component    ~/Library/Audio/Plug-Ins/Components/
killall -9 AudioComponentRegistrar 2>/dev/null   # refresh the AU cache
```

### Test

```bash
./test.sh
```

Runs the JUCE `UnitTest` suites declared in [Tests/Tests.cpp](Tests/Tests.cpp). The suites cover:

- Parameter declaration and defaults
- Bus layout (stereo only)
- Silence-without-SFZ behaviour
- Stereo M/S math
- Session state save/restore (only the SFZ path persists)
- SFZ loading (bad path rejected, minimal `*sine` SFZ accepted)
- **Real SFZ smoke** — loads `SFZ/Resonant2.sfz`, plays a noteOn, asserts non-silent finite audio
- MIDI handling (extreme noteOn, pitchwheel min/max, CC, allNotesOff)
- Overlay parameter audibility — filter cutoff reduces HF energy, slow attack reduces early energy, +100 c tune decorrelates audio
- Smooth-params guarantees — CC-driven cutoff change works *without* a rebuild; a continuous cutoff sweep produces 0 silent blocks
- Mono mode steals voices (poly RMS > mono RMS × 1.4)
- Structural-rebuild hold-notes — toggling doubler mid-play keeps the held note audible
- **Portamento engine** — 7 sub-tests: time=0 = no glide, 0.5 s glide arrives at 0, fingered respects key release, sync 1/4 @ 120 BPM = 0.5 s, poly disables portamento, audio finite during glide
- Arpeggiator timing, mode walking, hold latching, pass-through
- Editor lifecycle

Latest run: **43156/43156 pass.**

### Build notes

`CMakeLists.txt` applies two upstream sfizz patches automatically via `FetchContent`'s `PATCH_COMMAND`:

1. **Strip ARM32 flags.** sfizz 1.2.3's `cmake/SfizzConfig.cmake` adds `-mfpu=neon -mfloat-abi=hard` for any CPU matching `arm.*`. These are ARM32-only and clang on Apple Silicon (`arm64-apple-darwin`) rejects them.
2. **Fix `atomic_queue` template syntax.** sfizz pins an older `atomic_queue` submodule that uses `Base::template do_pop_any(...)` without an argument list — newer clang turns this into a hard error (`-Wmissing-template-arg-list-after-template-kw`). The patch removes the unnecessary `template` keyword.

Both patches run idempotently; a clean `rm -rf build && ./build.sh` works.

---

## Versioned binaries

`./release.sh` reads the version from `project(XSampler VERSION x.y.z ...)` in [CMakeLists.txt](CMakeLists.txt), builds the plugin, and stages all three formats into `bin/<version>/`.

```
bin/
├── 0.0.2/   first usable build with piano keyboard + crash fix
├── 0.0.3/   SFZ overlay layer + arpeggiator
├── 0.0.4/   real-time CC modulation, no reloads
├── 0.0.5/   filter env amount, sample start, tempo, doubler v1, mono fix
├── 0.0.6/   …
└── 0.0.7/   legato, held-note re-trigger on rebuild, LFO depth gate, 1% step on analog
```

Bump `VERSION` in `CMakeLists.txt`, run `./release.sh`, commit `bin/<new-version>/`.

---

## Parameter reference

Layout order matches what `juce::GenericAudioProcessorEditor` displays. All knob ranges are listed in user space; internal CC normalisation is documented per-parameter in [How the HDCC routing works](#how-the-hdcc-routing-works).

### Global

| ID | Range | Default | Notes |
|---|---|---|---|
| `master_gain` | 0.0 – 1.0 | 0.8 | Smoothed by `juce::SmoothedValue` (30 ms ramp) — no zipper noise. |
| `tune_global` | -100 – +100 cents | 0 | CC-routed (`tune_oncc110`). |
| `pitchbend_range` | 1 – 24 semitones | 12 | Scales the **user's** wheel input. Internal SFZ bend range is fixed at 24 semitones; the processor translates between the two. No SFZ rebuild required. |
| `octave_transpose` | -3 – +3 octaves | 0 | Applied to MIDI note numbers before they hit sfizz. |
| `sample_start` | 0.0 – 1.0 | 0.0 | CC-routed (`offset_oncc105`) — slider × 4410 samples (~100 ms at 44.1 kHz). The SFZ `offset` opcode is absolute, so this is intentionally a small range to stay inside short percussive samples. |
| `analog_amount` | 0% – 100% (1% step) | 0% | Drives [Analog mode](#analog-mode) when doubler is OFF, [Doubler mode](#doubler-engine) when ON. |
| `doubler_enabled` | bool | false | Toggle between Analog and Doubler interpretation of `analog_amount`. **Urgent** rebuild on change. |

### Voicing

| ID | Range | Default | Notes |
|---|---|---|---|
| `voice_mode` | Poly · Mono | Poly | Mono = `<global> polyphony=1` + `setNumVoices(1)` + voice steal. **Urgent** rebuild. |
| `legato_enabled` | bool | false | Active only when `voice_mode = Mono`. See [Voice mode and legato](#voice-mode-and-legato). **Urgent** rebuild. |
| `portamento_time` | 0.0 – 20.0 s (log) | 0.0 | Used when `portamento_sync` is OFF. |
| `portamento_sync` | bool | false | Switch between time and tempo-synced portamento. |
| `portamento_rate` | 1/32 · 1/16T · 1/16 · 1/8T · 1/8 · 1/4T · 1/4 · 1/2 · 1 bar | 1/8 | Used when `portamento_sync` is ON. Resolved against current tempo (host or user, see Tempo). |
| `fingered_portamento` | bool | false | Glide only when the previous key is still held. |

### Filter

| ID | Range | Default | Notes |
|---|---|---|---|
| `filter_type` | LP · HP · BP | LP | `fil_type=lpf_2p / hpf_2p / bpf_2p`. Structural rebuild (deferred). |
| `filter_cutoff` | 20 – 20000 Hz (log) | 8000 | CC-routed (`cutoff_oncc111`). Logarithmic mapping: slider 0..1 → cents 0..12000 above 20 Hz base. |
| `filter_resonance` | 0.0 – 1.0 | 0.0 | CC-routed (`resonance_oncc112`). 1.0 = +24 dB. |
| `filter_env_amount` | -1.0 – +1.0 | 0.0 | CC-routed (`fileg_depth_oncc104`). Bipolar: 0 = no modulation, +1 = +4800 c brighten, -1 = -4800 c darken. |

### Volume ADSR + Velocity → Volume

| ID | Range | Default | Notes |
|---|---|---|---|
| `vol_attack` | 0.001 – 10 s (log) | 0.01 | CC-routed (`ampeg_attack_oncc113`). |
| `vol_decay` | 0.001 – 10 s (log) | 0.1 | CC-routed (`ampeg_decay_oncc114`). |
| `vol_sustain` | 0.0 – 1.0 | 0.8 | CC-routed (`ampeg_sustain_oncc115`). 1.0 = 100 %. |
| `vol_release` | 0.001 – 20 s (log) | 0.3 | CC-routed (`ampeg_release_oncc116`). |
| `velocity_to_volume` | 0.0 – 1.0 | 0.8 | CC-routed (`amp_veltrack_oncc126`). 0 = velocity has no effect on volume, 1 = full sensitivity. |

### Filter ADSR + Velocity → Filter

| ID | Range | Default | Notes |
|---|---|---|---|
| `filter_attack` | 0.001 – 10 s (log) | 0.01 | CC-routed (`fileg_attack_oncc117`). |
| `filter_decay` | 0.001 – 10 s (log) | 0.1 | CC-routed (`fileg_decay_oncc118`). |
| `filter_sustain` | 0.0 – 1.0 | 0.8 | CC-routed (`fileg_sustain_oncc119`). |
| `filter_release` | 0.001 – 20 s (log) | 0.3 | CC-routed (`fileg_release_oncc120`). |
| `velocity_to_filter` | 0.0 – 1.0 | 0.0 | CC-routed (`fil_veltrack_oncc127`). 1.0 = velocity adds up to 4800 c to cutoff. |

### LFO

There is no LFO On/Off — `lfo_depth = 0` means inactive.

| ID | Range | Default | Notes |
|---|---|---|---|
| `lfo_waveform` | Sine · Tri · Saw · Sq · Random | Sine | Structural rebuild on change (sfizz `lfoNN_wave` is not CC-modulatable). |
| `lfo_rate` | 0.01 – 20 Hz (log) | 2.0 | CC-routed (`lfo01_freq_oncc121`). |
| `lfo_depth` | 0.0 – 1.0 | 0.0 | CC-routed (`lfo01_pitch_oncc / cutoff_oncc / volume_oncc`, depending on target — only the active target is non-zero). Crossing 0 ↔ >0 triggers an LFO declaration rebuild because sfizz silences output if the LFO opcodes are present with all-zero depths. |
| `lfo_delay` | 0.0 – 4.0 s (log) | 0.0 | CC-routed (`lfo01_delay_oncc123`). |
| `lfo_target` | Pitch · Filter · Volume | Pitch | Structural rebuild on change. |

### Tempo

| ID | Range | Default | Notes |
|---|---|---|---|
| `tempo_bpm` | 30 – 300 BPM | 120 | Used when `tempo_sync` is OFF (or when the host doesn't expose a playhead, e.g. in standalone). |
| `tempo_sync` | bool | true | When ON, both arpeggiator and portamento-sync follow the host BPM via `getPlayHead()->getPosition()->getBpm()`. |

### Arpeggiator

| ID | Range | Default | Notes |
|---|---|---|---|
| `arp_enabled` | bool | false | Pass-through when off. |
| `arp_hold` | bool | false | Latch the last set when all keys release; new keypress wipes the latch. |
| `arp_mode` | Up · Down · UpDown · Random · AsPlayed | Up | AsPlayed currently sorted by pitch — true played-order tracking is TODO. |
| `arp_rate` | 1/4 · 1/8 · 1/8T · 1/16 · 1/16T · 1/32 | 1/16 | Tempo-synced. |
| `arp_octaves` | 1 – 4 | 1 | Pattern repeats up `arp_octaves` octaves. |
| `arp_gate` | 0.05 – 1.0 | 0.5 | Note length as a fraction of a step. |

---

## How the SFZ overlay works

When a parameter that requires an overlay rebuild changes:

1. The `juce::AudioProcessorValueTreeState::Listener` flags `overlayDirty` and stores the millisecond timestamp.
2. The 20 Hz `Timer::timerCallback` checks the flag. It waits 60 ms after the last change (debounce). If the parameter is **urgent** (`voice_mode`, `legato_enabled`, `doubler_enabled`), it applies immediately. Otherwise, it polls `synth->getNumActiveVoices()` and waits until silence, with a 2 s ceiling.
3. When it's time to rebuild, the processor calls `buildSfzWithOverride()`. This:
   - Reads the user's `.sfz` source as text.
   - Strips opcodes XSampler controls globally (`cutoff=`, `resonance=`, `fil_type=`, all `ampeg_*=` and `fileg_*=`, `amp_veltrack=`, `fil_veltrack=`, `tune=`, `polyphony=`, `trigger=`, `pitch_random=`, `delay_random=`, `offset=`, `pan=`, `bend_up=`, `bend_down=`, plus `lfoNN_*=`). Suffixed variants like `_oncc7`, `_curvecc7`, `_smoothcc7` are caught by the same regex.
   - Prepends a synthesised `<global>` block declaring all the CC routes (one `_oncc{N}` per controllable parameter), the bend range (fixed `bend_up=2400 / bend_down=-2400`), and the LFO opcodes when active.
   - For **legato** (mono only): splits the source at the first SFZ section header and emits the body twice — once inside `<group>\ntrigger=first\n` and once inside `<group>\ntrigger=legato\n`.
   - For **doubler** (regardless of legato): splits and emits the body twice with `<group>\npan=-100\ntune_oncc100=-25\n` and `<group>\npan=100\ntune_oncc100=25\ndelay_oncc101=0.005\n`.
4. Calls `synth->loadSfzString(originalPath, combined)`. Sample data is cached by sfizz so the reload is fast.
5. Calls `synth->allSoundOff()` to clear in-flight voices, re-sends every CC value, and re-triggers every note in `heldNoteVel[]`. The user hears the change but never hears a dropped note.

---

## How the HDCC routing works

Each smooth parameter has a dedicated CC number declared inside `Source/SfzOverride.h::XSamplerCC`. The overlay declares the matching `_oncc{N}` opcode with a fixed modulation amount; the processor sends `synth->hdcc(0, N, normValue)` whenever the parameter moves.

Internal CC bank used:

```
100 PitchRandom        110 Tune          120 FltRelease
101 DelayRandom        111 Cutoff        121 LfoRate
102 (reserved)         112 Resonance     122 LfoDepth (target-dependent)
103 (reserved)         113 VolAttack     123 LfoDelay
104 FltEnvAmount       114 VolDecay      126 AmpVelTrack
105 SampleStart        115 VolSustain    127 FilVelTrack
                       116 VolRelease
                       117 FltAttack
                       118 FltDecay
                       119 FltSustain
```

`flushParamCCs()` is called at the start of every `processBlock` and at the end of every overlay rebuild. It diffs the current parameter values against `lastCC[]` and only sends what changed. On a fresh rebuild it forces a full re-flush so internal sfizz state is always in sync.

---

## Portamento engine

Portamento is implemented as a **monophonic pitch-bend glide** in `XSamplerAudioProcessor` itself, not via SFZ opcodes. It runs only when `voice_mode = Mono` because sfizz's pitchwheel is global per-channel, not per-voice.

### State

```cpp
int   portaLastNote;          // last-triggered MIDI note number
float portaCurrentSemis;      // current pitch offset, in semitones
float portaSemisPerSample;    // glide speed (signed)
int   portaSamplesRemaining;  // glide budget remaining
int   portaCurrentBendValue;  // last 14-bit pitchwheel value sent
int   userPitchBendValue;     // most recent host-sent wheel value
```

### How it works

1. **Trigger** — On every noteOn that arrives at the synth (after the arpeggiator), `startPortamentoTo(newNote)` runs:
   - Skip if Poly mode or `portaLastNote < 0` (first ever note).
   - Skip if `portamento_time` resolves to < 0.1 ms.
   - Skip if `fingered_portamento` is on **and** `heldNoteVel[portaLastNote] == 0` (the previous key has been released).
   - Otherwise: set `portaCurrentSemis = portaLastNote - newNote` (so the new voice initially sounds at the *old* pitch), compute `portaSemisPerSample = -portaCurrentSemis / totalSamples`, store `portaSamplesRemaining = totalSamples`.

2. **Bend emission** — Within `processBlock`, before any MIDI event we emit a pitchwheel reflecting the current state. Between events, an inner `advancePortamentoTo(targetSample)` lambda steps the glide in 64-sample sub-block resolution, advancing `portaCurrentSemis` and emitting fresh pitchwheel events whenever the value changes. This gives ~1.3 ms granularity at 48 kHz — perceptually smooth.

3. **Bend range** — The SFZ overlay always declares `bend_up=2400 / bend_down=-2400`, giving sfizz a fixed 24-semitone internal range. The processor maps both the user's wheel input *and* the portamento offset onto this fixed range; the user's `pitchbend_range` parameter only scales how the user's wheel input is interpreted. **No rebuild on `pitchbend_range` change** — it's a pure scaling factor in our pitchwheel math.

4. **Sync mode** — When `portamento_sync = true`, the glide time is computed from current tempo:

   ```
   secsPerBeat   = 60 / bpm
   inBeats       = { 1/8, 1/6, 1/4, 1/3, 1/2, 2/3, 1, 2, 4 }   (1/32 … 1 bar)
   glideSeconds  = secsPerBeat × inBeats[selectedRate]
   ```

   `bpm` follows `tempo_sync`: host playhead when on, user `tempo_bpm` when off.

### Verified behaviour (unit tests)

| Test | Result |
|---|---|
| Time = 0 → no glide | `portaSamplesRemaining == 0` ✓ |
| Time = 0.5 s @ 48 kHz → ~24000 sample glide | ✓ |
| Glide completes — `portaCurrentSemis → 0` exactly | ✓ |
| Fingered + previous key released → no glide | ✓ |
| Fingered + previous key held → glide | ✓ |
| Sync 1/4 @ 120 BPM → 0.5 s glide | ✓ |
| Poly mode → portamento disabled | ✓ |
| Audio finite throughout glide (no NaN) | ✓ |

---

## Doubler engine

Toggling `doubler_enabled` triggers a structural rebuild. The overlay duplicates the user's region body inside two `<group>` wrappers:

```
<group>
pan=-100
tune_oncc100=-25
[user regions]

<group>
pan=100
tune_oncc100=25
delay_oncc101=0.005
[user regions]
```

Each played note now triggers **two voices** — one hard-panned left, one hard-panned right, with opposite pitch detune driven by the analog/doubler slider (CC100), and a small per-note delay on the right channel (CC101). Result: a classic L/R double-tracking effect at the cost of doubled voice count.

`analog_amount` (0..1) drives both CCs simultaneously: at 0% no detune/delay (essentially mono); at 100% ±25 cents detune and 5 ms delay on the right side.

Doubler and Analog are mutually exclusive — the same slider does different things depending on the toggle.

---

## Analog mode

When `doubler_enabled` is OFF, `analog_amount` drives:

- `pitch_random=0 / pitch_random_oncc100=10` — each new voice gets a small random pitch offset, max ±10 cents at 100 %. Subtle, in the spirit of an old analog oscillator drifting slightly from note to note.
- `delay_random=0 / delay_random_oncc101=0.003` — each new voice gets a random startup delay, max ±3 ms. This breaks phase coherence between repeated triggers without producing audible attack jitter.

Maxes are intentionally tuned for **"analog synth feel"** rather than glitch. If you want extreme effects, this isn't the slider for that.

---

## LFO

A single LFO (`lfo01`) routed to one of three targets at a time:

- **Pitch** — `lfo01_pitch_oncc122=1200` → up to ±1 octave vibrato at depth = 1.0
- **Filter** — `lfo01_cutoff_oncc122=4800` → up to ±4 octaves wobble
- **Volume** — `lfo01_volume_oncc122=24` → up to ±24 dB tremolo

Rate (`lfo01_freq_oncc121=20`) is in Hz, scaled from the user knob 0.01..20 Hz.

The LFO opcodes are only declared in the overlay when `lfo_depth > 0` — sfizz 1.2.3 silences output if the LFO is declared with all-zero modulation depths. Crossing the depth = 0 boundary is the only condition that triggers a depth-related rebuild, and it's handled by a dedicated APVTS listener on `lfo_depth`.

Changing waveform or target also triggers a (deferred) rebuild because those opcodes can't be CC-modulated.

---

## Filter envelope

`filter_env_amount` is bipolar, exposed via a dedicated CC (104) and a bipolar overlay declaration:

```
fileg_depth=-4800
fileg_depth_oncc104=9600
```

CC104 = 0.5 → effective `fileg_depth = -4800 + 9600·0.5 = 0` (no modulation). CC104 = 1.0 → +4800 c (envelope brightens cutoff). CC104 = 0.0 → -4800 c (envelope darkens). User knob -1..+1 maps to CC104 0..1.

This means the filter ADSR controls (`filter_attack/decay/sustain/release`) only do anything when `filter_env_amount ≠ 0`. It's by design: many SFZ instruments don't expect an active filter envelope, and a non-zero default depth would surprise users on any patch.

---

## Voice mode and legato

`voice_mode = Mono` injects `<global> polyphony=1` and calls `synth->setNumVoices(1)`. The next noteOn steals the previous voice cleanly.

`legato_enabled = true` (only meaningful in Mono) duplicates the region body into two `<group>` wrappers:

```
<group>
trigger=first
[regions]

<group>
trigger=legato
[regions]
```

sfizz then routes incoming noteOns to:

- `trigger=first` regions when no previous note is sounding — full envelope retrigger.
- `trigger=legato` regions when overlapping with another note — the new voice inherits the active envelope state.

This is the canonical SFZ legato pattern. It requires Mono to behave as a true legato instrument (otherwise you'd just stack legato + first regions on top of each other).

`fingered_portamento` is independent of `legato_enabled` — it gates **pitch glide**, not envelope behaviour.

---

## Tempo and arpeggiator

`tempo_sync` controls where the BPM comes from:

- ON → `getPlayHead()->getPosition()->getBpm()`. Falls back to the user knob if the host doesn't expose a playhead (typical for standalone use).
- OFF → user knob (`tempo_bpm`, 30..300 BPM).

The chosen BPM feeds:

- The arpeggiator's step length (`60/bpm × inBeatsForRate / 1`).
- The portamento sync mode's glide length (same formula).

Arpeggiator details: see [Source/Arpeggiator.cpp](Source/Arpeggiator.cpp). Implementation is a simple per-block step engine with hold latching, octave expansion, gate-fraction note-off scheduling, and 5 modes (Up, Down, UpDown, Random, AsPlayed).

---

## Session state

`getStateInformation` writes only `<XSamplerSession sfzPath="..."/>`. **Knob values are not persisted** — every session starts from the parameter defaults declared in `createLayout`. This is deliberate while the engine is stabilising: it guarantees that opening an old session can never produce a silent or pathologically misconfigured plugin.

`setStateInformation` reads only `sfzPath` and re-loads the file if it still exists. The APVTS state remains at defaults.

When that policy changes (it will), it'll be a versioned upgrade with a migration path.

---

## Threading model

```
┌────────────────┐                          ┌────────────────┐
│ Message thread │                          │  Audio thread  │
│ (UI/host calls)│                          │ (processBlock) │
└────┬───────────┘                          └────┬───────────┘
     │                                           │
     │  parameterChanged()                       │
     │  → set overlayDirty, lastChangeMs          │
     │                                           │
     │  Timer @ 20 Hz (host thread)              │
     │  → rebuildAndApplyOverlay()               │
     │     ┌── synthLock (CriticalSection) ──┐   │
     │     │ allSoundOff                     │   │
     │     │ loadSfzString                   │   │
     │     │ setNumVoices                    │   │
     │     │ flushParamCCs(force=true)       │   │
     │     │ re-noteOn held keys             │   │
     │     └─────────────────────────────────┘   │
     │                                           │
     │                                           │
     │                                       ┌── synthLock ──┐
     │                                       │ flushParamCCs │
     │                                       │ portamento +  │
     │                                       │ MIDI dispatch │
     │                                       │ renderBlock   │
     │                                       └───────────────┘
```

- All sfizz calls (`loadSfzString`, `noteOn`, `noteOff`, `pitchWheel`, `cc`, `hdcc`, `renderBlock`, `setNumVoices`, `setSamplesPerBlock`, `setSampleRate`, `allSoundOff`) happen under `synthLock`.
- **`setNumVoices` and `loadSfzString` are not real-time safe** (they reallocate). They only ever run on the host thread, never on the audio thread.
- `flushParamCCs` runs both on the audio thread (per block) and on the host thread (post-rebuild). The same lock serialises both.
- Atomic flags (`overlayDirty`, `overlayUrgent`, `sfzLoaded`) coordinate without locking.
- `heldNoteVel[]` and the portamento state are touched only from the audio thread, except for the rebuild path which holds the lock.

---

## Known limitations / TODOs

| Limitation | Why | Plan |
|---|---|---|
| `start_offset` only goes up to ~100 ms | SFZ `offset` opcode is in absolute samples; we can't make it relative to per-region sample length. | Inspect each region's sample length on load and bake per-region offset_oncc maxes. |
| `lfo_target` change rebuilds | sfizz `lfoNN_pitch / cutoff / volume` aren't CC-multiplexable. | Could declare three LFO instances and toggle which one is depth-active via CC. Costs CPU; will revisit if rebuild latency becomes an issue. |
| Portamento is mono-only | sfizz pitchwheel is global per channel. | Implement per-voice pitch tracking and emit per-note pitch offsets via per-region CC routing. |
| Doubler doubles voice cost | Two regions per note. | Could be replaced by a single-region pan-spread approach via `pan_oncc` + `delay_oncc` if we ever need the CPU. |
| Knob values don't persist | See [Session state](#session-state). | Add migration when the engine is settled. |
| macOS arm64 only | Only what's been built and tested. | Universal binary first, then a Windows port — the C++/JUCE/sfizz code is portable; CI will need to be set up. |
| AsPlayed arp mode is pitch-sorted | Press-order tracking not implemented. | Add a small ring buffer of press order in `Arpeggiator`. |

---

## Roadmap

- **0.0.8** — multi-platform CMake hardening, no Apple-only assumptions.
- **0.0.9** — custom UI to replace the generic editor.
- **0.1.0** — universal binary + Windows VST3 build.
- **0.1.1** — preset/instrument browser, ad-hoc SFZ favourites.
- **0.1.2** — code-signing + notarisation pipeline for releases.
- **Beyond** — per-voice portamento, per-region offset awareness, MIDI Learn for the parameters.

---

## License

TBD. JUCE and sfizz retain their own licences (GPL/commercial for JUCE, ISC for sfizz).
