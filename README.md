# Duster

A MIDI kick synth (AU + VST3, macOS 12+) for hard techno kicks. Play a note and it makes the whole
kick. The ringy, noisy top end is crushed out of the kick itself, never layered on top.

Work in progress.

## Build

Needs CMake 3.25+, Ninja and Xcode's command line tools. JUCE and Catch2 are fetched by CPM.

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Other presets: `asan` (tests under ASan + UBSan), `release` (installs the plugin and runs
pluginval and auval), `dist` (universal build for `packaging/build-pkg.sh`).
