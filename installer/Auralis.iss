; Inno Setup script: installs the app and the VST3 plug-in.
#define AppName "Auralis"
#define AppVersion "1.0.0"

[Setup]
AppId={{6D7B2E4A-3C55-4B5E-9F0B-0A0A11500001}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Dj Maksy
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputBaseFilename=Auralis-{#AppVersion}-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
SetupIconFile=
UninstallDisplayIcon={app}\Auralis.exe

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "app";  Description: "Auralis by Dj Maksy (standalone app)"; Types: full compact custom; Flags: fixed
Name: "vst3"; Description: "VST3 plug-in"; Types: full custom

[Files]
Source: "..\dist\Auralis.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "..\dist\Auralis.vst3\*"; DestDir: "{commoncf64}\VST3\Auralis.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Auralis"; Filename: "{app}\Auralis.exe"
Name: "{autodesktop}\Auralis"; Filename: "{app}\Auralis.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Run]
Filename: "{app}\Auralis.exe"; Description: "{cm:LaunchProgram,Auralis}"; Flags: nowait postinstall skipifsilent
