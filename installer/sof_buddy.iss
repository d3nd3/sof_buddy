; Generated from features/FEATURES.txt. Do not edit by hand.
#define AppVersion "8.9"

[Setup]
AppId={{B0F7F5D4-4A1E-4F4A-A7B4-8CF2E7C9B1A6}}
AppName=SoF Buddy
AppVersion={#AppVersion}
AppPublisher=d3nd3
DefaultDirName={autopf}\Soldier of Fortune
DefaultGroupName=SoF Buddy
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
PrivilegesRequired=admin
OutputDir=output
OutputBaseFilename=sof_buddy_setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Types]
Name: "full"; Description: "Recommended feature set"
Name: "custom"; Description: "Custom feature selection"; Flags: iscustom

[Components]
Name: "features"; Description: "SoF Buddy features"; Types: full custom; Flags: fixed
Name: "windows_compatibility"; Description: "Windows compatibility"; Types: full custom
Name: "windows_compatibility\appcompat_fix"; Description: "Ensure Windows 10+ Application Compatibility Fix Applied"; Types: full custom
Name: "game_options"; Description: "Game options"; Types: full custom
Name: "game_options\full_violence"; Description: "Enable full violence"; Types: full custom; Flags: notselected
Name: "features\core_features_always_enabled"; Description: "Core Features (always enabled)"; Types: full custom; Flags: fixed
Name: "features\core_features_always_enabled\media_timers"; Description: "Media Timers"; Types: full custom; Flags: fixed
Name: "features\graphics_features"; Description: "Graphics Features"; Types: full custom
Name: "features\graphics_features\texture_mapping_min_mag"; Description: "Texture Mapping Min Mag"; Types: full custom
Name: "features\graphics_features\scaled_con"; Description: "Scaled Con"; Types: full custom
Name: "features\graphics_features\scaled_hud"; Description: "Scaled Hud"; Types: full custom
Name: "features\graphics_features\scaled_menu"; Description: "Scaled Menu"; Types: full custom; Flags: notselected
Name: "features\graphics_features\hd_textures"; Description: "Hd Textures"; Types: full custom
Name: "features\graphics_features\vsync_toggle"; Description: "Vsync Toggle"; Types: full custom
Name: "features\graphics_features\lighting_blend"; Description: "Lighting Blend"; Types: full custom
Name: "features\game_features"; Description: "Game Features"; Types: full custom
Name: "features\game_features\teamicons_offset"; Description: "Teamicons Offset"; Types: full custom
Name: "features\game_features\entity_visualizer"; Description: "Entity Visualizer"; Types: full custom; Flags: notselected
Name: "features\network_features"; Description: "Network Features"; Types: full custom
Name: "features\network_features\http_maps"; Description: "Http Maps"; Types: full custom
Name: "features\menu_features"; Description: "Menu Features"; Types: full custom
Name: "features\menu_features\internal_menus"; Description: "Internal Menus"; Types: full custom
Name: "features\bug_fixes"; Description: "Bug fixes"; Types: full custom
Name: "features\bug_fixes\new_system_bug"; Description: "New System Bug"; Types: full custom
Name: "features\bug_fixes\console_protection"; Description: "Console Protection"; Types: full custom
Name: "features\bug_fixes\cl_maxfps_singleplayer"; Description: "Cl Maxfps Singleplayer"; Types: full custom
Name: "features\bug_fixes\cbuf_limit_increase"; Description: "Cbuf Limit Increase"; Types: full custom; Flags: notselected
Name: "features\input_features"; Description: "Input Features"; Types: full custom
Name: "features\input_features\raw_mouse"; Description: "Raw Mouse"; Types: full custom

[Files]
Source: "payload\sof_buddy.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "payload\sof_buddy\*"; DestDir: "{app}\sof_buddy"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\rsrc\win_scripts\patch_windows_compat.ps1"; DestDir: "{app}\sof_buddy"; Components: windows_compatibility\appcompat_fix; Flags: ignoreversion

