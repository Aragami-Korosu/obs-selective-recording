; SPDX-License-Identifier: GPL-2.0-or-later
; Windows installer for OBS Selective Recording (Inno Setup 6).
;
; Built by .github/scripts/Package-Windows.ps1:
;   iscc /DProductVersion=1.0.0 /DSourceDir=<plugin folder> /DOutputDir=<dir> /DOutputName=<file> installer.iss

#ifndef ProductVersion
  #define ProductVersion "0.0.0"
#endif
#ifndef SourceDir
  #error SourceDir must point to the packaged plugin folder (bin\64bit and data)
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef OutputName
  #define OutputName "obs-selective-recording-installer"
#endif

#define AppName "OBS Selective Recording"
#define MinObsMajor 31
#define MinObsMinor 1

[Setup]
AppId={{591829E5-00ED-40F0-B4DE-7222089659F7}
AppName={#AppName}
AppVersion={#ProductVersion}
AppVerName={#AppName} {#ProductVersion}
AppPublisher=Aragami Korosu
AppCopyright=Copyright (C) 2026 Aragami Korosu
AppPublisherURL=https://github.com/Linux-bootloader/obs-selective-recording
AppSupportURL=https://github.com/Linux-bootloader/obs-selective-recording/issues
AppUpdatesURL=https://github.com/Linux-bootloader/obs-selective-recording/releases
VersionInfoCompany=Aragami Korosu
UninstallDisplayName={#AppName}
; OBS loads plugins from this folder for every user.
DefaultDirName={commonappdata}\obs-studio\plugins\obs-selective-recording
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\..\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
#ifdef Sign
; The sign tool command is passed in with /Ssigntool=... by the packaging script.
SignTool=signtool
SignedUninstaller=yes
#endif
VersionInfoVersion={#ProductVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} installer

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Messages]
FinishedLabel=Start OBS Studio. The Selective Recording dock opens on the first run, and a recording button appears next to each source in the Sources list.

[Code]
function ObsRunning(): Boolean;
var
  Locator, Service, Processes: Variant;
begin
  Result := False;
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Service := Locator.ConnectServer('.', 'root\CIMV2');
    Processes := Service.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE Name = ''obs64.exe''');
    Result := Processes.Count > 0;
  except
    Result := False;
  end;
end;

// Returns the installed OBS version from its executable, or False if OBS isn't found.
function InstalledObsVersion(var Major, Minor: Cardinal): Boolean;
var
  ObsPath: String;
  MS, LS: Cardinal;
begin
  Result := False;
  if not RegQueryStringValue(HKLM64, 'SOFTWARE\OBS Studio', '', ObsPath) then
    exit;
  if not GetVersionNumbers(AddBackslash(ObsPath) + 'bin\64bit\obs64.exe', MS, LS) then
    exit;
  Major := MS shr 16;
  Minor := MS and $FFFF;
  Result := True;
end;

function InitializeSetup(): Boolean;
var
  Major, Minor: Cardinal;
begin
  Result := True;
  if WizardSilent() then
    exit;

  if not InstalledObsVersion(Major, Minor) then begin
    Result := MsgBox('OBS Studio wasn''t found in its usual location.' + #13#10#13#10 +
      'Selective Recording needs OBS Studio {#MinObsMajor}.{#MinObsMinor} or newer. ' +
      'Continue anyway? (For a portable OBS, use the ZIP package instead.)',
      mbConfirmation, MB_YESNO) = IDYES;
    exit;
  end;

  if (Major < {#MinObsMajor}) or ((Major = {#MinObsMajor}) and (Minor < {#MinObsMinor})) then
    Result := MsgBox(Format('You have OBS Studio %d.%d. Selective Recording needs ' +
      '{#MinObsMajor}.{#MinObsMinor} or newer and won''t load in this version.' + #13#10#13#10 +
      'Install anyway?', [Major, Minor]), mbError, MB_YESNO) = IDYES;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if ObsRunning() then
    Result := 'OBS Studio is running. Close OBS, then click Back and Next to continue.';
end;

function InitializeUninstall(): Boolean;
begin
  Result := True;
  if ObsRunning() then begin
    MsgBox('Close OBS Studio before uninstalling Selective Recording.', mbError, MB_OK);
    Result := False;
  end;
end;
