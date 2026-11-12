# Pump

Sidechain-style volume ducking without the sidechain routing. Drop it on a bass, pad or bus and it pumps in time with your track.

VST3 for Windows and macOS (Intel + Apple Silicon).

![Pump](docs/screenshot.png)

## Features

- **16 shapes** — from a classic duck to gates, holds, double and triplet pumps
- **Rates** — 1/16, 1/8, 1/4, 1/2, 1 bar, locked to the host tempo and song position
- **Three triggers** — tempo sync, audio (sidechain input), or MIDI notes
- **Band split** — duck just the bass below an adjustable crossover and leave the top end alone
- **Mix** — from subtle to full duck
- Live curve display with playhead

## Install

Download the installer for your system from [Releases](../../releases/latest) and run it.

| System | File | First run |
|---|---|---|
| Windows | `Pump-Windows-Installer.exe` | SmartScreen warns (unsigned): **More info → Run anyway** |
| macOS | `Pump-macOS-Installer.pkg` | Right-click → **Open**, or System Settings → Privacy & Security → **Open Anyway** |

Then in FL Studio: **Options → File → Manage plugins → Find installed plugins**. Pump appears under Effects.

To uninstall, delete `Pump.vst3` from `C:\Program Files\Common Files\VST3\` (Windows) or `/Library/Audio/Plug-Ins/VST3/` (macOS).

## Controls

| Control | What it does |
|---|---|
| Shape | The ducking curve; browse with the arrows or the menu |
| Rate | Length of one cycle |
| Trigger | **Sync** repeats with the tempo. **Audio** and **MIDI** play the shape once per hit |
| Mix | How deep the duck goes |
| Band split | **Full range** ducks everything; **Low only** ducks what's below the crossover |
| Crossover | Split frequency for band split (40–2000 Hz) |
| Threshold | How loud the sidechain must be to trigger (Audio mode) |

### Triggering in FL Studio

- **Audio** — in the mixer, select the kick's track, right-click the send arrow on Pump's track and choose *Sidechain to this track*. Then in Pump's wrapper settings (gear icon → Processing tab) set the sidechain input to that source.
- **MIDI** — in Pump's wrapper settings set a MIDI input port, add a *MIDI Out* channel on the same port, and send it notes.

## Build from source

Needs CMake 3.22+ and a C++17 compiler (Xcode command line tools or Visual Studio). JUCE is downloaded automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --config Release
```

```bash
ctest --test-dir build -C Release
```

The plugin lands in `build/Pump_artefacts/Release/VST3/`.

## Release

```bash
git tag v1.2.0 && git push --tags
```

GitHub Actions builds, tests, and attaches both installers to a new release.

## Layout

| Path | Contents |
|---|---|
| `Source/Duck.h` | Shape library and gain math |
| `Source/PumpProcessor.h` | Audio processing, triggers, band split |
| `Source/PluginProcessor.cpp` | Interface |
| `Source/test_*.cpp` | Tests |
| `installer.iss` | Windows installer script |
