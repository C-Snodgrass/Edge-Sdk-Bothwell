#!/bin/bash

# Path to the video file
VIDEO_PATH="/home/nolan/Downloads/'Dron_Test_&_US.MP4'"

# Start high-quality stream (1080p)
ffmpeg -re -stream_loop -1 -i "$VIDEO_PATH" \
-c:v libx264 -b:v 8000k -r 30 -s 1920x1080 \
-c:a aac -b:a 128k -f flv rtmp://10.244.146.0/live/high &

# Start low-quality stream (540p)
ffmpeg -re -stream_loop -1 -i "$VIDEO_PATH" \
-c:v libx264 -b:v 512k -r 30 -s 960x540 \
-c:a aac -b:a 128k -f flv rtmp://10.244.146.0/live/low &

# Wait for both streams to start
wait

echo "Streaming high and low quality video..."

# no need for seperate command for the HLS segments this should be done
# automatically
