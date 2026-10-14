param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath
)

$ErrorActionPreference = "Stop"
$TargetAddress = [uint32]0x2015F1C0
$ExpectedImageBase = [uint32]0x20000000
$AsciiText = [Text.Encoding]::ASCII.GetBytes("Raven Software")
$UnicodeText = [Text.Encoding]::Unicode.GetBytes("Raven Software")

function Read-U16([byte[]]$Bytes, [int]$Offset) {
    return [BitConverter]::ToUInt16($Bytes, $Offset)
}

function Read-U32([byte[]]$Bytes, [int]$Offset) {
    return [BitConverter]::ToUInt32($Bytes, $Offset)
}

function Same-Bytes([byte[]]$Left, [byte[]]$Right) {
    if ($Left.Length -ne $Right.Length) { return $false }
    for ($i = 0; $i -lt $Left.Length; $i++) {
        if ($Left[$i] -ne $Right[$i]) { return $false }
    }
    return $true
}

function Matches-Bytes([byte[]]$Bytes, [int]$Offset, [byte[]]$Expected) {
    if ($Offset -lt 0 -or $Offset + $Expected.Length -gt $Bytes.Length) { return $false }
    return Same-Bytes $Bytes[$Offset..($Offset + $Expected.Length - 1)] $Expected
}

function All-Zero([byte[]]$Bytes, [int]$Offset, [int]$Length) {
    for ($i = 0; $i -lt $Length; $i++) {
        if ($Bytes[$Offset + $i] -ne 0) { return $false }
    }
    return $true
}

if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) {
    throw "SoF.exe was not found: $ExecutablePath"
}

$Bytes = [IO.File]::ReadAllBytes($ExecutablePath)
if ($Bytes.Length -lt 0x40) { throw "The executable is too small to be a PE file." }
if ([Text.Encoding]::ASCII.GetString($Bytes, 0, 2) -ne "MZ") {
    throw "SoF.exe is not a valid PE executable."
}

$PeOffset = [BitConverter]::ToInt32($Bytes, 0x3C)
if ($PeOffset -lt 0 -or $PeOffset + 0x58 -gt $Bytes.Length) {
    throw "Invalid PE header offset."
}
if ([Text.Encoding]::ASCII.GetString($Bytes, $PeOffset, 4) -ne "PE`0`0") {
    throw "SoF.exe is not a valid PE executable."
}

$FileHeader = $PeOffset + 4
$SectionCount = Read-U16 $Bytes ($FileHeader + 2)
$OptionalSize = Read-U16 $Bytes ($FileHeader + 16)
$OptionalHeader = $FileHeader + 20
if ((Read-U16 $Bytes $FileHeader) -ne 0x14C) {
    throw "SoF.exe is not an x86 PE executable."
}
if ($OptionalSize -lt 32 -or $OptionalHeader + $OptionalSize -gt $Bytes.Length) {
    throw "Invalid PE optional header."
}
if ((Read-U16 $Bytes $OptionalHeader) -ne 0x10B) {
    throw "SoF.exe is not a 32-bit PE executable."
}

$ImageBase = Read-U32 $Bytes ($OptionalHeader + 28)
if ($ImageBase -ne $ExpectedImageBase) {
    throw ("Unexpected SoF.exe image base 0x{0:X8}; refusing to patch." -f $ImageBase)
}

$Rva = [uint64]$TargetAddress - [uint64]$ImageBase
$SectionTable = $OptionalHeader + $OptionalSize
$SectionTableEnd = [uint64]$SectionTable + ([uint64]$SectionCount * 40)
if ($SectionTableEnd -gt [uint64]$Bytes.Length) {
    throw "Invalid PE section table."
}
$FileOffset = $null
for ($i = 0; $i -lt $SectionCount; $i++) {
    $section = $SectionTable + ($i * 40)
    $virtualAddress = Read-U32 $Bytes ($section + 12)
    $rawSize = Read-U32 $Bytes ($section + 16)
    $rawOffset = Read-U32 $Bytes ($section + 20)
    $virtualEnd = [uint64]$virtualAddress + [uint64]$rawSize
    if ($rawSize -gt 0 -and $Rva -ge $virtualAddress -and $Rva -lt $virtualEnd) {
        if ([uint64]$rawOffset + [uint64]$rawSize -gt [uint64]$Bytes.Length) {
            throw "Invalid PE section data."
        }
        $FileOffset = [uint64]$rawOffset + ($Rva - [uint64]$virtualAddress)
        break
    }
}
if ($null -eq $FileOffset) {
    throw ("Address 0x{0:X8} is not inside a PE section." -f $TargetAddress)
}
$maxTextLength = [Math]::Max($AsciiText.Length, $UnicodeText.Length)
if ($FileOffset -gt [uint64]([int]::MaxValue) -or
    $FileOffset + [uint64]$maxTextLength -gt [uint64]$Bytes.Length) {
    throw "The target string is outside the executable's file data."
}
$FileOffset = [int]$FileOffset

