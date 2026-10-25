param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot,
    [Parameter(Mandatory = $true)]
    [string]$OutFile
)

$ErrorActionPreference = "Stop"
$ImageBase = [uint32]0x20000000
$Kb4571756Va = [uint32]0x2015F1C0
$DllLinkOffset = 0x11AD72
$RavenUnicode = [Text.Encoding]::Unicode.GetBytes("Raven Software")

# GOG: parental / violence cvar string cluster (0x01 = stock GOG, 0x00 = null-term fix applied).
$GogStringAnchors = @{
    0x201373B8 = "GRANTED"
    0x201373C0 = "cl_violence"
    0x201373CC = "sv_violence"
    0x201373D8 = "lock_pure"
    0x201373E4 = "lock_textures"
    0x20137428 = "lock_password"
    0x20137438 = "non_multilingual"
    0x2013744C = "German"
}
$GogViolenceProbeVas = @(
    0x201373BF, 0x201373CB, 0x201373D7, 0x201373E1, 0x201373E2, 0x201373E3,
    0x201373F1, 0x201373F2, 0x201373F3, 0x201373FE, 0x201373FF, 0x2013740E, 0x2013740F,
    0x2013741A, 0x2013741B, 0x20137427, 0x20137435, 0x20137436, 0x20137437,
    0x20137448, 0x20137449, 0x2013744A, 0x2013744B, 0x20137452, 0x20137453
)

function Read-U16([byte[]]$Bytes, [int]$Offset) { [BitConverter]::ToUInt16($Bytes, $Offset) }
function Read-U32([byte[]]$Bytes, [int]$Offset) { [BitConverter]::ToUInt32($Bytes, $Offset) }

function Get-PeFileOffset([byte[]]$Bytes, [uint32]$Va) {
    $PeOffset = [BitConverter]::ToInt32($Bytes, 0x3C)
    $FileHeader = $PeOffset + 4
    $SectionCount = Read-U16 $Bytes ($FileHeader + 2)
    $OptionalSize = Read-U16 $Bytes ($FileHeader + 16)
    $OptionalHeader = $FileHeader + 20
    $Rva = [uint64]$Va - [uint64]$ImageBase
    $SectionTable = $OptionalHeader + $OptionalSize
    for ($i = 0; $i -lt $SectionCount; $i++) {
        $section = $SectionTable + ($i * 40)
        $virtualAddress = Read-U32 $Bytes ($section + 12)
        $virtualSize = Read-U32 $Bytes ($section + 8)
        $rawOffset = Read-U32 $Bytes ($section + 20)
        if ($virtualSize -gt 0 -and $Rva -ge $virtualAddress -and $Rva -lt ($virtualAddress + $virtualSize)) {
            return [int]([uint64]$rawOffset + ($Rva - [uint64]$virtualAddress))
        }
    }
    return -1
}

function Test-BytesAt([byte[]]$Bytes, [int]$Offset, [byte[]]$Expected) {
    if ($Offset -lt 0 -or $Offset + $Expected.Length -gt $Bytes.Length) { return $false }
    for ($i = 0; $i -lt $Expected.Length; $i++) {
        if ($Bytes[$Offset + $i] -ne $Expected[$i]) { return $false }
    }
    return $true
}

function Test-ZeroRun([byte[]]$Bytes, [int]$Offset, [int]$Length) {
    for ($i = 0; $i -lt $Length; $i++) {
        if ($Bytes[$Offset + $i] -ne 0) { return $false }
    }
    return $true
}

function Test-GogViolenceLayout([byte[]]$Bytes) {
    foreach ($entry in $GogStringAnchors.GetEnumerator()) {
        $expected = [Text.Encoding]::ASCII.GetBytes($entry.Value)
        $off = Get-PeFileOffset $Bytes ([uint32]$entry.Key)
        if ($off -lt 0) { return $false }
        if (-not (Test-BytesAt $Bytes $off $expected)) { return $false }
    }
    foreach ($va in $GogViolenceProbeVas) {
        $off = Get-PeFileOffset $Bytes ([uint32]$va)
        if ($off -lt 0) { return $false }
        $b = $Bytes[$off]
        if ($b -ne 0 -and $b -ne 1) { return $false }
    }
    return $true
}

function Test-Kb4571756ExeFix([byte[]]$Bytes) {
    $off = Get-PeFileOffset $Bytes $Kb4571756Va
    if ($off -lt 0) { return $false }
    return (Test-ZeroRun $Bytes $off $RavenUnicode.Length)
}

function Get-LinkedDllName([byte[]]$Bytes) {
    if ($Bytes.Length -le $DllLinkOffset) { return "" }
    $end = $DllLinkOffset
    while ($end -lt $Bytes.Length -and $Bytes[$end] -ne 0) { $end++ }
    return [Text.Encoding]::ASCII.GetString($Bytes, $DllLinkOffset, $end - $DllLinkOffset)
}

$exePath = Join-Path $GameRoot "SoF.exe"
$lines = @{
    distribution = "unknown"
    patch_variant = ""
    kb4571756_exe_fix = "0"
    raven_compat_cleared = "0"
    sofplus_present = "0"
    linked_dll = ""
    recommend_appcompat = "1"
    recommend_full_violence = "1"
    exe_size = "0"
    detect_error = ""
}

if (-not (Test-Path -LiteralPath $exePath)) {
    $lines.detect_error = "SoF.exe not found"
    ($lines.GetEnumerator() | Sort-Object Name | ForEach-Object { "{0}={1}" -f $_.Key, $_.Value }) | Set-Content -LiteralPath $OutFile -Encoding ASCII
    exit 1
}

$bytes = [IO.File]::ReadAllBytes($exePath)
$lines.exe_size = $bytes.Length.ToString()
$lines.linked_dll = Get-LinkedDllName $bytes

$spcl = Join-Path $GameRoot "spcl.dll"
$sofplusPak = Join-Path $GameRoot "base\sofplus.pak"
if ((Test-Path -LiteralPath $spcl) -and (Test-Path -LiteralPath $sofplusPak)) {
    $lines.sofplus_present = "1"
}

$isGog = Test-GogViolenceLayout $bytes
$kbFix = Test-Kb4571756ExeFix $bytes
if ($kbFix) {
    $lines.kb4571756_exe_fix = "1"
    $lines.raven_compat_cleared = "1"
}

if ($isGog) {
    $lines.distribution = "gog"
} else {
    # Retail CD does not carry the GOG violence / parental-control cvar cluster.
    $lines.distribution = "cd"
}

if ($lines.distribution -eq "cd") {
    $lines.patch_variant = if ($kbFix) { "cd-kb4571756" } else { "cd" }
} elseif ($lines.distribution -eq "gog") {
    $lines.patch_variant = if ($kbFix) { "gog-kb4571756" } else { "gog" }
}

if ($kbFix) { $lines.recommend_appcompat = "0" }
if ($lines.sofplus_present -eq "1") { $lines.recommend_full_violence = "0" }

($lines.GetEnumerator() | Sort-Object Name | ForEach-Object { "{0}={1}" -f $_.Key, $_.Value }) | Set-Content -LiteralPath $OutFile -Encoding ASCII
exit 0
