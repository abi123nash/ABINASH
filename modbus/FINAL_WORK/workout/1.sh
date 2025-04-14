#!/bin/bash

# Check if the user provided a file argument
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <file_path>"
    exit 1
fi

FILE="$1"

# Check if the file exists
if [ ! -f "$FILE" ]; then
    echo "Error: File '$FILE' not found!"
    exit 1
fi

# Replace 'perror' with 'print_error' in the file
sed -i 's/perror/print_error/g' "$FILE"

echo "Successfully replaced 'perror' with 'print_error' in $FILE"

