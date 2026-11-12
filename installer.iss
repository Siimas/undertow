; Windows installer (Inno Setup). Built by CI: iscc installer.iss
[Setup]
AppName=Pump
AppVersion={#GetEnv('GITHUB_REF_NAME')}
AppPublisher=FL Plugins
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; ponytail: no uninstaller; to remove, delete Pump.vst3 from the VST3 folder
Uninstallable=no
OutputDir=.
OutputBaseFilename=Pump-Windows-Installer

[Files]
Source: "build\Pump_artefacts\Release\VST3\Pump.vst3\*"; DestDir: "{commoncf64}\VST3\Pump.vst3"; Flags: recursesubdirs ignoreversion
