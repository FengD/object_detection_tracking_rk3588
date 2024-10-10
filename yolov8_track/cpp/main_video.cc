// Copyright (c) 2023 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*-------------------------------------------
                Includes
-------------------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <chrono>
#include <string>

#include "yolov8.h"
#include "image_utils.h"
#include "file_utils.h"
#include "image_drawing.h"

#include "ByteTrack/BYTETracker.h"


/*-------------------------------------------
                  Main Function
-------------------------------------------*/

void free_image_buffer(image_buffer_t* image) {
    if (image->virt_addr) {
        free(image->virt_addr);
        image->virt_addr = nullptr;
    }
}

int read_video(const char* path, rknn_app_context_t* app_ctx, image_buffer_t* image, object_detect_result_list* od_results) {
    AVFormatContext* pFormatContext = avformat_alloc_context();
    if (avformat_open_input(&pFormatContext, path, nullptr, nullptr) != 0) {
        printf("cannot open the file %s\n", path);
        return -1;
    }

    if (avformat_find_stream_info(pFormatContext, nullptr) < 0) {
        printf("cannot get the video stream\n");
        return -1;
    }

    int videoStreamIndex = -1;
    for (int i = 0; i < pFormatContext->nb_streams; i++) {
        if (pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        printf("cannot find video stream\n");
        return -1;
    }

    AVCodecParameters* pCodecParameters = pFormatContext->streams[videoStreamIndex]->codecpar;

    const AVCodec* pCodec = avcodec_find_decoder(pCodecParameters->codec_id);
    if (pCodec == nullptr) {
        printf("no adapt decoder\n");
        return -1;
    }

    AVCodecContext* pCodecContext = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecContext, pCodecParameters);

    if (avcodec_open2(pCodecContext, pCodec, nullptr) < 0) {
        printf("cannot open decoder\n");
        return -1;
    }

    SwsContext* swsCtx = sws_getContext(
        pCodecContext->width, pCodecContext->height, pCodecContext->pix_fmt,
        pCodecContext->width, pCodecContext->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* rgbFrame = av_frame_alloc();

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecContext->width, pCodecContext->height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer, AV_PIX_FMT_RGB24, pCodecContext->width, pCodecContext->height, 1);

    memset(image, 0, sizeof(image_buffer_t));

    image->width = pCodecContext->width;
    image->height = pCodecContext->height;
    image->virt_addr = (unsigned char*)malloc(numBytes);
    image->format = IMAGE_FORMAT_RGB888;
    image->size = numBytes;
    int counter = 0;
    
    byte_track::BYTETracker tracker(10, 100, 0.2, 0.3);
    
    while (av_read_frame(pFormatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            if (avcodec_send_packet(pCodecContext, packet) == 0) {
                while (avcodec_receive_frame(pCodecContext, frame) == 0) {
                    std::vector<byte_track::Object> objects;
                    sws_scale(swsCtx, (uint8_t const* const*)frame->data, frame->linesize, 0, pCodecContext->height, rgbFrame->data, rgbFrame->linesize);
                    memcpy(image->virt_addr, rgbFrame->data[0], numBytes);
                    printf("index: %d, width: %d, height: %d, size: %d\n", counter, image->width, image->height, image->size);
                    auto start = std::chrono::high_resolution_clock::now();
                    int ret = inference_yolov8_model(app_ctx, image, od_results);
                    auto end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                    printf("infer: %ldus\n", duration);
                
                    if (ret != 0) {
                        printf("init_yolov8_model fail! ret=%d\n", ret);
                        return 1;
                    }
                    
                    start = std::chrono::high_resolution_clock::now();

                    /*char text[256];
                    for (int i = 0; i < od_results->count; i++) {
                      object_detect_result *det_result = &(od_results->results[i]);
                      printf("%s @ (%d %d %d %d) %.3f\n", coco_cls_to_name(det_result->cls_id),
                             det_result->box.left, det_result->box.top,
                             det_result->box.right, det_result->box.bottom,
                             det_result->prop);
                      float x = (det_result->box.right + det_result->box.left) / 2.0;
                      float y = (det_result->box.bottom + det_result->box.top) / 2.0;
                      float width = det_result->box.bottom - det_result->box.top;
                      float height = det_result->box.right - det_result->box.left;
                      byte_track::Object object(byte_track::Rect<float>(x, y, width, height), det_result->cls_id, det_result->prop);
                      
                      objects.push_back(object);
                      int x1 = det_result->box.left;
                      int y1 = det_result->box.top;
                      int x2 = det_result->box.right;
                      int y2 = det_result->box.bottom;
              
                      draw_rectangle(image, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);
              
                      sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
                      draw_text(image, text, x1, y1 - 20, COLOR_BLUE, 10);
                    }*/
                    const auto outputs = tracker.update(objects);
                    
                    /*for (const auto &outputs_per_frame : outputs) {
                        const auto &rect = outputs_per_frame->getRect();
                        const auto &track_id = outputs_per_frame->getTrackId();
                        const auto &score = outputs_per_frame->getScore();
                        
                        int x1 = rect.x() - rect.width() / 2;
                        int y1 = rect.y() - rect.height() / 2;
                        int x2 = rect.x() + rect.width() / 2;
                        int y2 = rect.y() + rect.height() / 2;
                        
                        printf("%ld %d %d %d %d %f\n", track_id, x1, y1, x2, y2, score);
                
                        draw_rectangle(image, x1, y1, y2 - y1, x2 - x1, COLOR_RED, 3);
                
                        sprintf(text, "%s", std::to_string(track_id).c_str());
                        draw_text(image, text, x1, y1, COLOR_RED, 10);
                    }*/
                    
                    end = std::chrono::high_resolution_clock::now();
                    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                    printf("track: %ldus\n", duration);
                    // char buffer[32];
                    // std::sprintf(buffer, "out/%04d_out.png", counter);
                    // write_image(buffer, image);
                }
                counter++;
            }
        }
        av_packet_unref(packet);
    }

    av_frame_free(&rgbFrame);
    av_free(buffer);
    av_frame_free(&frame);
    av_packet_free(&packet);
    sws_freeContext(swsCtx);
    avcodec_free_context(&pCodecContext);
    avformat_close_input(&pFormatContext);

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("%s <model_path> <video_path>\n", argv[0]);
        return -1;
    }

    const char *model_path = argv[1];
    const char *video_path = argv[2];

    int ret;
    rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));

    init_post_process();

    ret = init_yolov8_model(model_path, &rknn_app_ctx);
    if (ret != 0) {
        printf("init_yolov8_model fail! ret=%d model_path=%s\n", ret, model_path);
        goto out;
    }

    image_buffer_t src_image;
    memset(&src_image, 0, sizeof(image_buffer_t));
    object_detect_result_list od_results;
    
    read_video(video_path, &rknn_app_ctx, &src_image, &od_results);
    
out:
    deinit_post_process();

    ret = release_yolov8_model(&rknn_app_ctx);
    if (ret != 0) {
        printf("release_yolov8_model fail! ret=%d\n", ret);
    }

    if (src_image.virt_addr != NULL) {
        free(src_image.virt_addr);
    }

    return 0;
}