[UninstallDelete]
Type: files; Name: "{app}\sof_buddy\features.cfg"

[Code]
const
  SoFRegistryKey = 'Software\Raven Software\SoF';
  FullViolencePassword = 'sof';
  LcgA = 214013;
  LcgC = 2531011;
  LcgModulus = 4294967296L;

function GetVolumeInformationW(RootPathName, VolumeNameBuffer: string; VolumeNameSize: Cardinal;
  var VolumeSerialNumber, MaximumComponentLength, FileSystemFlags: Cardinal;
  FileSystemNameBuffer: string; FileSystemNameSize: Cardinal): Boolean;
external 'GetVolumeInformationW@kernel32.dll stdcall';

function GetGameVolumeSerial(var Serial: Cardinal): Boolean;
var
  Drive, VolumeName, FileSystemName: String;
  MaxComponentLength, FileSystemFlags: Cardinal;
begin
  Drive := ExtractFileDrive(ExpandConstant('{app}'));
  if Drive = '' then begin Result := False; exit; end;
  Drive := AddBackslash(Drive);
  SetLength(VolumeName, 260);
  SetLength(FileSystemName, 260);
  Result := GetVolumeInformationW(Drive, VolumeName, 260, Serial,
    MaxComponentLength, FileSystemFlags, FileSystemName, 260);
end;

function LcgOutput(Seed: Cardinal; N: Integer): Cardinal;
var
  State: Int64;
  I: Integer;
begin
  State := Seed;
  for I := 0 to N do
    State := (State * LcgA + LcgC) mod LcgModulus;
  Result := Cardinal((State shr 16) and $FFFF);
end;

function DwordFromWords(LowWord, HighWord: Cardinal): Cardinal;
begin
  Result := Cardinal(Int64(HighWord) * 65536 + LowWord);
end;

function EncodeFullViolencePassword(Key: Cardinal): AnsiString;
var
  I, Plain, Ob: Integer;
  Hash: Int64;
begin
  Result := '';
  Hash := 0;
  for I := 0 to Length(FullViolencePassword) do begin
    if I = Length(FullViolencePassword) then Plain := 0
    else Plain := Ord(FullViolencePassword[I + 1]);
    Ob := (Plain + Integer((Int64(Key) shr ((I and 7) * 8)) and $FF)) and $FF;
    Result := Result + AnsiChar(Ob);
    Hash := (Hash * 2 + Ob) mod LcgModulus;
  end;
  Hash := Hash xor Int64(Key);
  for I := 0 to 3 do
    Result := Result + AnsiChar(Integer((Hash shr (I * 8)) and $FF));
end;

function WriteFullViolence(Serial: Cardinal): Boolean;
var
  Key: Cardinal;
  Blob: AnsiString;
begin
  Key := DwordFromWords(LcgOutput(Serial, 24), LcgOutput(Serial, 25));
  Blob := EncodeFullViolencePassword(Key);
  if not RegWriteBinaryValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Server', Blob) then begin Result := False; exit; end;
  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Graphics', DwordFromWords(LcgOutput(Serial, 4), LcgOutput(Serial, 5))) then begin Result := False; exit; end;
  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Sound', DwordFromWords(LcgOutput(Serial, 8), LcgOutput(Serial, 9))) then begin Result := False; exit; end;
  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Input', DwordFromWords(LcgOutput(Serial, 12), LcgOutput(Serial, 13))) then begin Result := False; exit; end;
  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Networking', DwordFromWords(LcgOutput(Serial, 16), LcgOutput(Serial, 17))) then begin Result := False; exit; end;
  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Performance', DwordFromWords(LcgOutput(Serial, 20), LcgOutput(Serial, 21))) then begin Result := False; exit; end;
  Result := RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Optimizations', 0);
end;

procedure ApplyFullViolence;
var
  Serial: Cardinal;
