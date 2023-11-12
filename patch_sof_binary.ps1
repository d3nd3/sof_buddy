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

# Check if $bytesToWrite parameter is provided
if (-not $args) {
    Write-Host "Error: string to write is required."
    Exit 1
}

Write-Host "Linking SoF.exe to $args[0]"

$scriptDirectory = Get-Location
$binaryFilePath = Join-Path $scriptDirectory "SoF.exe"

# Check if the .exe file exists
if (Test-Path $binaryFilePath -PathType Leaf) {
    Write-Host "Found $binaryFileName at $binaryFilePath"
} else {
    Write-Host "Run this script inside your SoF root directory"
    Exit 1
}

$offset = 0x11AD72
$userAsciiString = $args[0]
$userByteArray = Convert-StringToBytes $userAsciiString
$userByteArray += 0x00

Write-HexBytes $binaryFilePath $offset $userByteArray

Write-Host "Patching completed."

Read-Host -Prompt "Press Enter to exit"
