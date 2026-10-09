# Arguments: DLL name to load, optionally -NoPause for unattended callers.
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$DllName,
    [switch]$NoPause
)

# Function to convert ASCII string to byte array
function Convert-StringToBytes($asciiString) {
    $encoding = [System.Text.Encoding]::ASCII
    $byteArray = $encoding.GetBytes($asciiString)
    return $byteArray
}

# Function to write hex bytes directly to a file at a specified offset
function Write-HexBytes($filePath, $offset, $hexBytes) {
    $fileContent = [System.IO.File]::ReadAllBytes($filePath)
    for ($i = 0; $i -lt $hexBytes.Length; $i++) {
        $fileContent[$offset + $i] = $hexBytes[$i]
    }
    [IO.File]::WriteAllBytes($filePath, $fileContent)
}

Write-Output "Linking SoF.exe to $DllName"

# Get the script directory using a compatible method
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Definition
$binaryFilePath = Join-Path (Split-Path -Parent $scriptDirectory) "SoF.exe"


# Check if the .exe file exists
if (Test-Path "$binaryFilePath" -PathType Leaf) {
    Write-Output "Found SoF.exe at $binaryFilePath"
} else {
    Write-Output "Run this script inside your SoF root directory"
    Exit 1
}


$offset = 0x11AD72
$userAsciiString = $DllName
$userByteArray = Convert-StringToBytes $userAsciiString
$userByteArray += 0x00

if (-not (Test-Path "$binaryFilePath.bak" -PathType Leaf)) {
    Copy-Item -LiteralPath $binaryFilePath -Destination "$binaryFilePath.bak" -ErrorAction Stop
    Write-Output "Saved original executable to $binaryFilePath.bak"
}

Write-HexBytes "$binaryFilePath" $offset $userByteArray

Write-Output "Patching completed."

if (-not $NoPause) {
    Read-Host -Prompt "Press Enter to exit"
}
