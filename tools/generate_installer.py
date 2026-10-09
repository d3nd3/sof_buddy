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


def component_slug(category):
    return re.sub(r"[^a-z0-9]+", "_", category.lower()).strip("_")


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
        "[Types]",
        'Name: "full"; Description: "Recommended feature set"',
        'Name: "custom"; Description: "Custom feature selection"; Flags: iscustom',
        "",
        "[Components]",
        'Name: "features"; Description: "SoF Buddy features"; Types: full custom; Flags: fixed',
        'Name: "windows_compatibility"; Description: "Windows compatibility"; Types: full custom',
        'Name: "windows_compatibility\\appcompat_fix"; Description: "Ensure Windows 10+ Application Compatibility Fix Applied"; Types: full custom',
        'Name: "game_options"; Description: "Game options"; Types: full custom',
        'Name: "game_options\\full_violence"; Description: "Enable full violence"; Types: custom',
    ]

    feature_components = []
    for category, features in categories:
        slug = component_slug(category)
        category_path = f"features\\{slug}"
        category_flags = "; Flags: fixed" if category.lower().startswith("core") else ""
        lines.append(
            f'Name: "{category_path}"; Description: "{category}"; '
            f"Types: full custom{category_flags}"
        )
        for name, disabled in features:
            component = f"{category_path}\\{name}"
            flags = "fixed" if category.lower().startswith("core") else ""
            types = "custom" if disabled else "full custom"
            suffix = f"; Flags: {flags}" if flags else ""
            lines.append(
                f'Name: "{component}"; Description: "{name.replace("_", " ").title()}"; '
                f"Types: {types}{suffix}"
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
        "  FullViolencePassword = 'sof';",
        "  LcgA = 214013;",
        "  LcgC = 2531011;",
        "  LcgModulus = 4294967296L;",
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
        "function EncodeFullViolencePassword(Key: Cardinal): AnsiString;",
        "var",
        "  I, Plain, Ob: Integer;",
        "  Hash: Int64;",
        "begin",
        "  Result := '';",
        "  Hash := 0;",
        "  for I := 0 to Length(FullViolencePassword) do begin",
        "    if I = Length(FullViolencePassword) then Plain := 0",
        "    else Plain := Ord(FullViolencePassword[I + 1]);",
        "    Ob := (Plain + Integer((Int64(Key) shr ((I and 7) * 8)) and $FF)) and $FF;",
        "    Result := Result + AnsiChar(Ob);",
        "    Hash := (Hash * 2 + Ob) mod LcgModulus;",
        "  end;",
        "  Hash := Hash xor Int64(Key);",
        "  for I := 0 to 3 do",
        "    Result := Result + AnsiChar(Integer((Hash shr (I * 8)) and $FF));",
        "end;",
        "",
        "function WriteFullViolence(Serial: Cardinal): Boolean;",
        "var",
        "  Key: Cardinal;",
        "  Blob: AnsiString;",
        "begin",
        "  Key := DwordFromWords(LcgOutput(Serial, 24), LcgOutput(Serial, 25));",
        "  Blob := EncodeFullViolencePassword(Key);",
        "  if not RegWriteBinaryValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Server', Blob) then begin Result := False; exit; end;",
        "  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Graphics', DwordFromWords(LcgOutput(Serial, 4), LcgOutput(Serial, 5))) then begin Result := False; exit; end;",
        "  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Sound', DwordFromWords(LcgOutput(Serial, 8), LcgOutput(Serial, 9))) then begin Result := False; exit; end;",
        "  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Input', DwordFromWords(LcgOutput(Serial, 12), LcgOutput(Serial, 13))) then begin Result := False; exit; end;",
        "  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Networking', DwordFromWords(LcgOutput(Serial, 16), LcgOutput(Serial, 17))) then begin Result := False; exit; end;",
        "  if not RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Performance', DwordFromWords(LcgOutput(Serial, 20), LcgOutput(Serial, 21))) then begin Result := False; exit; end;",
        "  Result := RegWriteDWordValue(HKEY_CURRENT_USER, SoFRegistryKey, 'Optimizations', 0);",
        "end;",
        "",
        "procedure ApplyFullViolence;",
        "var",
        "  Serial: Cardinal;",
        "begin",
        "  if not GetGameVolumeSerial(Serial) then begin",
        "    MsgBox('Could not read the volume serial for the selected SoF folder; full violence was not applied.',",
        "      mbError, MB_OK);",
        "    exit;",
        "  end;",
        "  if not WriteFullViolence(Serial) then",
        "    MsgBox('Could not write the SoF parental-control registry values.', mbError, MB_OK);",
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
        "  ScriptPath := ExpandConstant('{app}\\sof_buddy\\patch_windows_compat.ps1');",
        "  Parameters := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"' + ScriptPath +",
        "    '\" -ExecutablePath \"' + ExpandConstant('{app}\\SoF.exe') + '\"';",
        "  if not Exec(ExpandConstant('{sys}\\WindowsPowerShell\\v1.0\\powershell.exe'),",
        "    Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then",
        "    MsgBox('The Windows 10+ compatibility patch failed. Check the SoF.exe version and backup before retrying.',",
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
        "end;",
        "",
        "procedure CurStepChanged(CurStep: TSetupStep);",
        "begin",
        "  if CurStep = ssPostInstall then begin",
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
