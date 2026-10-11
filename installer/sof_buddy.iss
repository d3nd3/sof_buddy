; Generated from features/FEATURES.txt. Do not edit by hand.
#define AppVersion "8.20"

[Setup]
AppId={{B0F7F5D4-4A1E-4F4A-A7B4-8CF2E7C9B1A6}}
AppName=SoF Buddy
AppVersion={#AppVersion}
AppPublisher=d3nd3
DefaultDirName={autopf}\Soldier of Fortune
AppendDefaultDirName=no
DisableDirPage=no
DirExistsWarning=no
UsePreviousAppDir=no
DefaultGroupName=SoF Buddy
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
PrivilegesRequired=admin
OutputDir=output
OutputBaseFilename=sof_buddy_setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Messages]
SelectDirDesc=Select the Soldier of Fortune folder
SelectDirLabel3=SoF Buddy will be installed into the selected folder:
SelectDirBrowseLabel=Choose the folder containing SoF.exe, then click Next.

[Types]
Name: "full"; Description: "Recommended feature set"
Name: "custom"; Description: "Custom feature selection"; Flags: iscustom

[Components]
Name: "features"; Description: "SoF Buddy features"; Types: full custom; Flags: fixed
Name: "features\recommended"; Description: "Recommended / tested features"; Types: full custom
Name: "features\recommended\core_performance_features_always_enabled"; Description: "Core / Performance Features"; Types: full custom; Flags: fixed
Name: "features\recommended\core_performance_features_always_enabled\media_timers"; Description: "Media Timers"; Types: full custom; Flags: fixed
Name: "features\recommended\security_fixes_always_enabled"; Description: "Security Fixes"; Types: full custom; Flags: fixed
Name: "features\recommended\security_fixes_always_enabled\console_protection"; Description: "Console Protection"; Types: full custom; Flags: fixed
Name: "features\recommended\bug_fixes"; Description: "Bug Fixes"; Types: full custom
Name: "features\recommended\bug_fixes\new_system_bug"; Description: "New System Bug"; Types: full custom
Name: "features\recommended\bug_fixes\cl_maxfps_singleplayer"; Description: "Cl Maxfps Singleplayer"; Types: full custom
Name: "features\recommended\bug_fixes\list_match_fix"; Description: "List Match Fix"; Types: full custom
Name: "features\recommended\graphics_features"; Description: "Graphics Features"; Types: full custom
Name: "features\recommended\graphics_features\texture_mapping_min_mag"; Description: "Texture Mapping Min Mag"; Types: full custom
Name: "features\recommended\graphics_features\scaled_con"; Description: "Scaled Con"; Types: full custom
Name: "features\recommended\graphics_features\scaled_hud"; Description: "Scaled Hud"; Types: full custom
Name: "features\recommended\graphics_features\hd_textures"; Description: "Hd Textures"; Types: full custom
Name: "features\recommended\graphics_features\vsync_toggle"; Description: "Vsync Toggle"; Types: full custom
Name: "features\recommended\graphics_features\lighting_blend"; Description: "Lighting Blend"; Types: full custom
Name: "features\recommended\gameplay_features"; Description: "Gameplay Features"; Types: full custom
Name: "features\recommended\gameplay_features\teamicons_offset"; Description: "Teamicons Offset"; Types: full custom
Name: "features\recommended\networking_features"; Description: "Networking Features"; Types: full custom
Name: "features\recommended\networking_features\http_maps"; Description: "Http Maps"; Types: full custom
Name: "features\recommended\menu_features"; Description: "Menu Features"; Types: full custom
Name: "features\recommended\menu_features\internal_menus"; Description: "Internal Menus"; Types: full custom
Name: "features\recommended\input_features"; Description: "Input Features"; Types: full custom
Name: "features\recommended\input_features\raw_mouse"; Description: "Raw Mouse"; Types: full custom
Name: "features\optional"; Description: "Un-recommended / unstable features"; Types: custom; Flags: checkablealone
Name: "features\optional\bug_fixes"; Description: "Bug Fixes"; Types: custom
Name: "features\optional\bug_fixes\cbuf_limit_increase"; Description: "Cbuf Limit Increase"; Types: custom
Name: "features\optional\graphics_features"; Description: "Graphics Features"; Types: custom
Name: "features\optional\graphics_features\scaled_menu"; Description: "Scaled Menu"; Types: custom
Name: "features\optional\gameplay_features"; Description: "Gameplay Features"; Types: custom
Name: "features\optional\gameplay_features\entity_visualizer"; Description: "Entity Visualizer"; Types: custom

