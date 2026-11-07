; Inno Setup script for the Vassian compiler.
; Build with:  iscc /DAppVersion=0.1.0 /DSourceDir=..\dist installer\vassian.iss
; The resulting installer is written to SourceDir (..\dist by default).

#define AppName "Vassian"
#define AppPublisher "VassiaQT"
#define AppExeName "vassian.exe"

#ifndef AppVersion
  #define AppVersion "0.0.1"
#endif

#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{7C4B2F1A-9E3D-4A6B-8F5C-1D2E3A4B5C6D}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/VassiaQT/Vassian
AppSupportURL=https://github.com/VassiaQT/Vassian/issues
AppUpdatesURL=https://github.com/VassiaQT/Vassian/releases/latest
DefaultDirName={localappdata}\Programs\Vassian
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir={#SourceDir}
OutputBaseFilename=vassian-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
ChangesEnvironment=yes
UninstallDisplayIcon={app}\{#AppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\vassian.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LLVM-C.dll"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Check: NeedsAddPath('{app}')

[Code]
function NeedsAddPath(Param: string): Boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKCU, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    Exit;
  end;
  Result := Pos(';' + Uppercase(Param) + ';', ';' + Uppercase(OrigPath) + ';') = 0;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    MsgBox('Vassian was installed and added to your PATH.' + #13#10 + #13#10 +
           'Open a NEW terminal and run:' + #13#10 +
           '    vassian --version' + #13#10 + #13#10 +
           'Compiling to a native .exe also requires clang on your PATH.',
           mbInformation, MB_OK);
end;
