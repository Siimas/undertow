# Pump

Tempo-synced volume ducker (sidechain pump without the sidechain). VST3, Windows and macOS (Intel + Apple Silicon).

Controls: **Depth** (how far it ducks), **Rate** (1/8, 1/4, 1/2, 1 bar), **Curve** (how fast the volume comes back).

## Install

Download the zip for your system from [Releases](../../releases/latest) and unzip it.

**Windows** — move `Pump.vst3` into `C:\Program Files\Common Files\VST3\`

**macOS** — move `Pump.vst3` into `~/Library/Audio/Plug-Ins/VST3/`, then clear the download quarantine (the plugin is not notarized):

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Pump.vst3
```

**FL Studio** — Options → Manage plugins → Find installed plugins. Pump shows up under Effects.

## Release a new version

```bash
git tag v0.2.0 && git push --tags
```

GitHub Actions builds both platforms and attaches the zips to the release.
