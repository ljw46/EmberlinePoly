# Contributing

Bug reports and pull requests are welcome. When you report a bug, include your DAW, operating system and the version shown
under the title.

Before opening a pull request:

1. Build the tests and run them: `cmake --build build --target EmberlinePolyTests`, then
   `build/EmberlinePolyTests_artefacts/Release/EmberlinePolyTests`. CI runs the same.
2. Keep the engine (`Source/SequencerEngine.*`, `Rates.h`, `Scales.h`, `Humanize.h`, `MidiClip.*`) free of GUI and parameter code
   and real-time safe (no allocation, no locks in `Engine::process`). New engine behaviour needs a test in `tests/EngineTests.cpp`.
3. If you add a parameter, add it in `Source/Parameters.cpp` (layout and `Reader`) and, if the engine uses it, in `Source/Settings.h`.

Contributions to the source code are under AGPL-3.0.
