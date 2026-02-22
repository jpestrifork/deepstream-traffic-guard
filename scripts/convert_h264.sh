#!/bin/bash

if ! command -v ffmpeg &> /dev/null; then
    echo "ffmpeg is not installed. Please install it:"
    echo "  sudo apt install ffmpeg"
    exit 1
fi

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <input.mp4>"
    exit 1
fi

INPUT="$1"

if [ ! -f "$INPUT" ]; then
    echo "Error: file '$INPUT' not found."
    exit 1
fi

BASENAME="${INPUT%.*}"
OUTPUT="${BASENAME}.h264"

ffmpeg -i "$INPUT" -c:v libx264 -crf 23 -preset medium -c:a aac -b:a 128k "$OUTPUT"

echo "Done: $OUTPUT"