begin
  if not GetGameVolumeSerial(Serial) then begin
    MsgBox('Could not read the volume serial for the selected SoF folder; full violence was not applied.',
      mbError, MB_OK);
    exit;
  end;
  if not WriteFullViolence(Serial) then
    MsgBox('Could not write the SoF parental-control registry values.', mbError, MB_OK);
end;

procedure AddFeatureLine(var Config: String; const Name, Component: String);
begin
  if WizardIsComponentSelected(Component) then
    Config := Config + Name + #13#10
  else
    Config := Config + '// ' + Name + #13#10;
end;

procedure ApplyCompatibilityFix;
var
  ResultCode: Integer;
  ScriptPath, Parameters: String;
begin
  ScriptPath := ExpandConstant('{app}\sof_buddy\patch_windows_compat.ps1');
  Parameters := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + ScriptPath +
    '" -ExecutablePath "' + ExpandConstant('{app}\SoF.exe') + '"';
  if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
    MsgBox('The Windows 10+ compatibility patch failed. Check the SoF.exe version and backup before retrying.',
      mbError, MB_OK);
end;

procedure WriteFeatureConfig;
var
  Config: String;
begin
  ForceDirectories(ExpandConstant('{app}\sof_buddy'));
  Config := '// Generated by SoF Buddy Setup.' + #13#10 +
            '// Active lines are enabled; // lines are disabled.' + #13#10#13#10;
  AddFeatureLine(Config, 'media_timers', 'features\core_features_always_enabled\media_timers');
  AddFeatureLine(Config, 'texture_mapping_min_mag', 'features\graphics_features\texture_mapping_min_mag');
  AddFeatureLine(Config, 'scaled_con', 'features\graphics_features\scaled_con');
  AddFeatureLine(Config, 'scaled_hud', 'features\graphics_features\scaled_hud');
  AddFeatureLine(Config, 'scaled_menu', 'features\graphics_features\scaled_menu');
  AddFeatureLine(Config, 'hd_textures', 'features\graphics_features\hd_textures');
  AddFeatureLine(Config, 'vsync_toggle', 'features\graphics_features\vsync_toggle');
  AddFeatureLine(Config, 'lighting_blend', 'features\graphics_features\lighting_blend');
  AddFeatureLine(Config, 'teamicons_offset', 'features\game_features\teamicons_offset');
  AddFeatureLine(Config, 'entity_visualizer', 'features\game_features\entity_visualizer');
  AddFeatureLine(Config, 'http_maps', 'features\network_features\http_maps');
  AddFeatureLine(Config, 'internal_menus', 'features\menu_features\internal_menus');
  AddFeatureLine(Config, 'new_system_bug', 'features\bug_fixes\new_system_bug');
  AddFeatureLine(Config, 'console_protection', 'features\bug_fixes\console_protection');
  AddFeatureLine(Config, 'cl_maxfps_singleplayer', 'features\bug_fixes\cl_maxfps_singleplayer');
  AddFeatureLine(Config, 'cbuf_limit_increase', 'features\bug_fixes\cbuf_limit_increase');
  AddFeatureLine(Config, 'raw_mouse', 'features\input_features\raw_mouse');
  if not SaveStringToFile(ExpandConstant('{app}\sof_buddy\features.cfg'), Config, False) then
    MsgBox('Could not save the selected feature settings.', mbError, MB_OK);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectDir then
    if not FileExists(AddBackslash(WizardDirValue) + 'SoF.exe') then begin
      MsgBox('Choose the Soldier of Fortune folder containing SoF.exe.', mbError, MB_OK);
      Result := False;
    end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
    WriteFeatureConfig;
    if WizardIsComponentSelected('game_options\full_violence') then
      ApplyFullViolence;
    if WizardIsComponentSelected('windows_compatibility\appcompat_fix') then
      ApplyCompatibilityFix;
  end;
end;
