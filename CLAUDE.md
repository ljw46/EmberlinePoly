# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Emberline Poly is a JUCE 8 / C++20 MIDI generator plugin (VST3, AU on macOS, Standalone). It produces MIDI, no audio. UI, theme,
build and CI conventions deliberately mirror the sibling repo `~/repos/EmberlineSpectralis`; check there first when adding something that repo already solves.

## Commands

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release     # JUCE 8.0.6 is fetched; add -DFETCHCONTENT_SOURCE_DIR_JUCE=<path> to reuse a local checkout
    cmake --build build --target EmberlinePoly_VST3 EmberlinePoly_Standalone   # plus EmberlinePoly_AU on macOS
    cmake --build build --target EmberlinePolyTests && build/EmberlinePolyTests_artefacts/Release/EmberlinePolyTests
    cmake --build build --target EmberlinePolyTool              # snapshot out.png | render out.mid  (needs no display)

The test binary is a single `main()` with `CHECK` macros (no framework), so there is no single-test filter: add a `test...()` function to `tests/EngineTests.cpp`
and comment out the others in `main()` while iterating. `EmberlinePolyTests` and `EmberlinePolyTool` are `EXCLUDE_FROM_ALL`.
CI also validates the VST3 with pluginval (`--strictness-level 5 --skip-gui-tests`); the same command works locally.

The version lives only in `project(... VERSION)` in `CMakeLists.txt`; a release tag `vX.Y.Z[-label]` must match it (CI enforces this).

## Architecture

The code splits into a GUI-free core and the plugin shell. `EmberlinePolyCore` (static lib, depends on `juce_audio_basics` only) holds everything testable:

- `Settings.h`: a plain snapshot struct (4 `EngineSettings`, `ChordSettings`, scale / guard / humanize options, note rules, audition options). The engine only ever sees this, never JUCE parameters. Each engine has seven lanes
  (`StepKind`: pitch, velocity, gate, probability, ratchet, late, harmony), each with its own `laneLength` (0 follows `steps`) and `laneDivide`, read through `rates::laneStepAt` (stateless, like `stepAt`), and a `pitchMask` over chord-buffer positions.
- `SequencerEngine.*`: `Engine::process` is the heart. Four **motif engines**, each on its own grid (step count + rate + `n:d` ratio), fire notes. On a trigger an engine
  reads the **chord buffer** at that beat (`bufferAt`: held MIDI notes in live mode, otherwise the internal progression's chord), picks from it by arp mode
  (cyclic / chord hold / low&high / random / root), adds the step's pitch and the engine octave, runs the scale guard (`guard`), and emits a note-on/off pair
  (`addNote`) on the engine's channel. Ratchets subdivide the step. Step position is stateless against the host ppq (`rates::stepAt`), which is what makes
  polymeter / polyrhythm survive loops, jumps and tempo changes: do not replace it with per-engine running counters. Per-engine state is only the next-boundary
  bookkeeping and arp counters. Notes are queued ~25 ms ahead in a fixed event queue so negative jitter can land before the grid position; `sounding` is tracked per
  channel + pitch (several parts can sound the same pitch). Discontinuities (start, loop, jump >0.02 beat) flush sounding notes. With no host position the engine runs an
  internal clock (`run` / `bpm`). Internal chords are scheduled as long notes on chord boundaries (`scheduleChord`); `chords::timingShift` moves the boundaries for
  anticipated / off-beat changes and the engines read the same chord grid, so they follow the changes.
- `Chords.h`: voicing (`voice` stacks scale tones, so chords are always in the scale), chord timing helpers, and `suggest()`, the genre-aware progression generator
  (templates + weighted colours per genre; it also picks the scale). Pure and seeded, so it is testable.
- `Rates.h` (rate table + `n:d` ratio = speed multiplier), `Scales.h` (degree to semitone, nearest-tone snap), `Humanize.h` (xorshift RNG, bounded Gaussian jitter, gate slip).
- `Randomise.h` (per-role dice for one engine's pattern, pure and seeded), `Preset.h` (preset struct + JSON read/write; choices are stored by name so reordering a list does not break files, values are clamped on read)
  and `FactoryPresets.h` (the library, defined in C++ and exported to JSON by `EmberlinePolyTool presets dir`). Preset apply/capture against the APVTS is `params::applyPreset` / `capturePreset` in `Parameters.cpp`.
- `Capture.h` + `MidiClip.*`: the engine pushes every generated note (tagged with a source: 0 chords, 1-4 engines) into a lock-free ring (`CaptureRing`, overwrite-oldest).
  `clip::chooseWindow` picks the last N bars on a bar line from *all* sources (so separately dragged clips line up); `clip::build/write` takes a source mask and makes a
  type-1 file with a named track per source; `clip::importFile` reads a dropped `.mid` into one engine. With the internal chord source the editor does not export from the ring: `clip::render` plays N bars through a private `Engine` from bar 0 and `writeWindow` writes it
  (full length, no playback needed); the ring is only used for Live MIDI exports.

The plugin shell:

- `Parameters.*`: the APVTS layout and `params::Reader`, which caches raw atomic pointers and fills a `seq::Settings` once per block. Per engine: 5 step kinds x 32
  `AudioParameterInt`s (`e1_pitch_01` ... `e4_ratch_32`) plus steps / rate / ratio / octave / mute / solo / arp / channel; chords use `c1_deg`, `c1_ext` ... and `chord_*`.
  Adding an engine option means: layout entry, `Reader` pointer + read, field in `Settings`, use in the engine. Choice params store an index (the `Reader` converts octave
  and channel), so changing a choice list changes saved sessions.
- `PluginProcessor.*`: builds a `seq::Transport` from the host playhead (ppq, or seconds x bpm as a fallback), swaps the incoming MIDI into the engine as input, and the engine
  writes the output into the host buffer. Declared as a synth with MIDI in and out. The stereo audio output is silent unless AUDITION is on, in which case `audition::Player` renders the engine's output MIDI
  (each channel with the sound of the part that sends on it).
- `PluginEditor.*`: the editor draws on a fixed 1240 x 896 `Content` canvas that is scaled to the window (so child bounds are in canvas units). It owns the top bar, the settings drawer, the chord strip, one slim `EngineRow` per engine and the
  per-engine header controls (only the selected engine's are visible). Other pieces: `PatternEditor.*` (the lane stack: painting, mouse editing, per-lane len/div fields; it is the only place that edits step parameters), `ChordStrip.*` (named chord blocks
  via `chords::label`), `EditorWidgets.h` (combo/knob/toggle fields, `NoteRules`, `MaskToggles`, `DragButton`), `EmberLookAndFeel.*` (palette and look ported from Spectralis), `Audition.*` (the sample player; samples are in `Assets/audition`, made by
  `tools/make_audition_samples.py` and embedded through `EmberlineAssets`) and `Pattern.h` (pattern slot text; `params::PatternBank` keeps slots A to D in the state tree, and switching slots writes the stored pattern into the live parameters).
  A 30 Hz timer reads the playheads (`Engine::playheadStep(engine, lane)`) and the playing chord (atomics) and refreshes the widgets. The editor is also a `DragAndDropContainer` and `FileDragAndDropTarget`; `DragButton`
  (`performExternalDragDropOfFiles`) is used for the six export handles, and a click without a drag opens a save dialog. The suggest button runs `chords::suggest` on the message thread and writes the result into the parameters.

`EmberlinePolyTool selftest x` exercises the pieces that need a real processor (pattern slots, preset apply, saved state, the pattern editor's mouse editing, the audition sampler); CI runs it after the engine tests.