[Tasks]
Name: "appcompat_fix"; Description: "[OPTIONAL] Apply Windows 10+ Application Compatibility fix (recommended)"; GroupDescription: "Optional installation options:"
Name: "full_violence"; Description: "[OPTIONAL] Unlock full violence (highly recommended if not using SoFPlus spcl.dll)"; GroupDescription: "Optional installation options:"

[Files]
Source: "payload\sof_buddy.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "payload\sof_buddy\*"; DestDir: "{app}\sof_buddy"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\rsrc\win_scripts\patch_windows_compat.ps1"; DestDir: "{app}\sof_buddy"; Tasks: appcompat_fix; Flags: ignoreversion

[UninstallDelete]
Type: files; Name: "{app}\sof_buddy\features.cfg"

[Code]
const
  SoFRegistryKey = 'Software\Raven Software\SoF';
  LcgA = 214013;
  LcgC = 2531011;
  LcgInverse = $B9B33155;
  LcgModulus = 4294967296;
  PatchOffset = $11AD72;

var
  FullViolencePage: TInputQueryWizardPage;
  FullViolenceModePage: TInputOptionWizardPage;
  FullViolenceSerial: Cardinal;

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

function ExpectedParentalValue(Serial: Cardinal; Index: Integer; Censored: Boolean): Cardinal;
var
  FirstOutput: Integer;
begin
  if Index = 6 then FirstOutput := 26
  else FirstOutput := Index * 4;
  if Censored then FirstOutput := FirstOutput + 2;
  Result := DwordFromWords(LcgOutput(Serial, FirstOutput), LcgOutput(Serial, FirstOutput + 1));
end;

function EncodeFullViolencePassword(const Password: AnsiString; Key: Cardinal): AnsiString;
var
  I, Plain, Ob: Integer;
  Hash: Int64;
begin
  Result := '';
  Hash := 0;
  for I := 0 to Length(Password) do begin
    if I = Length(Password) then Plain := 0
    else Plain := Ord(Password[I + 1]);
    Ob := (Plain + Integer((Int64(Key) shr ((I and 7) * 8)) and $FF)) and $FF;
    Result := Result + Chr(Ob);
    Hash := (Hash * 2 + Ob) mod LcgModulus;
  end;
  Hash := Hash xor Int64(Key);
  for I := 0 to 3 do
    Result := Result + Chr(Integer((Hash shr (I * 8)) and $FF));
end;

function WriteFullViolence(Serial: Cardinal; const Password: AnsiString; Censored: Boolean): Boolean;
var
  Key: Cardinal;
  Blob: AnsiString;
begin
  Key := DwordFromWords(LcgOutput(Serial, 24), LcgOutput(Serial, 25));
  Blob := EncodeFullViolencePassword(Password, Key);
  if not RegWriteBinaryValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Server', Blob) then begin Result := False; exit; end;
  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Graphics', ExpectedParentalValue(Serial, 1, Censored)) then begin Result := False; exit; end;
  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Sound', ExpectedParentalValue(Serial, 2, Censored)) then begin Result := False; exit; end;
  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Input', ExpectedParentalValue(Serial, 3, Censored)) then begin Result := False; exit; end;
  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Networking', ExpectedParentalValue(Serial, 4, Censored)) then begin Result := False; exit; end;
  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Performance', ExpectedParentalValue(Serial, 5, Censored)) then begin Result := False; exit; end;
  Result := RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Optimizations', ExpectedParentalValue(Serial, 6, Censored));
end;

function TryParseVolumeSerial(const Value: String; var Serial: Cardinal): Boolean;
var
  HexValue: String;
  LowWord, HighWord: Integer;
