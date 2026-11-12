; Windows installer (Inno Setup). Built by CI: iscc installer.iss
[Setup]
AppName=Undertow
AppVersion={#Copy(GetEnv('GITHUB_REF_NAME'), 2)}
AppPublisher=FL Plugins
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Shows the license before installing, and adds Undertow to Windows' "Installed apps" so it can be removed there
LicenseFile=LICENSE
UninstallDisplayName=Undertow
UninstallFilesDir={commonpf64}\Undertow
OutputDir=.
OutputBaseFilename=Undertow-Windows-Installer

[Files]
Source: "build\Undertow_artefacts\Release\VST3\Undertow.vst3\*"; DestDir: "{commoncf64}\VST3\Undertow.vst3"; Flags: recursesubdirs ignoreversion
