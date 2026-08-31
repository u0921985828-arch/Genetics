; Inno Setup script for CHRONA (Windows VST3 installer).
; Build:  ISCC.exe packaging\windows\CHRONA.iss
; Sign the resulting installer separately with signtool if you have a cert.

#define AppName "CHRONA"
#define AppVersion "1.0.0"
#define Publisher "ARTiFACTS"

[Setup]
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
DefaultDirName={autopf}\Common Files\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=CHRONA-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin

[Files]
; VST3 is a bundle folder; recurse it into the shared VST3 directory.
Source: "..\..\build\CHRONA_artefacts\Release\VST3\CHRONA.vst3\*"; \
  DestDir: "{autopf}\Common Files\VST3\CHRONA.vst3"; \
  Flags: recursesubdirs createallsubdirs ignoreversion
; Factory preset banks → the user's CHRONA preset directory (shown grouped as
; "Bank / Preset" in the plug-in's browser).
Source: "..\..\presets\*"; \
  DestDir: "{userappdata}\CHRONA\Presets"; \
  Excludes: "README.md"; \
  Flags: recursesubdirs createallsubdirs

[Run]
; nothing to launch; DAWs rescan on next start.
