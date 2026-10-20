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

Each of the four engines is a complete voice with 1 to 32 steps. Every step has:

| Value | Range | Notes |
|---|---|---|
| **Pitch** | -24 to +24 semitones | Shift from the note picked out of the chord buffer. |
| **Velocity** | 0 to 127 | 0 is a rest. |
| **Gate** | 10 to 200 % | Length as a percentage of the step; over 100 % overlaps the next note. |
| **Probability** | 0 to 100 % | Chance that the step plays. |
| **Ratchet** | 1 to 4 | Splits the step into that many equal retriggers, each a little softer. |

And every engine has its own **step count** (polymeter), **rate** (1/1 to 1/32, with triplets and dotted values) plus an `n:d`
**ratio** from 1 to 16 (polyrhythm: `5:4` plays a quarter faster, `3:4` at three quarters speed), an **octave** shift of -2 to +2, **mute**
and **solo**, and its **MIDI channel**. Step positions are calculated from the host's quarter-note position, so engines stay locked to the
timeline through loops, jumps and tempo changes. With no host transport (the Standalone app) an internal clock runs instead.

In the editor each pad shows its step: height is velocity, width is gate, brightness is probability, dots are ratchets and the number is the
pitch shift. Pick what you are editing with the **VEL / GATE / PITCH / PROB / RATCH** buttons, then click or drag on the pads.

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

### MIDI routing

Each part sends on its own channel (1 to 16), so one instance can drive several instruments. Defaults: base chords on channel 1, engines 1 to 4
on channels 2 to 5. In a DAW, route the plugin's MIDI output to the instruments (see `INSTALL.txt`) and filter each instrument to its channel.

### Drag-to-DAW

The plugin records what it generates while the transport plays. Drag a handle onto a DAW track to drop the last 1 to 32 bars (your choice under
**Drag Length**) as a `.mid` clip, including the humanized timing:

* **CHORDS**: the chord progression as a polyphonic clip.
* **ENGINE 1 to 4**: one engine's motif.
* **ALL TRACKS**: a single type-1 MIDI file with a named track per part, each on its own channel.

All handles use the same bar window, so the clips line up when you drop them. A plain click saves a handle's clip to a file instead.
Starting the transport again clears the capture.

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

`EmberlinePolyTool` (`snapshot out.png`, `render out.mid`) renders the editor to an image and bounces a few bars of generated multi-track MIDI.

## Licence

* **Source code:** [GNU AGPL-3.0](LICENSE).
* **Artwork:** [royalty-free EULA](LICENSE-CONTENT.md). MIDI you generate is yours.

Copyright (c) 2026 Liam Walsh.