begin
  Result := False;
  HexValue := Uppercase(Trim(Value));
  if (Length(HexValue) = 9) and (HexValue[5] = '-') then
    HexValue := Copy(HexValue, 1, 4) + Copy(HexValue, 6, 4);
  if Length(HexValue) <> 8 then exit;
  try
    LowWord := StrToInt('$' + Copy(HexValue, 5, 4));
    HighWord := StrToInt('$' + Copy(HexValue, 1, 4));
  except
    exit;
  end;
  Serial := Cardinal(Int64(HighWord) * 65536 + LowWord);
  Result := True;
end;

function TryParseHexDword(const Value: String; var Number: Cardinal): Boolean;
var
  HexValue: String;
  LowWord, HighWord: Integer;
begin
  Result := False;
  HexValue := Uppercase(Trim(Value));
  if (Length(HexValue) >= 2) and (Copy(HexValue, 1, 2) = '0X') then
    HexValue := Copy(HexValue, 3, Length(HexValue) - 2);
  if Length(HexValue) <> 8 then exit;
  try
    LowWord := StrToInt('$' + Copy(HexValue, 5, 4));
    HighWord := StrToInt('$' + Copy(HexValue, 1, 4));
  except
    exit;
  end;
  Number := Cardinal(Int64(HighWord) * 65536 + LowWord);
  Result := True;
end;

function IsValidViolencePassword(const Password: String): Boolean;
var
  I, Ch: Integer;
begin
  Result := False;
  if Length(Password) > 31 then exit;
  for I := 1 to Length(Password) do begin
    Ch := Ord(Password[I]);
    if (Ch < 32) or (Ch > 126) then exit;
  end;
  Result := True;
end;

function ReverseLcgStep(State: Cardinal): Cardinal;
var
  Value, LowProduct, HighProduct: Int64;
begin
  Value := (Int64(State) - LcgC + LcgModulus) mod LcgModulus;
  LowProduct := (Value and $FFFF) * (LcgInverse and $FFFF);
  HighProduct := (((Value and $FFFF) * (LcgInverse shr 16) +
    (Value shr 16) * (LcgInverse and $FFFF)) and $FFFF) * 65536;
  Result := Cardinal((LowProduct + HighProduct) mod LcgModulus);
end;

function TrySeedFromKey(Key: Cardinal; var Seed: Cardinal; var CandidateCount: Integer): Boolean;
var
  LowWord: Integer;
  KeyLow, KeyHigh, State, NextState, Candidate: Cardinal;
  I: Integer;
begin
  CandidateCount := 0;
  KeyLow := Key and $FFFF;
  KeyHigh := (Key shr 16) and $FFFF;
  for LowWord := 0 to $FFFF do begin
    State := DwordFromWords(LowWord, KeyLow);
    NextState := Cardinal((Int64(State) * LcgA + LcgC) mod LcgModulus);
    if ((NextState shr 16) and $FFFF) = KeyHigh then begin
      Candidate := State;
      for I := 1 to 25 do Candidate := ReverseLcgStep(Candidate);
      Inc(CandidateCount);
      if CandidateCount = 1 then Seed := Candidate;
    end;
  end;
  Result := CandidateCount > 0;
end;

function TryResolveFullViolenceSerial(var Serial: Cardinal): Boolean;
var
  OverrideValue, KeyValue: String;
  Key: Cardinal;
  CandidateCount: Integer;
begin
  OverrideValue := Trim(FullViolencePage.Values[1]);
  if OverrideValue <> '' then begin
    Result := TryParseVolumeSerial(OverrideValue, Serial);
    if not Result then
      MsgBox('Enter the volume serial as XXXX-XXXX, for example 4300-0000.', mbError, MB_OK);
    exit;
  end;
  if GetGameVolumeSerial(Serial) then begin
    Result := True;
    exit;
  end;
  KeyValue := Trim(FullViolencePage.Values[2]);
  if KeyValue = '' then begin Result := False; exit; end;
  if not TryParseHexDword(KeyValue, Key) then begin
    MsgBox('Enter the SoF.exe -cs key as 8 hexadecimal digits.', mbError, MB_OK);
    Result := False;
    exit;
  end;
  Result := TrySeedFromKey(Key, Serial, CandidateCount);
  if Result then begin
    Log(Format('SoF parental settings: key %8.8x recovered %d serial candidate(s); using %8.8x.', [Key, CandidateCount, Serial]));
    if CandidateCount > 1 then Log('The -cs key is ambiguous; enter the exact volume serial to select the correct seed.');
  end;
