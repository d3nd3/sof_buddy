  SoFDetectPath: String;
  SoFDistribution: String;
  SoFPatchVariant: String;
  SoFRavenCompatCleared: Boolean;
  SoFPlusPresent: Boolean;
  SoFRecommendAppcompat: Boolean;
  SoFRecommendFullViolence: Boolean;

function SoFDetectValue(const Key: String): String;
var
  Lines: TArrayOfString;
  I, P: Integer;
  Line, K: String;
begin
  Result := '';
  if SoFDetectPath = '' then exit;
  if not LoadStringsFromFile(SoFDetectPath, Lines) then exit;
  for I := 0 to GetArrayLength(Lines) - 1 do begin
    Line := Trim(Lines[I]);
    P := Pos('=', Line);
    if P <= 1 then continue;
    K := Trim(Copy(Line, 1, P - 1));
    if CompareText(K, Key) = 0 then begin
      Result := Trim(Copy(Line, P + 1, MaxInt));
      exit;
    end;
  end;
end;

function RunSoFDetect(const GameRoot: String): Boolean;
var
  ResultCode: Integer;
  ScriptPath, Parameters: String;
begin
  SoFDetectPath := ExpandConstant('{tmp}\sofbuddy_sof_detect.ini');
  ScriptPath := ExpandConstant('{tmp}\sof_install_detect.ps1');
  Parameters := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + ScriptPath +
    '" -GameRoot "' + GameRoot + '" -OutFile "' + SoFDetectPath + '"';
  Result := Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
  if not Result then exit;
  SoFDistribution := SoFDetectValue('distribution');
  SoFPatchVariant := SoFDetectValue('patch_variant');
  SoFRavenCompatCleared := (SoFDetectValue('kb4571756_exe_fix') = '1') or
    (SoFDetectValue('raven_compat_cleared') = '1');
  SoFPlusPresent := SoFDetectValue('sofplus_present') = '1';
  SoFRecommendAppcompat := SoFDetectValue('recommend_appcompat') <> '0';
  SoFRecommendFullViolence := SoFDetectValue('recommend_full_violence') <> '0';
end;

procedure ApplySoFDetectTaskDefaults;
begin
  if SoFDetectPath = '' then exit;
  if WizardForm.TasksList.Items.Count < 3 then exit;
  WizardForm.TasksList.Checked[0] := SoFRecommendAppcompat;
  WizardForm.TasksList.Checked[1] := SoFRecommendFullViolence;
  WizardForm.TasksList.Checked[2] := False;
end;

procedure InstallSoFPlusClient;
var
  ResultCode: Integer;
  ScriptPath, Parameters: String;
begin
  ScriptPath := ExpandConstant('{app}\sof_buddy\install_sofplus_client.ps1');
  Parameters := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + ScriptPath +
    '" -GameRoot "' + ExpandConstant('{app}') +
    '" -PatchKitRoot "' + ExpandConstant('{app}\sof_buddy\sofplus_patches') +
    '" -Variant "' + SoFPatchVariant + '"';
  if IsWine then
    Parameters := Parameters + ' -UseWine';
  if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
    MsgBox('SoF Plus client installation failed. You can run install_sofplus_client.ps1 from the sof_buddy folder after setup.',
      mbError, MB_OK);
end;
