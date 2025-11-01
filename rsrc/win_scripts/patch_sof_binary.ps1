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

# must supply a string
if (-not $args) {
    Write-Output "Error: string to write is required."
    Exit 1
}

Write-Output "Linking SoF.exe to $($args[0])"

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
$userAsciiString = $args[0]
$userByteArray = Convert-StringToBytes $userAsciiString
$userByteArray += 0x00

Write-HexBytes "$binaryFilePath" $offset $userByteArray

Write-Output "Patching completed."

Read-Host -Prompt "Press Enter to exit"