end;

procedure ApplyFullViolence;
var
  Password: AnsiString;
  Censored: Boolean;
begin
  Password := AnsiString(FullViolencePage.Values[0]);
  Censored := FullViolenceModePage.SelectedValueIndex = 1;
  if not WriteFullViolence(FullViolenceSerial, Password, Censored) then
    MsgBox('Could not write the SoF parental-control registry values.', mbError, MB_OK)
  else
    MsgBox('SoF parental settings were written to this Windows/Wine user registry. Restart SoF, open the console at startup, type userinfo, and confirm cl_violence is 0.', mbInformation, MB_OK);
end;

procedure InitializeWizard;
begin
  FullViolencePage := CreateInputQueryPage(wpSelectTasks, 'OPTIONAL full-violence details',
    'OPTIONAL settings', 'Every field below is optional. Leave fields blank unless you need to override automatic detection.');
  FullViolencePage.Add('[OPTIONAL] Password (0 to 31 ASCII characters)', True);
  FullViolencePage.Values[0] := '';
  FullViolencePage.Add('[OPTIONAL] Volume serial (XXXX-XXXX; blank detects the game drive, then uses the -cs key)', False);
  FullViolencePage.Values[1] := '';
  FullViolencePage.Add('[OPTIONAL] SoF.exe -cs key (8 hex digits; only needed if serial detection fails)', False);
  FullViolencePage.Values[2] := '';
  FullViolenceModePage := CreateInputOptionPage(FullViolencePage.ID, 'OPTIONAL violence level',
    'OPTIONAL settings', 'Choose the optional parental-control state to write.', True, False);
  FullViolenceModePage.Add('Full violence (unlocked)');
  FullViolenceModePage.Add('Censored');
  FullViolenceModePage.SelectedValueIndex := 0;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := ((PageID = FullViolencePage.ID) or (PageID = FullViolenceModePage.ID)) and
    not WizardIsTaskSelected('full_violence');
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
  if RegKeyExists(HKEY_CURRENT_USER, 'Software\Wine') then begin
    Log('Skipping Windows compatibility fix under Wine.');
    exit;
  end;
  ScriptPath := ExpandConstant('{app}\sof_buddy\patch_windows_compat.ps1');
  Parameters := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + ScriptPath +
    '" -ExecutablePath "' + ExpandConstant('{app}\SoF.exe') + '"';
  if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
    MsgBox('The Windows 10+ compatibility patch failed. Check the SoF.exe version and backup before retrying.',
      mbError, MB_OK);
end;

function IsWine: Boolean;
begin
  Result := RegKeyExists(HKEY_CURRENT_USER, 'Software\Wine');
end;

function ApplyWineDllPatch: Boolean;
var
  ExePath, BackupPath: String;
  Data, Link: AnsiString;
  I: Integer;
begin
  Result := False;
  ExePath := ExpandConstant('{app}\SoF.exe');
  BackupPath := ExePath + '.bak';
  if not FileExists(BackupPath) then
    if not FileCopy(ExePath, BackupPath, False) then exit;
  if not LoadStringFromFile(ExePath, Data) then exit;
  Link := 'sof_buddy.dll' + #0;
  if Length(Data) < PatchOffset + Length(Link) then exit;
  for I := 1 to Length(Link) do
    Data[PatchOffset + I] := Link[I];
  Result := SaveStringToFile(ExePath, Data, False);
end;

procedure EnableSoFBuddy;
var
  ResultCode: Integer;
  ScriptPath, Parameters: String;
