#!/bin/bash

# Function to convert a string to a byte array and append a null terminator
convert_string_to_bytes() {
  local input_string="$1"
  local byte_array=""
  # Convert each character to its ASCII value in hex
  for ((i=0; i<${#input_string}; i++)); do
    byte_array+="$(printf '\\x%02X' "'${input_string:$i:1}")"
  done
  # Append null terminator
  byte_array+="\\x00"
  echo -n "$byte_array"
}

# Usage check
if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <binary_file_path> <ascii_string>"
  exit 1
fi

# Variables
binary_file_path="$1"
ascii_string="$2"
offset=0x11AD72

# Convert the input string to bytes
byte_array=$(convert_string_to_bytes "$ascii_string")

# Write the byte array to the binary file at the specified offset
printf "$byte_array" | dd of="$binary_file_path" bs=1 seek=$((offset)) conv=notrunc

echo "Bytes written to $binary_file_path at offset $offset."