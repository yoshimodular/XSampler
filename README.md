# XSampler

An SFZ-powered sampler plugin for macOS. Built with **JUCE 8** and the **sfizz** SFZ engine. Targets VST3, AU, and Standalone on Apple Silicon.

> **Status:** Alpha 1 skeleton. Audio engine works (load any `.sfz`, play notes, pitchbend, CCs). Most macro parameters are wired in the UI but not yet routed to per-region SFZ overrides — see [Parameter status](#parameter-status).

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

All 31 parameters in the spec are declared in the APVTS and persist in state. Wiring to the sfizz engine is staged:

| Parameter | Status |
|---|---|
| `master_gain` | ✅ applied post-render |
| `octave_transpose` | ✅ applied to MIDI note numbers |
| `pitchbend_range` | ✅ pitchwheel scaled by ratio to SFZ default |
| `voice_mode` (poly/mono) | ✅ approximated via `setNumVoices(1)` when mono |
| `output_width` | ✅ M/S matrix post-render |
| `tune_global` (cents) | ⏳ TODO — needs runtime SFZ override |
| `start_offset` | ⏳ TODO — needs `offset` opcode override |
| `analog_amount` | ⏳ TODO — `tune_random` + `offset_random` injection |
| `doubler_enabled` | ⏳ TODO — second voice hard L/R |
| `legato_enabled`, `portamento_*`, `fingered_portamento` | ⏳ TODO |
| `filter_type`, `filter_cutoff`, `filter_resonance` | ⏳ TODO |
| Volume + filter ADSR (8 params) | ⏳ TODO |
| LFO (6 params) | ⏳ TODO |
| `velocity_to_volume`, `velocity_to_filter` | ⏳ TODO |

✅ = audible / verified · ⏳ = exposed in state but not yet routed (engine plays the SFZ as authored).

The "TODO" group all require either runtime `<region>` overlays generated on top of the loaded SFZ, or sfizz's experimental override API. They are next on the roadmap.

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
