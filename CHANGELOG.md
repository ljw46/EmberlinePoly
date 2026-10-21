# Changelog

Versions follow `MAJOR.MINOR.PATCH`. The number lives in one place: `project(... VERSION x.y.z)` in `CMakeLists.txt`
(the release stage label is `EMBER_VERSION_LABEL`). Release tags look like `v0.1.0-beta.1`.

## Unreleased (0.1.0 pre-beta)

* Four independent motif engines, each with its own step count (1-32), rate, `n:d` ratio, octave, mute / solo, note-pick mode and MIDI channel.
  Per step: pitch, velocity, gate, probability and ratchet.
* Chord buffer with five note-pick modes (As Played, Chord Hold, Low & High, Random, Root), fed by an internal progression or by held MIDI notes.
* Internal chord memory (up to 8 chords, triads to 9ths and suspended colours), chord timing on the beat / anticipated / off-beat, and a **SUGGEST** button for
  Melodic House, Progressive and Deep Techno progressions.
* Scale & chord guard: eight scales, Scale Lock or direct override, a borrow scale with a probability, and mod-wheel borrow.
* Humanize: Gaussian micro-timing jitter up to 25 ms and gate slip.
* Multi-channel MIDI routing: base chords and each engine send on their own channel.
* Drag-to-DAW: separate handles for the chords, each engine and a multi-track file, rendered offline from bar 0 at the full Drag Length (Live MIDI exports what was played);
  a plain click saves the clip. Drop a `.mid` file on an engine to load it.
* JSON presets with a factory library, a **PRESETS** menu (factory, user folder, save / load file) and a **DICE** button per engine. A preset stores the chord source;
  the progression is optional. Routing is never part of a preset.
* With a host timeline the tempo box shows the host tempo and the no-host controls are disabled. The chord source selector moved to the scale panel, and Live MIDI greys out the
  progression, chord timing / output and the CHORDS export handle.
* VST3, Audio Unit (macOS) and Standalone builds for Windows and macOS, with an internal clock when there is no host transport.
