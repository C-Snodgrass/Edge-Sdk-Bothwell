#!/bin/bash

# Check if ffmpeg is installed
if ! command -v ffmpeg &> /dev/null; then
    echo "ffmpeg could not be found. Please install ffmpeg first."
    exit 1
fi

# Define input and output paths
INPUT_VIDEO="/home/nolan/Downloads/student.mp4"
OUTPUT_DIR="/var/www/hls/program_2"

# Clean up old files
echo "Cleaning up old high-quality stream files..."
rm -f $OUTPUT_DIR/high/high*.ts $OUTPUT_DIR/high/high*.m3u8

# Run ffmpeg command for high-quality stream
ffmpeg -re -stream_loop -1 -i "$INPUT_VIDEO" \
    -c:v libx264 -b:v 2800k -s 1280:720 \
    -c:a aac -b:a 128k -f hls -hls_time 5 \
    -hls_playlist_type event -hls_flags delete_segments \
    "$OUTPUT_DIR/high/high.m3u8"

# Check if ffmpeg command was successful
if [ $? -eq 0 ]; then
    echo "Open CV High-quality streaming started successfully."
else
    echo "Error occurred while starting the OpenCV high-quality stream."
fi
