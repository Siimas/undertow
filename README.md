# Pump

Tempo-synced volume ducker (sidechain pump without the sidechain). VST3, Windows and macOS (Intel + Apple Silicon).

Controls: **Depth** (how far it ducks), **Rate** (1/8, 1/4, 1/2, 1 bar), **Curve** (how fast the volume comes back).

## Install

Download the installer for your system from [Releases](../../releases/latest) and run it.

- **Windows** — `Pump-Windows-Installer.exe`. The installer is unsigned, so SmartScreen shows a warning: click **More info → Run anyway**.
- **macOS** — `Pump-macOS-Installer.pkg`. The installer is unsigned, so right-click it → **Open** (on newer macOS: System Settings → Privacy & Security → **Open Anyway**).

Then in FL Studio: Options → File → Manage plugins → Find installed plugins. Pump shows up under Effects.

Installed to `C:\Program Files\Common Files\VST3\` (Windows) or `/Library/Audio/Plug-Ins/VST3/` (macOS). To uninstall, delete `Pump.vst3` from there.

## Release a new version

```bash
git tag v0.3.0 && git push --tags
```

GitHub Actions builds both installers and attaches them to the release.
