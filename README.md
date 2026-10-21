# Emberline Poly

[![build](https://github.com/ljw46/EmberlinePoly/actions/workflows/build.yml/badge.svg)](https://github.com/ljw46/EmberlinePoly/actions/workflows/build.yml)
[![release](https://img.shields.io/github/v/release/ljw46/EmberlinePoly?include_prereleases&label=release)](https://github.com/ljw46/EmberlinePoly/releases/latest)
[![licence: AGPL-3.0](https://img.shields.io/badge/licence-AGPL--3.0-blue)](LICENSE)
![platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20macOS-lightgrey)
![formats](https://img.shields.io/badge/formats-VST3%20%7C%20AU%20%7C%20Standalone-555)
[![Buy me a coffee](https://img.shields.io/badge/buy%20me%20a%20coffee-emberlinesky-ffdd00?logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/emberlinesky)

A polymetric and polyrhythmic MIDI generator for melodic, deep and progressive house and techno. Four independent motif
engines each have their own pattern length and clock rate, play from a shared chord buffer, and send on their own MIDI
channel, so short patterns slide against each other over a chord progression. A genre-aware chord generator writes the
progression, and every part can be dragged into the DAW as its own clip. VST3 and Audio Unit plugin plus Standalone app, built
with [JUCE](https://juce.com) 8.

> **Status:** pre-beta. Expect changes, and please report anything odd.

## Download

Grab the latest build from the [**Releases page**](https://github.com/ljw46/EmberlinePoly/releases/latest): download the zip for your
computer (**Windows** or **Mac**), unzip it and follow `INSTALL.txt` inside.

Emberline Poly makes **MIDI, not sound**. Put it on an instrument track and send its MIDI output to a synth.

## How it works

```
                        chord memory: internal progression  /  held MIDI notes (live)
                                              │  sorted note buffer
        ┌──────────────┬──────────────┬───────┴──────┬──────────────┐
   Engine 1       Engine 2       Engine 3       Engine 4        Base chords
   own steps      own steps      own steps      own steps       (internal source)
   own rate       own rate       own rate       own rate
   note pick      note pick      note pick      note pick
        └──────────────┴──────────────┴──────┬───────┴──────────────┘
                     scale & chord guard  ──►  humanize  ──►  MIDI out, one channel per part
                                                            └─►  capture for Drag-to-DAW
```

### Motif engines

Each of the four engines is a complete voice with 1 to 32 steps. Every step has seven values, each in its own **lane**:

| Lane | Range | Notes |
|---|---|---|
| **Pitch** | -24 to +24 semitones | Shift from the note picked out of the chord buffer (the roll shows -12 to +12). |
| **Velocity** | 0 to 127 | 0 is a rest. This lane is the engine's rhythm. |
| **Gate** | 10 to 200 % | Length as a percentage of the step; over 100 % overlaps the next note. |
| **Probability** | 0 to 100 % | Chance that the step plays. |
| **Ratchet** | 1 to 4 | Splits the step into that many equal retriggers, each a little softer. |
| **Late** | -50 to +50 % | Moves the step early or late by that share of the step: swing, push and drag, step by step. |
| **Harmony** | -12 to +12 semitones | Adds a second note that far from the first (0 is off). |

And every engine has its own **step count** (polymeter), **rate** (1/1 to 1/32, with triplets and dotted values) plus an `n:d`
**ratio** from 1 to 16 (polyrhythm: `5:4` plays a quarter faster, `3:4` at three quarters speed), an **octave** shift of -2 to +2, **mute**
and **solo**, and its **MIDI channel**. Step positions are calculated from the host's quarter-note position, so engines stay locked to the
timeline through loops, jumps and tempo changes. With no host transport (the Standalone app) an internal clock runs instead.

**Every lane can have its own length and clock divide.** Drag the `len` number on a lane's label to give it its own length (double-click puts it back on the
engine's step count), and the `/n` number to make the lane advance once every n steps. A 5-step velocity lane against a 3-step pitch lane gives a pattern that only repeats every 15 steps,
and the highlighted column moves through each lane at its own pace.

**Pitch moves notes** (eight small numbers in the engine header) chooses which chord notes the pitch lane shifts, counting from the lowest. Turn off 1 and 3 and only the middle of the chord moves.

### The editor

A selected engine is edited in the large pattern view; the four slim rows above it show every engine's velocity pattern with its own moving playhead, and clicking a row selects it.

* Click or drag across a lane to paint values. Pitch is a piano roll. Double-click or right-click a step to put it back to its default.
* The **ON / OFF** row switches steps on and off without losing their velocity, and the green bar under it sets the step count. Steps 17 to 32 are on the second page (click the page label).
* **Pattern slots A to D** on each engine hold variations. The first time you pick a slot it starts as a copy of the current pattern; shift-click a slot to copy it to another slot or clear it. Slots are saved with the project.
* The **chord strip** shows the progression as named blocks ("Am7", "Fmaj7"). Click a block to change its chord or colour, duplicate or remove it, and use the **+** to add one. The playing chord is lit.
* **Settings** (top right) holds the borrow scale, humanize, chord timing, the internal clock and **note rules**.

**Note rules** are a one-octave piano: click a key to cycle *play*, *block*, *a semitone up* or *a semitone down* for that pitch class. They are applied to every generated note after the scale guard, so a note you do not want is
simply never played, or is nudged to one you do.

### Note pick (chord buffer)

On every trigger an engine picks from the **buffer**, which holds the chord currently sounding, sorted by pitch:

| Mode | What it does |
|---|---|
| **As Played** | Cycles through the buffer one note per trigger. |
| **Chord Hold** | Plays the whole chord on every trigger (polyrhythmic stabs). |
| **Low & High** | Alternates between the lowest and the highest note. |
| **Random** | Picks a random note from the buffer. |
| **Root** | Always the lowest note (handy for bass lines and imported melodies). |

The buffer comes from the **chord source**: **Internal** (the progression below) or **Live MIDI** (the notes you hold, or a chord clip on
the host's MIDI track). In Live mode, nothing held means nothing plays.

### Chords and the genre-aware generator

The internal progression has up to 8 chords. Each is a scale degree (I to VII) built by stacking tones of the current scale, with a colour:
Triad, 7th, 9th, Sus2, Sus4 or 7sus4. Chords can last 1/2, 1, 2 or 4 bars, and their changes can land **on the beat**, **anticipated**
(an eighth early) or **off the beat** (an eighth late). The chords are sent as MIDI on their own channel.

**SUGGEST** writes a new progression, and sets the scale, for the chosen genre:

* **Melodic House**: i-VI-III-VII style loops in Natural Minor or Dorian, with 7ths and 9ths on the tonic.
* **Progressive**: slower minor-key movement, mixing triads, 7ths and suspended colours (Natural Minor, Dorian, sometimes Harmonic Minor).
* **Deep Techno**: static or two-chord vamps in Phrygian or Dorian, with sus2, sus4 and 7sus4 colours.

### Scale & chord guard

* **Scale Lock** on: every note an engine plays is snapped to the chosen scale (Major, Natural Minor, Dorian, Phrygian, Lydian, Mixolydian,
  Minor Pentatonic, Harmonic Minor). Off is direct override: notes are exactly the chord plus the step's shift.
* **Modal / borrowed chords**: pick a **borrow scale** and an amount, and that share of triggers snap to the borrowed scale instead. Push the
  **mod wheel** above half to borrow on every trigger.

### Humanize

* **Jitter**: Gaussian micro-timing drift, 0 to 25 ms early or late. Outliers are clamped, so the groove never leaves the grid. The notes of a chord move together.
* **Gate slip**: random variation of each note's length.

### Presets and dice

**PRESETS** (next to the export handles) opens the preset menu: the factory library grouped by genre (most use one or two engines, the rest are muted),
**My presets**, **Save preset...**, **Load from file...**, **Show presets folder** and **Roll all engines**.

A preset is a JSON file (see `Source/Preset.h` for the format). It stores the engines' patterns, octave and note pick, the scale and borrow settings,
the humanize amounts and the **chord source**. The chord progression is optional: choose *With / Without the chord progression* when saving, and untick
*Presets also set the chord progression* in the menu to keep your own chords when loading. MIDI channels, mute/solo, tempo and drag length are never stored, so your routing
survives a preset change. Engines a preset does not use are muted. User presets live in `Emberline/Poly/Presets` under your application data folder.

**DICE** on each engine rolls a new pattern (step count, rate, ratio and the five step lanes). The roll is shaped per role (bass, lead, pad, accent): the first step always
sounds, rests and probability dips are limited, the bass never ratchets past two, and step counts avoid the other engines' lengths. Octave, note pick and MIDI channel stay as they are.

### Audition

Emberline Poly makes MIDI, but **AUDITION** (bottom right) plays the generated notes through a small built-in sampler, so you can hear a pattern before sending it anywhere. Each part has its own sound:
Pluck, Bass, Pad, Keys, Bell or Off (the engines choose theirs in their header, the chords in Settings). It is off by default, and the audio output is silent otherwise. Turn it off when the MIDI goes to a real instrument.

The sounds are samples rendered by `tools/make_audition_samples.py` and compiled into the plugin, so they carry no third-party licence and cost next to no CPU: playback is plain resampling.
`EmberlinePolyTool audition out.wav` bounces the current patterns through them to a WAV file.

### MIDI routing

Each part sends on its own channel (1 to 16), so one instance can drive several instruments. Defaults: base chords on channel 1, engines 1 to 4
on channels 2 to 5. In a DAW, route the plugin's MIDI output to the instruments (see `INSTALL.txt`) and filter each instrument to its channel.

### Drag-to-DAW

With the **Internal** chord source, a handle exports a clip that is rendered offline from bar 0, for as many bars as **Drag Length** says (1 to 32). It starts on the
first chord of the progression, is always the full length, and does not need the transport to have played. Each render rolls probability and humanize fresh, so
the clip is one take of the pattern, not a recording of what you heard.

* **CHORDS**: the chord progression as a polyphonic clip.
* **ENGINE 1 to 4**: one engine's motif (exported even if the engine is muted).
* **ALL TRACKS**: a single type-1 MIDI file with a named track per unmuted part (respecting solo), each on its own channel.

All handles share the same bar window, so the clips line up when you drop them. A plain click saves a handle's clip to a file instead, which is the way to go when a host
does not accept the drag (FL Studio's Patcher, for one).

With the **Live MIDI** chord source the engines depend on the notes you play, so the export is instead what the plugin generated while the transport was running
(the last 1 to 32 bars, starting on a bar line; starting the transport again clears it). The CHORDS handle is disabled in this mode.

Going the other way, drop a `.mid` file on an engine to load its first 32 notes into that engine's pitch, velocity and gate steps.

## Build

Requires CMake 3.22+ and a C++20 compiler. JUCE is fetched automatically.

**Windows** (Visual Studio 2022 Build Tools):

    scripts\build-windows.bat

**Linux / macOS:**

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target EmberlinePoly_Standalone EmberlinePoly_VST3

On macOS add `EmberlinePoly_AU` for the Audio Unit. The VST3 ends up in `build/EmberlinePoly_artefacts/Release/VST3/`; copy
it to your VST3 folder (Windows: `C:\Program Files\Common Files\VST3`).

### Tests and tools

    cmake --build build --target EmberlinePolyTests
    build/EmberlinePolyTests_artefacts/Release/EmberlinePolyTests

The tests cover the scale and rate maths, the chord generator, the note-pick modes, multi-engine polymeter and polyrhythm, channel routing, humanization, and multi-track MIDI export. They need no
display or audio device.

`EmberlinePolyTool` (`snapshot out.png [engine] [settings]`, `render out.mid`, `audition out.wav`, `presets dir`, `selftest x`) renders the editor to an image and bounces a few bars of generated multi-track MIDI; `presets dir` writes the factory presets out as JSON.

## Licence

* **Source code:** [GNU AGPL-3.0](LICENSE).
* **Artwork:** [royalty-free EULA](LICENSE-CONTENT.md). MIDI you generate is yours.

Copyright (c) 2026 Liam Walsh.
