#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

#ifndef SourceDir
  #define SourceDir "..\..\build"
#endif

#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

[Setup]
AppId={{A4F5D31C-838B-49C6-9E1E-8D9A44CF3B31}
AppName=Hell's Tower
AppVersion={#AppVersion}
AppPublisher=Hell's Tower
DefaultDirName={localappdata}\Programs\HellsTower
DefaultGroupName=Hell's Tower
PrivilegesRequired=lowest
OutputDir={#OutputDir}
OutputBaseFilename=Hells-Tower-Setup-Windows-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=hells_tower.ico
UninstallDisplayIcon={app}\hells_tower.exe

[Tasks]
Name: "desktopicon"; Description: "Creer un raccourci sur le bureau"; GroupDescription: "Raccourcis :"; Flags: unchecked

[Files]
Source: "{#SourceDir}\hells_tower.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\assets\*"; DestDir: "{app}\assets"; Excludes: "video.mp4"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Hell's Tower"; Filename: "{app}\hells_tower.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Hell's Tower"; Filename: "{app}\hells_tower.exe"; WorkingDir: "{app}"; Tasks: desktopicon