begin
  if IsWine then begin
    if not ApplyWineDllPatch then
      MsgBox('Could not enable SoF Buddy automatically under Wine. Run sof_buddy/enable_sofplus_and_buddy.sh from the SoF folder.',
        mbError, MB_OK);
    exit;
  end;
  ScriptPath := ExpandConstant('{app}\sof_buddy\enable_sofplus_and_buddy.cmd');
  Parameters := '/C ""' + ScriptPath + '" -NoPause"';
  if not Exec(ExpandConstant('{sys}\cmd.exe'), Parameters,
    ExpandConstant('{app}\sof_buddy'), SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
    MsgBox('Could not enable SoF Buddy automatically. Run enable_sofplus_and_buddy.cmd from the SoF folder.',
      mbError, MB_OK);
end;

procedure WriteFeatureConfig;
var
  Config: String;
begin
  ForceDirectories(ExpandConstant('{app}\sof_buddy'));
  Config := '// Generated by SoF Buddy Setup.' + #13#10 +
            '// Active lines are enabled; // lines are disabled.' + #13#10#13#10;
  AddFeatureLine(Config, 'media_timers', 'features\recommended\core_performance_features_always_enabled\media_timers');
  AddFeatureLine(Config, 'console_protection', 'features\recommended\security_fixes_always_enabled\console_protection');
  AddFeatureLine(Config, 'new_system_bug', 'features\recommended\bug_fixes\new_system_bug');
  AddFeatureLine(Config, 'cl_maxfps_singleplayer', 'features\recommended\bug_fixes\cl_maxfps_singleplayer');
  AddFeatureLine(Config, 'list_match_fix', 'features\recommended\bug_fixes\list_match_fix');
  AddFeatureLine(Config, 'texture_mapping_min_mag', 'features\recommended\graphics_features\texture_mapping_min_mag');
  AddFeatureLine(Config, 'scaled_con', 'features\recommended\graphics_features\scaled_con');
  AddFeatureLine(Config, 'scaled_hud', 'features\recommended\graphics_features\scaled_hud');
  AddFeatureLine(Config, 'hd_textures', 'features\recommended\graphics_features\hd_textures');
  AddFeatureLine(Config, 'vsync_toggle', 'features\recommended\graphics_features\vsync_toggle');
  AddFeatureLine(Config, 'lighting_blend', 'features\recommended\graphics_features\lighting_blend');
  AddFeatureLine(Config, 'teamicons_offset', 'features\recommended\gameplay_features\teamicons_offset');
  AddFeatureLine(Config, 'http_maps', 'features\recommended\networking_features\http_maps');
  AddFeatureLine(Config, 'internal_menus', 'features\recommended\menu_features\internal_menus');
  AddFeatureLine(Config, 'raw_mouse', 'features\recommended\input_features\raw_mouse');
  AddFeatureLine(Config, 'cbuf_limit_increase', 'features\optional\bug_fixes\cbuf_limit_increase');
  AddFeatureLine(Config, 'scaled_menu', 'features\optional\graphics_features\scaled_menu');
  AddFeatureLine(Config, 'entity_visualizer', 'features\optional\gameplay_features\entity_visualizer');
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
  if (CurPageID = FullViolencePage.ID) and WizardIsTaskSelected('full_violence') then begin
    if not IsValidViolencePassword(FullViolencePage.Values[0]) then begin
      MsgBox('The password must contain 0 to 31 printable ASCII characters.', mbError, MB_OK);
      Result := False;
    end else if not TryResolveFullViolenceSerial(FullViolenceSerial) then begin
      if Trim(FullViolencePage.Values[1]) = '' then
        MsgBox('Could not detect the selected game-drive serial. Under Wine, enter the serial shown by wine cmd /c vol C: (use the same WINEPREFIX as Setup).',
          mbError, MB_OK);
      Result := False;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
    EnableSoFBuddy;
    WriteFeatureConfig;
    if WizardIsTaskSelected('full_violence') then
      ApplyFullViolence;
    if WizardIsTaskSelected('appcompat_fix') then
      ApplyCompatibilityFix;
  end;
end;
