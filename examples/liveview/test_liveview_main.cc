/**
 ********************************************************************
 *
 * @copyright (c) 2023 DJI. All rights reserved.
 *
 * All information contained herein is, and remains, the property of DJI.
 * The intellectual and technical concepts contained herein are proprietary
 * to DJI and may be covered by U.S. and foreign patents, patents in process,
 * and protected by trade secret or copyright law.  Dissemination of this
 * information, including but not limited to data and other proprietary
 * material(s) incorporated within the information, in any form, is strictly
 * prohibited without the express written consent of DJI.
 *
 * If you receive this source code without DJI’s authorization, you may not
 * further disseminate the information, and you must immediately remove the
 * source code and notify DJI of its removal. DJI reserves the right to pursue
 * legal actions against you for any loss(es) or damage(s) caused by your
 * failure to do so.
 *
 *********************************************************************
 */
#include <unistd.h>
#include <string>
#include <memory>
#include <thread>
#include <filesystem>
#include "logger.h"
#include "sample_liveview.h"
#include "stream_decoder.h"
#include "ffmpeg_stream_decoder.h"

using namespace edge_sdk;
using namespace edge_app;

class DualOutputProcessor : public ImageProcessor {
private:
    std::shared_ptr<ImageProcessor> displayProcessor;
    FILE* ffmpegPipe;
    std::thread segmentationThread;
    bool isRunning;
    
public:
    DualOutputProcessor(std::shared_ptr<ImageProcessor> display, 
                       const std::string& outputFolder) 
        : displayProcessor(display), isRunning(true) {
        
        // Create output directory if it doesn't exist
        std::filesystem::create_directories(outputFolder);
        
        // Start FFmpeg process for HLS segmentation
        std::string ffmpegCommand = "ffmpeg -i pipe:0 -codec copy "
                                  "-f hls -hls_time 10 -hls_list_size 0 "
                                  "-hls_segment_filename '" + outputFolder + "/segment%03d.ts' " +
                                  outputFolder + "/playlist.m3u8";
        
        INFO("Starting FFmpeg with command: %s", ffmpegCommand.c_str());
        
        ffmpegPipe = popen(ffmpegCommand.c_str(), "w");
        if (!ffmpegPipe) {
            ERROR("Failed to open FFmpeg pipe");
        }
    }
    
    ~DualOutputProcessor() {
        isRunning = false;
        if (ffmpegPipe) {
            pclose(ffmpegPipe);
        }
    }
    
    virtual int Process(const unsigned char* data, unsigned int len) override {
        // Forward to display processor
        int result = displayProcessor->Process(data, len);
        
        // Also write to FFmpeg pipe
        if (ffmpegPipe && data && len > 0) {
            size_t written = fwrite(data, 1, len, ffmpegPipe);
            fflush(ffmpegPipe);
            if (written != len) {
                ERROR("Failed to write complete frame to FFmpeg pipe");
            }
        }
        
        return result;
    }
};

std::string getHLSOutputPath(int quality) {
    std::string baseDir = "/home/nolan/hls/program1/";
    
    // Quality 1 goes to 'low', 4 goes to 'high'
    if (quality == 1) {
        return baseDir + "low";
    } else if (quality == 4) {
        return baseDir + "high";
    }
    
    // For any other quality settings, don't do HLS output
    return "";
}

ErrorCode ESDKInit();

int main(int argc, char **argv) {
    auto rc = ESDKInit();
    if (rc != kOk) {
        ERROR("pre init failed");
        return -1;
    }

    int type = 0;
    while (argc < 3 || (type = atoi(argv[1])) > 2) {
        ERROR(
            "Usage: %s [CAMERA_TYPE] [QUALITY] [SOURCE]\n"
            "DESCRIPTION:\n"
            "CAMERA_TYPE: 0-FPV, 1-Payload\n"
            "QUALITY: 1-540p(low), 2-720p, 3-720pHigh, 4-1080p(high), 5-1080pHigh\n"
            "SOURCE: 1-wide 2-zoom 3-IR\n"
            "eg: %s 1 3 1",
            argv[0], argv[0]);
        sleep(1);
    }

    auto quality = atoi(argv[2]);
    const char type_to_str[2][16] = {"FPVCamera", "PayloadCamera"};
    auto camera = std::string(type_to_str[type]);
    
    auto liveview_sample = std::make_shared<LiveviewSample>(std::string(camera));
    
    // Create stream decoder
    StreamDecoder::Options decoder_option = {.name = std::string("ffmpeg")};
    auto stream_decoder = CreateStreamDecoder(decoder_option);
    
    // Create the original display processor
    ImageProcessor::Options display_processor_option = {
        .name = std::string("display"),
        .alias = camera,
        .userdata = liveview_sample
    };
    auto display_processor = CreateImageProcessor(display_processor_option);
    
    // Get the HLS output path based on quality
    std::string hlsPath = getHLSOutputPath(quality);
    
    // Create our dual output processor if quality is 1 or 4
    std::shared_ptr<ImageProcessor> final_processor = display_processor;
    if (!hlsPath.empty()) {
        INFO("HLS output will be written to: %s", hlsPath.c_str());
        final_processor = std::make_shared<DualOutputProcessor>(
            display_processor, hlsPath);
    }
    
    if (0 != InitLiveviewSample(
        liveview_sample, 
        (Liveview::CameraType)type, 
        (Liveview::StreamQuality)quality,
        stream_decoder,
        final_processor)) 
    {
        ERROR("Init %s liveview sample failed", camera.c_str());
        return -1;
    }
    
    liveview_sample->Start();
    
    if (argc > 3) {
        auto src = atoi(argv[3]);
        INFO("set camera source: %d", src);
        liveview_sample->SetCameraSource((edge_sdk::Liveview::CameraSource)src);
    }
    
    while (1) sleep(3);
    return 0;
}

