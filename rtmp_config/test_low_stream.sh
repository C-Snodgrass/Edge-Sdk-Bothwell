#!/bin/bash

# Check if ffmpeg is installed
if ! command -v ffmpeg &> /dev/null; then
    echo "ffmpeg could not be found. Please install ffmpeg first."
    exit 1
fi

# Define input and output paths
INPUT_VIDEO="/home/nolan/Downloads/Dron_Test_&_US.MP4"
OUTPUT_DIR="/var/www/hls"

# Clean up old files
echo "Cleaning up old stream files..."
rm -f $OUTPUT_DIR/low/*.ts $OUTPUT_DIR/low/*.m3u8

# Run ffmpeg command
ffmpeg -re -stream_loop -1 -i "$INPUT_VIDEO" \
    -c:v libx264 -b:v 800k -s 640x360 \
    -c:a aac -f hls -hls_time 5 \
    -hls_playlist_type event -hls_flags delete_segments \
    "$OUTPUT_DIR/low/low.m3u8"

# Check if ffmpeg command was successful
if [ $? -eq 0 ]; then
    echo "Low-quality streaming started successfully."
else
    echo "Error occurred while starting the low-quality stream."
fi
