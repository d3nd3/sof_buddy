#!/usr/bin/env python3
"""Generate the Inno Setup script from the repository feature defaults."""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FEATURE_NAME = re.compile(r"^[a-z][a-z0-9_-]*$")
IGNORED_HEADERS = ("SoF Buddy", "Lines ", "Uncommented", "Commented")


def read_features(path):
    categories = []
    category = "Features"
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue

        disabled = False
        if line.startswith("//"):
            disabled, line = True, line[2:].strip()
        elif line.startswith("#"):
            line = line[1:].strip()
            if not re.match(r"^[a-z][a-z0-9_-]*(?:\s|$)", line):
                if line and not line.startswith(IGNORED_HEADERS):
                    category = line
                continue
            disabled = True

        name = line.split("#", 1)[0].strip()
        if FEATURE_NAME.fullmatch(name):
            if not categories or categories[-1][0] != category:
                categories.append((category, []))
            categories[-1][1].append((name, disabled))
    return categories


def feature_label(name):
    return name.replace("_", " ").title()


def generate(features_path, output_path):
    categories = read_features(features_path)
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    lines = [
        "; Generated from features/FEATURES.txt. Do not edit by hand.",
        f'#define AppVersion "{version}"',
        "",
        "[Setup]",
        'AppId={{B0F7F5D4-4A1E-4F4A-A7B4-8CF2E7C9B1A6}}',
        "AppName=SoF Buddy",
        "AppVersion={#AppVersion}",
        "AppPublisher=d3nd3",
        "DefaultDirName={autopf}\\Soldier of Fortune",
        "AppendDefaultDirName=no",
        "DisableDirPage=no",
        "DirExistsWarning=no",
        "UsePreviousAppDir=no",
        "DefaultGroupName=SoF Buddy",
        "DisableProgramGroupPage=yes",
        "LicenseFile=..\\LICENSE",
        "PrivilegesRequired=admin",
        "OutputDir=output",
        "OutputBaseFilename=sof_buddy_setup",
        "Compression=lzma",
        "SolidCompression=yes",
        "WizardStyle=modern",
        "",
        "[Messages]",
        "SelectDirDesc=Select the Soldier of Fortune folder",
        "SelectDirLabel3=SoF Buddy will be installed into the selected folder:",
        "SelectDirBrowseLabel=Choose the folder containing SoF.exe, then click Next.",
        "",
        "[Types]",
        'Name: "full"; Description: "Recommended feature set"',
        'Name: "custom"; Description: "Custom feature selection"; Flags: iscustom',
        "",
        "[Components]",
        'Name: "features"; Description: "SoF Buddy features"; Types: full custom; Flags: fixed',
        'Name: "windows_compatibility"; Description: "Windows compatibility"; Types: full custom',
        'Name: "windows_compatibility\\appcompat_fix"; Description: "Ensure Windows 10+ Application Compatibility Fix Applied"; Types: full custom',
        'Name: "game_options"; Description: "Game options"; Types: full custom',
        'Name: "game_options\\full_violence"; Description: "Configure SoF parental controls (full violence unlock)"; '
        "Types: full custom",
    ]

    recommended = []
    optional = []
    for category, features in categories:
        core = category.lower().startswith("core")
        for name, disabled in features:
            if core or not disabled:
                recommended.append((name, category, core))
            else:
                optional.append((name, category))

    lines += [
        'Name: "features\\recommended"; Description: "Recommended / tested features"; '
        "Types: full custom",
    ]
    feature_components = []
    for name, category, core in recommended:
        component = f"features\\recommended\\{name}"
        flags = "; Flags: fixed" if core else ""
        desc = feature_label(name)
        if not core:
            desc = f"{desc} ({category})"
        lines.append(
            f'Name: "{component}"; Description: "{desc}"; Types: full custom{flags}'
        )
        feature_components.append((name, component))

    if optional:
        lines.append(
            'Name: "features\\optional"; Description: "Un-recommended / unstable features"; '
            "Types: full custom; Flags: checkablealone"
        )
        for name, category in optional:
            component = f"features\\optional\\{name}"
            desc = f"{feature_label(name)} ({category})"
            lines.append(
                f'Name: "{component}"; Description: "{desc}"; Types: custom'
            )
            feature_components.append((name, component))

    lines += [
        "",
        "[Files]",
        'Source: "payload\\sof_buddy.dll"; DestDir: "{app}"; Flags: ignoreversion',
        'Source: "payload\\sof_buddy\\*"; DestDir: "{app}\\sof_buddy"; '
        "Flags: ignoreversion recursesubdirs createallsubdirs",
        'Source: "..\\rsrc\\win_scripts\\patch_windows_compat.ps1"; '
        'DestDir: "{app}\\sof_buddy"; Components: windows_compatibility\\appcompat_fix; '
        "Flags: ignoreversion",
        "",
        "[UninstallDelete]",
        'Type: files; Name: "{app}\\sof_buddy\\features.cfg"',
        "",
        "[Code]",
        "const",
        "  SoFRegistryKey = 'Software\\Raven Software\\SoF';",
        "  LcgA = 214013;",
        "  LcgC = 2531011;",
        "  LcgInverse = $B9B33155;",
        "  LcgModulus = 4294967296;",
        "  PatchOffset = $11AD72;",
        "",
        "var",
        "  FullViolencePage: TInputQueryWizardPage;",
        "  FullViolenceModePage: TInputOptionWizardPage;",
        "  FullViolenceSerial: Cardinal;",
        "  ViolenceDefaultApplied: Boolean;",
        "",
        "function GetVolumeInformationW(RootPathName, VolumeNameBuffer: string; VolumeNameSize: Cardinal;",
        "  var VolumeSerialNumber, MaximumComponentLength, FileSystemFlags: Cardinal;",
        "  FileSystemNameBuffer: string; FileSystemNameSize: Cardinal): Boolean;",
        "external 'GetVolumeInformationW@kernel32.dll stdcall';",
        "",
        "function GetGameVolumeSerial(var Serial: Cardinal): Boolean;",
        "var",
        "  Drive, VolumeName, FileSystemName: String;",
        "  MaxComponentLength, FileSystemFlags: Cardinal;",
        "begin",
        "  Drive := ExtractFileDrive(ExpandConstant('{app}'));",
        "  if Drive = '' then begin Result := False; exit; end;",
        "  Drive := AddBackslash(Drive);",
        "  SetLength(VolumeName, 260);",
        "  SetLength(FileSystemName, 260);",
        "  Result := GetVolumeInformationW(Drive, VolumeName, 260, Serial,",
        "    MaxComponentLength, FileSystemFlags, FileSystemName, 260);",
        "end;",
        "",
        "function LcgOutput(Seed: Cardinal; N: Integer): Cardinal;",
        "var",
        "  State: Int64;",
        "  I: Integer;",
        "begin",
        "  State := Seed;",
        "  for I := 0 to N do",
        "    State := (State * LcgA + LcgC) mod LcgModulus;",
        "  Result := Cardinal((State shr 16) and $FFFF);",
        "end;",
        "",
        "function DwordFromWords(LowWord, HighWord: Cardinal): Cardinal;",
        "begin",
        "  Result := Cardinal(Int64(HighWord) * 65536 + LowWord);",
        "end;",
        "",
        "function ExpectedParentalValue(Serial: Cardinal; Index: Integer; Censored: Boolean): Cardinal;",
        "var",
        "  FirstOutput: Integer;",
        "begin",
        "  if Index = 6 then FirstOutput := 26",
        "  else FirstOutput := Index * 4;",
        "  if Censored then FirstOutput := FirstOutput + 2;",
        "  Result := DwordFromWords(LcgOutput(Serial, FirstOutput), LcgOutput(Serial, FirstOutput + 1));",
        "end;",
        "",
        "function EncodeFullViolencePassword(const Password: AnsiString; Key: Cardinal): AnsiString;",
        "var",
        "  I, Plain, Ob: Integer;",
        "  Hash: Int64;",
        "begin",
        "  Result := '';",
        "  Hash := 0;",
        "  for I := 0 to Length(Password) do begin",
        "    if I = Length(Password) then Plain := 0",
        "    else Plain := Ord(Password[I + 1]);",
        "    Ob := (Plain + Integer((Int64(Key) shr ((I and 7) * 8)) and $FF)) and $FF;",
        "    Result := Result + Chr(Ob);",
        "    Hash := (Hash * 2 + Ob) mod LcgModulus;",
        "  end;",
        "  Hash := Hash xor Int64(Key);",
        "  for I := 0 to 3 do",
        "    Result := Result + Chr(Integer((Hash shr (I * 8)) and $FF));",
        "end;",
        "",
        "function WriteFullViolence(Serial: Cardinal; const Password: AnsiString; Censored: Boolean): Boolean;",
        "var",
        "  Key: Cardinal;",
        "  Blob: AnsiString;",
        "begin",
        "  Key := DwordFromWords(LcgOutput(Serial, 24), LcgOutput(Serial, 25));",
        "  Blob := EncodeFullViolencePassword(Password, Key);",
        "  if not RegWriteBinaryValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Server', Blob) then begin Result := False; exit; end;",
        "  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Graphics', ExpectedParentalValue(Serial, 1, Censored)) then begin Result := False; exit; end;",
        "  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Sound', ExpectedParentalValue(Serial, 2, Censored)) then begin Result := False; exit; end;",
        "  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Input', ExpectedParentalValue(Serial, 3, Censored)) then begin Result := False; exit; end;",
        "  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Networking', ExpectedParentalValue(Serial, 4, Censored)) then begin Result := False; exit; end;",
        "  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Performance', ExpectedParentalValue(Serial, 5, Censored)) then begin Result := False; exit; end;",
        "  Result := RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Optimizations', ExpectedParentalValue(Serial, 6, Censored));",
        "end;",
        "",
        "function TryParseVolumeSerial(const Value: String; var Serial: Cardinal): Boolean;",
        "var",
        "  HexValue: String;",
        "  LowWord, HighWord: Integer;",
        "begin",
        "  Result := False;",
        "  HexValue := Uppercase(Trim(Value));",
        "  if (Length(HexValue) = 9) and (HexValue[5] = '-') then",
        "    HexValue := Copy(HexValue, 1, 4) + Copy(HexValue, 6, 4);",
        "  if Length(HexValue) <> 8 then exit;",
        "  try",
        "    LowWord := StrToInt('$' + Copy(HexValue, 5, 4));",
        "    HighWord := StrToInt('$' + Copy(HexValue, 1, 4));",
        "  except",
        "    exit;",
        "  end;",
        "  Serial := Cardinal(Int64(HighWord) * 65536 + LowWord);",
        "  Result := True;",
        "end;",
        "",
        "function TryParseHexDword(const Value: String; var Number: Cardinal): Boolean;",
        "var",
        "  HexValue: String;",
        "  LowWord, HighWord: Integer;",
        "begin",
        "  Result := False;",
        "  HexValue := Uppercase(Trim(Value));",
        "  if (Length(HexValue) >= 2) and (Copy(HexValue, 1, 2) = '0X') then",
        "    HexValue := Copy(HexValue, 3, Length(HexValue) - 2);",
        "  if Length(HexValue) <> 8 then exit;",
        "  try",
        "    LowWord := StrToInt('$' + Copy(HexValue, 5, 4));",
        "    HighWord := StrToInt('$' + Copy(HexValue, 1, 4));",
        "  except",
        "    exit;",
        "  end;",
        "  Number := Cardinal(Int64(HighWord) * 65536 + LowWord);",
        "  Result := True;",
        "end;",
        "",
        "function IsValidViolencePassword(const Password: String): Boolean;",
        "var",
        "  I, Ch: Integer;",
        "begin",
        "  Result := False;",
        "  if Length(Password) > 31 then exit;",
        "  for I := 1 to Length(Password) do begin",
        "    Ch := Ord(Password[I]);",
        "    if (Ch < 32) or (Ch > 126) then exit;",
        "  end;",
        "  Result := True;",
        "end;",
        "",
        "function ReverseLcgStep(State: Cardinal): Cardinal;",
        "var",
        "  Value, LowProduct, HighProduct: Int64;",
        "begin",
        "  Value := (Int64(State) - LcgC + LcgModulus) mod LcgModulus;",
        "  LowProduct := (Value and $FFFF) * (LcgInverse and $FFFF);",
        "  HighProduct := (((Value and $FFFF) * (LcgInverse shr 16) +",
        "    (Value shr 16) * (LcgInverse and $FFFF)) and $FFFF) * 65536;",
        "  Result := Cardinal((LowProduct + HighProduct) mod LcgModulus);",
        "end;",
        "",
        "function TrySeedFromKey(Key: Cardinal; var Seed: Cardinal; var CandidateCount: Integer): Boolean;",
        "var",
        "  LowWord: Integer;",
        "  KeyLow, KeyHigh, State, NextState, Candidate: Cardinal;",
        "  I: Integer;",
        "begin",
        "  CandidateCount := 0;",
        "  KeyLow := Key and $FFFF;",
        "  KeyHigh := (Key shr 16) and $FFFF;",
        "  for LowWord := 0 to $FFFF do begin",
        "    State := DwordFromWords(LowWord, KeyLow);",
        "    NextState := Cardinal((Int64(State) * LcgA + LcgC) mod LcgModulus);",
        "    if ((NextState shr 16) and $FFFF) = KeyHigh then begin",
        "      Candidate := State;",
        "      for I := 1 to 25 do Candidate := ReverseLcgStep(Candidate);",
        "      Inc(CandidateCount);",
        "      if CandidateCount = 1 then Seed := Candidate;",
        "    end;",
        "  end;",
        "  Result := CandidateCount > 0;",
        "end;",
        "",
        "function TryResolveFullViolenceSerial(var Serial: Cardinal): Boolean;",
        "var",
        "  OverrideValue, KeyValue: String;",
        "  Key: Cardinal;",
        "  CandidateCount: Integer;",
        "begin",
        "  OverrideValue := Trim(FullViolencePage.Values[1]);",
        "  if OverrideValue <> '' then begin",
        "    Result := TryParseVolumeSerial(OverrideValue, Serial);",
        "    if not Result then",
        "      MsgBox('Enter the volume serial as XXXX-XXXX, for example 4300-0000.', mbError, MB_OK);",
        "    exit;",
        "  end;",
        "  if GetGameVolumeSerial(Serial) then begin",
        "    Result := True;",
        "    exit;",
        "  end;",
        "  KeyValue := Trim(FullViolencePage.Values[2]);",
        "  if KeyValue = '' then begin Result := False; exit; end;",
        "  if not TryParseHexDword(KeyValue, Key) then begin",
        "    MsgBox('Enter the SoF.exe -cs key as 8 hexadecimal digits.', mbError, MB_OK);",
        "    Result := False;",
        "    exit;",
        "  end;",
        "  Result := TrySeedFromKey(Key, Serial, CandidateCount);",
        "  if Result then begin",
        "    Log(Format('SoF parental settings: key %8.8x recovered %d serial candidate(s); using %8.8x.', [Key, CandidateCount, Serial]));",
        "    if CandidateCount > 1 then Log('The -cs key is ambiguous; enter the exact volume serial to select the correct seed.');",
        "  end;",
        "end;",
        "",
        "procedure ApplyFullViolence;",
        "var",
        "  Password: AnsiString;",
        "  Censored: Boolean;",
        "begin",
        "  Password := AnsiString(FullViolencePage.Values[0]);",
        "  Censored := FullViolenceModePage.SelectedValueIndex = 1;",
        "  if not WriteFullViolence(FullViolenceSerial, Password, Censored) then",
        "    MsgBox('Could not write the SoF parental-control registry values.', mbError, MB_OK)",
        "  else",
        "    MsgBox('SoF parental settings were written to this Windows/Wine user registry. Restart SoF, open the console at startup, type userinfo, and confirm cl_violence is 0.', mbInformation, MB_OK);",
        "end;",
        "",
        "procedure SelectFullViolenceComponent;",
        "begin",
        "  WizardSelectComponents('game_options\\full_violence');",
        "end;",
        "",
        "procedure TypesComboChange(Sender: TObject);",
        "begin",
        "  if WizardCurPageID = wpSelectComponents then",
        "    SelectFullViolenceComponent;",
        "end;",
        "",
        "procedure InitializeWizard;",
        "begin",
        "  ViolenceDefaultApplied := False;",
        "  FullViolencePage := CreateInputQueryPage(wpSelectComponents, 'Full Violence',",
        "    'SoF parental controls', 'Enter the password and optional game-drive serial. The registry values are written to the same Windows or Wine user running Setup.');",
        "  FullViolencePage.Add('Password (0 to 31 ASCII characters)', True);",
        "  FullViolencePage.Values[0] := 'sof';",
        "  FullViolencePage.Add('Volume serial (XXXX-XXXX; blank detects the game drive, then uses the -cs key)', False);",
        "  FullViolencePage.Values[1] := '';",
        "  FullViolencePage.Add('SoF.exe -cs key (8 hex digits; fallback if serial detection fails)', False);",
        "  FullViolencePage.Values[2] := '';",
        "  FullViolenceModePage := CreateInputOptionPage(FullViolencePage.ID, 'Violence Level',",
        "    'SoF parental controls', 'Choose the parental-control state to write.', True, False);",
        "  FullViolenceModePage.Add('Full violence (unlocked)');",
        "  FullViolenceModePage.Add('Censored');",
        "  FullViolenceModePage.SelectedValueIndex := 0;",
        "  WizardForm.TypesCombo.OnChange := @TypesComboChange;",
        "  SelectFullViolenceComponent;",
        "end;",
        "",
        "procedure CurPageChanged(CurPageID: Integer);",
        "begin",
        "  if (CurPageID = wpSelectComponents) and not ViolenceDefaultApplied then begin",
        "    SelectFullViolenceComponent;",
        "    ViolenceDefaultApplied := True;",
        "  end;",
        "end;",
        "",
        "function ShouldSkipPage(PageID: Integer): Boolean;",
        "begin",
        "  Result := ((PageID = FullViolencePage.ID) or (PageID = FullViolenceModePage.ID)) and",
        "    not WizardIsComponentSelected('game_options\\full_violence');",
        "end;",
        "",
        "procedure AddFeatureLine(var Config: String; const Name, Component: String);",
        "begin",
        "  if WizardIsComponentSelected(Component) then",
        "    Config := Config + Name + #13#10",
        "  else",
        "    Config := Config + '// ' + Name + #13#10;",
        "end;",
        "",
        "procedure ApplyCompatibilityFix;",
        "var",
        "  ResultCode: Integer;",
        "  ScriptPath, Parameters: String;",
        "begin",
        "  if RegKeyExists(HKEY_CURRENT_USER, 'Software\\Wine') then begin",
        "    Log('Skipping Windows compatibility fix under Wine.');",
        "    exit;",
        "  end;",
        "  ScriptPath := ExpandConstant('{app}\\sof_buddy\\patch_windows_compat.ps1');",
        "  Parameters := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"' + ScriptPath +",
        "    '\" -ExecutablePath \"' + ExpandConstant('{app}\\SoF.exe') + '\"';",
        "  if not Exec(ExpandConstant('{sys}\\WindowsPowerShell\\v1.0\\powershell.exe'),",
        "    Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then",
        "    MsgBox('The Windows 10+ compatibility patch failed. Check the SoF.exe version and backup before retrying.',",
        "      mbError, MB_OK);",
        "end;",
        "",
        "function IsWine: Boolean;",
        "begin",
        "  Result := RegKeyExists(HKEY_CURRENT_USER, 'Software\\Wine');",
        "end;",
        "",
        "function ApplyWineDllPatch: Boolean;",
        "var",
        "  ExePath, BackupPath: String;",
        "  Data, Link: AnsiString;",
        "  I: Integer;",
        "begin",
        "  Result := False;",
        "  ExePath := ExpandConstant('{app}\\SoF.exe');",
        "  BackupPath := ExePath + '.bak';",
        "  if not FileExists(BackupPath) then",
        "    if not FileCopy(ExePath, BackupPath, False) then exit;",
        "  if not LoadStringFromFile(ExePath, Data) then exit;",
        "  Link := 'sof_buddy.dll' + #0;",
        "  if Length(Data) < PatchOffset + Length(Link) then exit;",
        "  for I := 1 to Length(Link) do",
        "    Data[PatchOffset + I] := Link[I];",
        "  Result := SaveStringToFile(ExePath, Data, False);",
        "end;",
        "",
        "procedure EnableSoFBuddy;",
        "var",
        "  ResultCode: Integer;",
        "  ScriptPath, Parameters: String;",
        "begin",
        "  if IsWine then begin",
        "    if not ApplyWineDllPatch then",
        "      MsgBox('Could not enable SoF Buddy automatically under Wine. Run sof_buddy/enable_sofplus_and_buddy.sh from the SoF folder.',",
        "        mbError, MB_OK);",
        "    exit;",
        "  end;",
        "  ScriptPath := ExpandConstant('{app}\\sof_buddy\\enable_sofplus_and_buddy.cmd');",
        "  Parameters := '/C \"\"' + ScriptPath + '\" -NoPause\"';",
        "  if not Exec(ExpandConstant('{sys}\\cmd.exe'), Parameters,",
        "    ExpandConstant('{app}\\sof_buddy'), SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then",
        "    MsgBox('Could not enable SoF Buddy automatically. Run enable_sofplus_and_buddy.cmd from the SoF folder.',",
        "      mbError, MB_OK);",
        "end;",
        "",
        "procedure WriteFeatureConfig;",
        "var",
        "  Config: String;",
        "begin",
        "  ForceDirectories(ExpandConstant('{app}\\sof_buddy'));",
        "  Config := '// Generated by SoF Buddy Setup.' + #13#10 +",
        "            '// Active lines are enabled; // lines are disabled.' + #13#10#13#10;",
    ]
    for name, component in feature_components:
        lines.append(f"  AddFeatureLine(Config, '{name}', '{component}');")
    lines += [
        "  if not SaveStringToFile(ExpandConstant('{app}\\sof_buddy\\features.cfg'), Config, False) then",
        "    MsgBox('Could not save the selected feature settings.', mbError, MB_OK);",
        "end;",
        "",
        "function NextButtonClick(CurPageID: Integer): Boolean;",
        "begin",
        "  Result := True;",
        "  if CurPageID = wpSelectDir then",
        "    if not FileExists(AddBackslash(WizardDirValue) + 'SoF.exe') then begin",
        "      MsgBox('Choose the Soldier of Fortune folder containing SoF.exe.', mbError, MB_OK);",
        "      Result := False;",
        "    end;",
        "  if (CurPageID = FullViolencePage.ID) and WizardIsComponentSelected('game_options\\full_violence') then begin",
        "    if not IsValidViolencePassword(FullViolencePage.Values[0]) then begin",
        "      MsgBox('The password must contain 0 to 31 printable ASCII characters.', mbError, MB_OK);",
        "      Result := False;",
        "    end else if not TryResolveFullViolenceSerial(FullViolenceSerial) then begin",
        "      if Trim(FullViolencePage.Values[1]) = '' then",
        "        MsgBox('Could not detect the selected game-drive serial. Under Wine, enter the serial shown by wine cmd /c vol C: (use the same WINEPREFIX as Setup).',",
        "          mbError, MB_OK);",
        "      Result := False;",
        "    end;",
        "  end;",
        "end;",
        "",
        "procedure CurStepChanged(CurStep: TSetupStep);",
        "begin",
        "  if CurStep = ssPostInstall then begin",
        "    EnableSoFBuddy;",
        "    WriteFeatureConfig;",
        "    if WizardIsComponentSelected('game_options\\full_violence') then",
        "      ApplyFullViolence;",
        "    if WizardIsComponentSelected('windows_compatibility\\appcompat_fix') then",
        "      ApplyCompatibilityFix;",
        "  end;",
        "end;",
        "",
    ]
    output_path.write_text("\n".join(lines), encoding="utf-8")


def main():
    output = ROOT / "installer" / "sof_buddy.iss"
    if "--output" in sys.argv:
        index = sys.argv.index("--output")
        if index + 1 >= len(sys.argv):
            raise SystemExit("--output requires a path")
        output = Path(sys.argv[index + 1])
    output.parent.mkdir(parents=True, exist_ok=True)
    generate(ROOT / "features" / "FEATURES.txt", output)


if __name__ == "__main__":
    main()
