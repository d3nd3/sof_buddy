param(
	[Parameter(Mandatory = $true)][string]$ExePath
)
$ErrorActionPreference = 'Stop'
$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
$jsonPath = Join-Path $dir 'patches.json'
if (-not (Test-Path $jsonPath)) { throw "patches.json not found: $jsonPath" }
$profile = (Get-Content $jsonPath -Raw | ConvertFrom-Json).profiles[0]
$offset = [int]$profile.import_dll_name_offset
$dllName = $profile.import_dll_name
$bytes = [System.Text.Encoding]::ASCII.GetBytes($dllName + [char]0)
$backup = "$ExePath.bak"
if (-not (Test-Path $ExePath)) { throw "SoF2MP.exe not found: $ExePath" }
if (-not (Test-Path $backup)) { Copy-Item -LiteralPath $ExePath -Destination $backup }
$fs = [System.IO.File]::Open($ExePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
try {
	$fs.Seek($offset, [System.IO.SeekOrigin]::Begin) | Out-Null
	$fs.Write($bytes, 0, $bytes.Length)
} finally {
	$fs.Close()
}
Write-Host "Patched import to $dllName (backup: $backup)"
