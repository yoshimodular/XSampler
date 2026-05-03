# XSampler

An SFZ-powered sampler plugin for macOS. Built with **JUCE 8** and the **sfizz** SFZ engine. Targets VST3, AU, and Standalone on Apple Silicon.

> **Status:** Alpha 0.0.3. Audio engine works (load any `.sfz`, play notes, pitchbend, CCs). All overlay parameters are wired through a runtime SFZ override layer (`<global>` opcodes injected on top of the loaded file, with per-region conflicts stripped). Includes a built-in arpeggiator.

---

## Features

- Loads any SFZ instrument via [sfizz 1.2.3](https://github.com/sfztools/sfizz).
- VST3 + Audio Unit + Standalone, single-build CMake — no Projucer.
- 31 parameters exposed through `AudioProcessorValueTreeState` (host-automatable).
- Master gain, octave transpose, pitchbend range scaling, stereo width (M/S) post-processing.
- State persistence: APVTS + last-loaded SFZ path saved with the project.
- Generic UI on top of a `Load SFZ…` chooser — final UI to come.
- Full unit-test battery (JUCE `UnitTest`, 9 suites).

---

## Requirements

| | Version |
|---|---|
| macOS | 12.0+ (Monterey) |
| Architecture | Apple Silicon (arm64) — Intel not yet supported |
| Xcode Command Line Tools | required (`xcode-select --install`) |
| CMake | 3.22+ |
| Ninja | required by `build.sh` (full Xcode.app **not** required) |

```bash
brew install cmake ninja
```

---

## Build

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

### Versioned binaries

`./release.sh` builds the plugin and stages the artefacts into `bin/<version>/`.
The version comes from `project(XSampler VERSION x.y.z ...)` in `CMakeLists.txt`
— bump it there before each release.

```
bin/
└── 0.0.2/
    ├── XSampler.vst3
    ├── XSampler.component
    └── XSampler.app
```

### Install the plugin

```bash
cp -R build/XSampler_artefacts/Release/VST3/XSampler.vst3       ~/Library/Audio/Plug-Ins/VST3/
cp -R build/XSampler_artefacts/Release/AU/XSampler.component    ~/Library/Audio/Plug-Ins/Components/
```

After installing the AU you may need to clear the AU cache:

```bash
killall -9 AudioComponentRegistrar 2>/dev/null
```

### Run the standalone

```bash
open build/XSampler_artefacts/Release/Standalone/XSampler.app
```

---

## Tests

A standalone test executable runs the JUCE `UnitTest` suite (no DAW needed):

```bash
./test.sh
```

Suites covered:

1. **Parameters** — every declared ID exists with the correct default.
2. **Bus layout** — stereo accepted, mono rejected; MIDI flags correct.
3. **Silence without SFZ** — `processBlock` clears buffer + MIDI when nothing is loaded.
4. **Stereo width** — width=1 identity, width=0 collapses to mono.
5. **State save/restore** — `getStateInformation` / `setStateInformation` round-trip.
6. **SFZ loading** — bad path rejected, minimal `<region>sample=*sine` loads.
7. **Rendering with SFZ** — noteOn produces non-silent finite audio.
8. **MIDI handling** — extreme noteOn / pitchWheel / CC / allNotesOff under -3 octave shift produce finite output.
9. **Editor lifecycle** — `createEditor()` returns a 520×800 component cleanly destroyed.

Exit code `0` = all green; non-zero = failures (per-test summary printed).

---

## Project layout

```
XSampler/
├── CMakeLists.txt         # JUCE + sfizz via FetchContent (with auto-patches)
├── build.sh               # Ninja Release build for arm64
├── test.sh                # Configure + build + run unit tests
├── Source/
│   ├── PluginProcessor.h
│   ├── PluginProcessor.cpp
│   ├── PluginEditor.h
│   └── PluginEditor.cpp
├── Tests/
│   └── Tests.cpp          # 9 JUCE UnitTest suites
└── SFZ/                   # User-supplied SFZ libraries (gitignored)
```

---

## Parameter status

All 31 macro parameters + 6 arp parameters are wired and verified by unit tests:

| Parameter | Status | How |
|---|---|---|
| `master_gain` | ✅ | post-render gain |
| `octave_transpose` | ✅ | applied to MIDI note numbers |
| `pitchbend_range` | ✅ | pitchwheel scaled |
| `voice_mode` (poly/mono) | ✅ | `polyphony=1` overlay + voice count |
| `output_width` | ✅ | post-render M/S matrix |
| `tune_global` cents | ✅ | overlay `tune=` |
| `analog_amount` | ✅ | overlay `pitch_random` + `delay_random` |
| `filter_type / cutoff / resonance` | ✅ | overlay `fil_type / cutoff / resonance` |
| Volume ADSR (4) | ✅ | overlay `ampeg_*` |
| Filter ADSR (4) | ✅ | overlay `fileg_*` (depth=2400 c) |
| LFO 1 (6 params) | ✅ | overlay `lfo01_freq / wave / delay / pitch|cutoff|volume` |
| `velocity_to_volume`, `velocity_to_filter` | ✅ | overlay `amp_veltrack / fil_veltrack` |
| `arp_enabled / hold / mode / rate / octaves / gate` | ✅ | host-side MIDI engine |
| `start_offset` | ⏳ | needs per-region inspection (`offset` is sample-frame-based) |
| `doubler_enabled` | ⏳ | needs duplicated-region injection |
| `legato_enabled`, `portamento_*`, `fingered_portamento` | ⏳ | sfizz 1.2.3 lacks a clean SFZ-level expression |

### How the overlay works

When any overlay parameter changes, an `AudioProcessorValueTreeState::Listener` flags the SFZ as dirty. A `Timer` running at 20 Hz on the message thread waits ~80 ms for the user to settle, then:

1. Loads the original `.sfz` source as text.
2. Strips conflicting opcodes from regions/groups (`cutoff=`, `ampeg_*=`, `lfoNN_*=`, etc.) so per-region values can't shadow our globals.
3. Prepends a synthesised `<global>` block built from the current parameter values.
4. Calls `sfz::Sfizz::loadSfzString()` under the synth lock — sfizz re-parses the SFZ but **keeps the decoded sample cache** by absolute path, so the reload is fast.

### Arpeggiator

| Knob | Range | Default |
|---|---|---|
| `arp_enabled` | bool | off |
| `arp_hold` | bool | off |
| `arp_mode` | Up · Down · UpDown · Random · AsPlayed | Up |
| `arp_rate` | 1/4 · 1/8 · 1/8T · 1/16 · 1/16T · 1/32 | 1/16 |
| `arp_octaves` | 1–4 | 1 |
| `arp_gate` | 0.05–1.0 | 0.5 |

BPM is taken from the host playhead (falls back to 120 BPM when running standalone). Hold latches the held set when all keys are released; a fresh keypress wipes the latch and starts a new phrase.

---

## Build notes

`CMakeLists.txt` applies two upstream sfizz patches automatically via `FetchContent`'s `PATCH_COMMAND`:

1. **Strip ARM32 flags.** sfizz's `cmake/SfizzConfig.cmake` adds `-mfpu=neon -mfloat-abi=hard` for any CPU matching `arm.*`. These are ARM32-only and clang on Apple Silicon (`arm64-apple-darwin`) rejects them.
2. **Fix `atomic_queue` template syntax.** sfizz pins an older `atomic_queue` submodule that uses `Base::template do_pop_any(...)` without an argument list — newer clang turns this into a hard error (`-Wmissing-template-arg-list-after-template-kw`). The patch removes the unnecessary `template` keyword.

Both patches run idempotently and survive a clean `rm -rf build && ./build.sh`.

---

## Roadmap

- [ ] Runtime SFZ-override layer for the ⏳ parameters above
- [ ] Custom UI (replace `GenericAudioProcessorEditor`)
- [ ] Universal binary (arm64 + x86_64)
- [ ] Code-signed + notarized release builds
- [ ] Preset / instrument browser

---

## License

TBD. JUCE and sfizz retain their own licenses (GPL/commercial for JUCE, ISC for sfizz).
