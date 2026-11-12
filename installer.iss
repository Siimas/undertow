; Windows installer (Inno Setup). Built by CI: iscc installer.iss
[Setup]
AppName=Undertow
AppVersion={#GetEnv('GITHUB_REF_NAME')}
AppPublisher=FL Plugins
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; ponytail: no uninstaller; to remove, delete Undertow.vst3 from the VST3 folder
Uninstallable=no
OutputDir=.
OutputBaseFilename=Undertow-Windows-Installer

[Files]
Source: "build\Undertow_artefacts\Release\VST3\Undertow.vst3\*"; DestDir: "{commoncf64}\VST3\Undertow.vst3"; Flags: recursesubdirs ignoreversion
