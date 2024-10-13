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
#ifndef __IMAGE_PROCESSOR_DISPLAY_H__
#define __IMAGE_PROCESSOR_DISPLAY_H__

#include <memory>
#include <string>
#include "opencv2/opencv.hpp"
#include "liveview/sample_liveview.h"

// Include necessary FFmpeg headers
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace edge_app {

class ImageDisplayProcessor : public ImageProcessor {
public:
    ImageDisplayProcessor(const std::string& name, std::shared_ptr<void> userdata) 
        : name_(name), rtmpEnabled_(false) {
        cv::namedWindow(name.c_str(), cv::WINDOW_NORMAL);
        cv::resizeWindow(name.c_str(), 960, 540);
        cv::moveWindow(name.c_str(), rand() & 0xFF, rand() & 0xFF);
        if (userdata) {
            liveview_sample_ = std::static_pointer_cast<LiveviewSample>(userdata);
        }
    }

    ~ImageDisplayProcessor() override {
        if (rtmpEnabled_) {
            finalizeRTMP();
        }
    }

    void Process(const std::shared_ptr<Image> image) override {
        std::string h = std::to_string(image->size().width);
        std::string w = std::to_string(image->size().height);
        std::string osd = h + "x" + w;
        if (liveview_sample_) {
            auto kbps = liveview_sample_->GetStreamBitrate();
            osd += std::string(",") + std::to_string(kbps) + std::string("kbps");
        }
        putText(*image, osd, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1,
                cv::Scalar(0, 0, 255), 3);
        imshow(name_.c_str(), *image);
        cv::waitKey(1);

        if (rtmpEnabled_) {
            sendFrameToRTMP(image);
        }
    }
// My additions
    bool enableRTMPStreaming(const std::string& rtmpUrl) {
        if (rtmpEnabled_) {
            return false;  // Already enabled
        }
        rtmpUrl_ = rtmpUrl;
        if (initializeRTMP()) {
            rtmpEnabled_ = true;
            return true;
        }
        return false;
    }

private:
    std::string name_;
    std::shared_ptr<LiveviewSample> liveview_sample_;
    bool rtmpEnabled_;
    std::string rtmpUrl_;

    // FFmpeg-related members
    AVFormatContext *formatContext_ = nullptr;
    AVStream *videoStream_ = nullptr;
    AVCodecContext *codecContext_ = nullptr;
    SwsContext *swsContext_ = nullptr;

       bool initializeRTMP() {
        avformat_network_init();
        int ret = avformat_alloc_output_context2(&formatContext_, nullptr, "flv", rtmpUrl_.c_str());
        if (ret < 0) {
            // Handle error
            return false;
        }

        const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        videoStream_ = avformat_new_stream(formatContext_, codec);
        codecContext_ = avcodec_alloc_context3(codec);

        // Set codec parameters to match the specified stream quality
        codecContext_->width = 1920;
        codecContext_->height = 1080;
        codecContext_->time_base = (AVRational){1, 30}; // 30 fps
        codecContext_->framerate = (AVRational){30, 1};
        codecContext_->pix_fmt = AV_PIX_FMT_YUV420P;
        codecContext_->bit_rate = 8 * 1024 * 1024; // 8 Mbps
        
        // Set H.264-specific parameters for better quality
        codecContext_->gop_size = 30; // Keyframe every 1 second
        codecContext_->max_b_frames = 2;
        codecContext_->profile = FF_PROFILE_H264_HIGH;
        codecContext_->level = 41; // Level 4.1

        // Set quality-related parameters
        codecContext_->flags |= AV_CODEC_FLAG_QSCALE;
        codecContext_->global_quality = FF_QP2LAMBDA * 23; // Adjust this value for quality vs bitrate trade-off

        ret = avcodec_open2(codecContext_, codec, nullptr);
        if (ret < 0) {
            // Handle error
            return false;
        }

        avcodec_parameters_from_context(videoStream_->codecpar, codecContext_);

        if (!(formatContext_->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&formatContext_->pb, rtmpUrl_.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                // Handle error
                return false;
            }
        }

        AVDictionary *opts = nullptr;
        av_dict_set(&opts, "rtmp_live", "live", 0); // Ensure we're in live mode

        ret = avformat_write_header(formatContext_, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            // Handle error
            return false;
        }

        return true;
    }

    void sendFrameToRTMP(const std::shared_ptr<Image>& image) {
        if (!swsContext_) {
            swsContext_ = sws_getContext(
                image->cols, image->rows, AV_PIX_FMT_BGR24,
                codecContext_->width, codecContext_->height, AV_PIX_FMT_YUV420P,
                SWS_BICUBIC, nullptr, nullptr, nullptr);
        }

        AVFrame *frame = av_frame_alloc();
        frame->format = codecContext_->pix_fmt;
        frame->width = codecContext_->width;
        frame->height = codecContext_->height;
        av_frame_get_buffer(frame, 0);

        const uint8_t *srcSlice[] = { image->data };
        int srcStride[] = { static_cast<int>(image->step) };
        sws_scale(swsContext_, srcSlice, srcStride, 0, image->rows, frame->data, frame->linesize);

        frame->pts = av_rescale_q(codecContext_->frame_number, codecContext_->time_base, videoStream_->time_base);

        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;

        int ret = avcodec_send_frame(codecContext_, frame);
        if (ret < 0) {
            // Handle error
        }

        while (ret >= 0) {
            ret = avcodec_receive_packet(codecContext_, &pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                // Handle error
                break;
            }

            pkt.stream_index = videoStream_->index;
            av_packet_rescale_ts(&pkt, codecContext_->time_base, videoStream_->time_base);
            av_interleaved_write_frame(formatContext_, &pkt);
            av_packet_unref(&pkt);
        }

        av_frame_free(&frame);
    }

    void finalizeRTMP() {
        av_write_trailer(formatContext_);
        if (!(formatContext_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&formatContext_->pb);
        }
        avcodec_free_context(&codecContext_);
        avformat_free_context(formatContext_);
        sws_freeContext(swsContext_);
    }
};

}  // namespace edge_app

#endif