$Expected = $null
if (Matches-Bytes $Bytes $FileOffset $AsciiText) {
    $Expected = $AsciiText
} elseif (Matches-Bytes $Bytes $FileOffset $UnicodeText) {
    $Expected = $UnicodeText
} elseif (All-Zero $Bytes $FileOffset $AsciiText.Length) {
    $BackupPath = "$ExecutablePath.sofbuddy.bak"
    if (Test-Path -LiteralPath $BackupPath -PathType Leaf) {
        $BackupBytes = [IO.File]::ReadAllBytes($BackupPath)
        $BackupExpected = $null
        if (Matches-Bytes $BackupBytes $FileOffset $AsciiText) {
            $BackupExpected = $AsciiText
        } elseif (Matches-Bytes $BackupBytes $FileOffset $UnicodeText) {
            $BackupExpected = $UnicodeText
        } else {
            throw "An existing SoF.exe.sofbuddy.bak does not contain the original string."
        }
        $Restored = [byte[]]$Bytes.Clone()
        for ($i = 0; $i -lt $BackupExpected.Length; $i++) {
            $Restored[$FileOffset + $i] = $BackupBytes[$FileOffset + $i]
        }
        if (-not (Same-Bytes $Restored $BackupBytes)) {
            throw "An existing SoF.exe.sofbuddy.bak does not match the patched executable."
        }
    }
    Write-Output "Windows compatibility string is already removed."
    exit 0
} else {
    throw ("Unexpected bytes at 0x{0:X8}; refusing to patch this executable." -f $TargetAddress)
}

$BackupPath = "$ExecutablePath.sofbuddy.bak"
if (Test-Path -LiteralPath $BackupPath -PathType Leaf) {
    $BackupBytes = [IO.File]::ReadAllBytes($BackupPath)
    if (-not (Same-Bytes $BackupBytes $Bytes)) {
        throw "An existing SoF.exe.sofbuddy.bak does not match SoF.exe; refusing to overwrite it."
    }
} else {
    $BackupTempPath = "$BackupPath.$([IO.Path]::GetRandomFileName()).tmp"
    try {
        [IO.File]::Copy($ExecutablePath, $BackupTempPath)
        $BackupBytes = [IO.File]::ReadAllBytes($BackupTempPath)
        if (-not (Same-Bytes $BackupBytes $Bytes)) {
            throw "The SoF.exe backup could not be verified."
        }
        [IO.File]::Move($BackupTempPath, $BackupPath)
    } finally {
        if (Test-Path -LiteralPath $BackupTempPath -PathType Leaf) {
            Remove-Item -LiteralPath $BackupTempPath -Force
        }
    }
}

for ($i = 0; $i -lt $Expected.Length; $i++) {
    $Bytes[$FileOffset + $i] = 0
}

$TempPath = "$ExecutablePath.$([IO.Path]::GetRandomFileName()).tmp"
$ReplaceBackupPath = "$ExecutablePath.$([IO.Path]::GetRandomFileName()).bak"
try {
    [IO.File]::WriteAllBytes($TempPath, $Bytes)
    $TempBytes = [IO.File]::ReadAllBytes($TempPath)
    if (-not (All-Zero $TempBytes $FileOffset $Expected.Length)) {
        throw "The Windows compatibility patch could not be verified."
    }
    [IO.File]::Replace($TempPath, $ExecutablePath, $ReplaceBackupPath)
} finally {
    if (Test-Path -LiteralPath $TempPath -PathType Leaf) {
        Remove-Item -LiteralPath $TempPath -Force
    }
    if (Test-Path -LiteralPath $ReplaceBackupPath -PathType Leaf) {
        Remove-Item -LiteralPath $ReplaceBackupPath -Force
    }
}

$Verify = [IO.File]::ReadAllBytes($ExecutablePath)
if (-not (All-Zero $Verify $FileOffset $Expected.Length)) {
    throw "The Windows compatibility patch could not be verified."
}

Write-Output ("Patched 0x{0:X8}; backup saved to {1}" -f $TargetAddress, $BackupPath)
