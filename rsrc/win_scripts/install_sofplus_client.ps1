param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot,
    [Parameter(Mandatory = $true)]
    [string]$PatchKitRoot,
    [string]$Variant = "",
    [switch]$UseWine
)

$ErrorActionPreference = "Stop"
$KitFiles = @(
    "sofplus-patch.exe",
    "sofplus-client-patch.bin",
    "sofplus-client-patch.cfg",
    "sofplus-client-patch.cmd",
    "sofplus-client-patch-readme.txt"
)

function Invoke-PatchTool([string]$WorkDir, [string]$Exe, [string]$ZipName, [string]$CfgName) {
    $prev = Get-Location
    Set-Location -LiteralPath $WorkDir
    try {
        if ($UseWine) {
            $wine = (Get-Command wine -ErrorAction SilentlyContinue).Source
            if (-not $wine) { throw "wine not found" }
            & $wine $Exe $ZipName $CfgName | Out-String | Write-Verbose
            if ($LASTEXITCODE -ne 0) { throw "sofplus-patch.exe failed (exit $LASTEXITCODE)" }
        } else {
            $proc = Start-Process -FilePath $Exe -ArgumentList @($ZipName, $CfgName) -WorkingDirectory $WorkDir -Wait -PassThru -NoNewWindow
            if ($proc.ExitCode -ne 0) { throw "sofplus-patch.exe failed (exit $($proc.ExitCode))" }
        }
    } finally {
        Set-Location -LiteralPath $prev
    }
}

$detectScript = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "sof_install_detect.ps1"
$detectFile = Join-Path $env:TEMP ("sofbuddy_sofplus_detect_{0}.ini" -f [Guid]::NewGuid().ToString("N"))
& $detectScript -GameRoot $GameRoot -OutFile $detectFile
$detect = @{}
Get-Content -LiteralPath $detectFile | ForEach-Object {
    if ($_ -match '^\s*([^=]+)=(.*)$') { $detect[$Matches[1]] = $Matches[2] }
}
Remove-Item -LiteralPath $detectFile -Force -ErrorAction SilentlyContinue

if ($detect.sofplus_present -eq "1") {
    Write-Output "SoF Plus 20140531 is already installed; skipping client patch."
    exit 0
}

if (-not $Variant) { $Variant = $detect.patch_variant }
if (-not $Variant) {
    throw ("Could not map this SoF install to a SoF Plus patch (distribution={0}, size={1})." -f `
        $detect.distribution, $detect.exe_size)
}
Write-Output ("SoF detect: distribution={0} patch_variant={1} kb4571756_exe_fix={2}" -f `
    $detect.distribution, $Variant, $detect.kb4571756_exe_fix)

$kitDir = Join-Path $PatchKitRoot $Variant
if (-not (Test-Path -LiteralPath $kitDir)) { throw "Patch kit not found: $kitDir" }

$zipName = if ($Variant -match 'kb4571756$') { "sofplus-client-20140531-kb4571756.zip" } else { "sofplus-client-20140531.zip" }
$outZip = Join-Path $GameRoot $zipName
$staged = @()
foreach ($name in $KitFiles) {
    $src = Join-Path $kitDir $name
    if (-not (Test-Path -LiteralPath $src)) { continue }
    $dst = Join-Path $GameRoot $name
    Copy-Item -LiteralPath $src -Destination $dst -Force
    $staged += $dst
}

try {
    Invoke-PatchTool $GameRoot (Join-Path $GameRoot "sofplus-patch.exe") $zipName "sofplus-client-patch.cfg"
    if (-not (Test-Path -LiteralPath $outZip)) { throw "Expected output zip was not created: $outZip" }
    Expand-Archive -LiteralPath $outZip -DestinationPath $GameRoot -Force
    Remove-Item -LiteralPath $outZip -Force
    Write-Output ("SoF Plus client installed using patch variant {0}." -f $Variant)
} finally {
    foreach ($path in $staged) {
        Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
    }
}

exit 0
