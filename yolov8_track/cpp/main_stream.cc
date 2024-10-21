
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>
#include <sys/mman.h>

#include <iostream>
#include <chrono>
#include <string>

#include "yolov8.h"
#include "image_utils.h"
#include "file_utils.h"
#include "image_drawing.h"

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("%s <model_path> <video_path>\n", argv[0]);
        return -1;
    }

    const char *model_path = argv[1];
    const char *stream_path = argv[2];

    int fd = open(stream_path, O_RDWR);
    if (fd == -1) {
        std::cerr << "Error: Cannot open " << stream_path << std::endl;
        return -1;
    }

    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        std::cerr << "Error: Cannot query capabilities" << std::endl;
        close(fd);
        return -1;
    }

    struct v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = 800;
    format.fmt.pix.height = 600;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
        std::cerr << "Error: Cannot set format" << std::endl;
        close(fd);
        return -1;
    }
    
    struct v4l2_streamparm streamparm;
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_PARM, &streamparm) == -1) {
        perror("Getting Frame Rate");
        return -1;
    }
    
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = 20;
    
    if (ioctl(fd, VIDIOC_S_PARM, &streamparm) == -1) {
        perror("Setting Frame Rate");
        return -1;
    }

    struct v4l2_requestbuffers reqbuf;
    reqbuf.count = 1;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
        std::cerr << "Error: Cannot request buffer" << std::endl;
        close(fd);
        return -1;
    }

    struct v4l2_buffer buffer;
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) == -1) {
        std::cerr << "Error: Cannot query buffer" << std::endl;
        close(fd);
        return -1;
    }

    unsigned char* buffer_start = static_cast<unsigned char*>(mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset));
    if (buffer_start == MAP_FAILED) {
        std::cerr << "Error: mmap failed" << std::endl;
        close(fd);
        return -1;
    }

    if (ioctl(fd, VIDIOC_QBUF, &buffer) == -1) {
        std::cerr << "Error: Cannot queue buffer" << std::endl;
        close(fd);
        return -1;
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        std::cerr << "Error: Cannot start capture" << std::endl;
        close(fd);
        return -1;
    }
    
    int ret;
    rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));

    init_post_process();

    ret = init_yolov8_model(model_path, &rknn_app_ctx);
    if (ret != 0) {
        printf("init_yolov8_model fail! ret=%d model_path=%s\n", ret, model_path);
        return 1;
    }
    
    int output_width = 810;
    int output_height = 1080;

    image_buffer_t image;
    memset(&image, 0, sizeof(image_buffer_t));
    image.width = output_width;
    image.height = output_height;
    image.virt_addr = (unsigned char*)malloc(output_width * output_height * 3);
    image.format = IMAGE_FORMAT_RGB888;
    image.size = output_width * output_height * 3;
    
    object_detect_result_list od_results;
    memset(&od_results, 0, sizeof(object_detect_result_list));
    
    

    while (true) {
        if (ioctl(fd, VIDIOC_DQBUF, &buffer) == -1) {
            std::cerr << "Error: Cannot dequeue buffer" << std::endl;
            break;
        }

        cv::Mat img_yuv(format.fmt.pix.height, format.fmt.pix.width, CV_8UC2, buffer_start); // YUV image
        cv::Mat img_rgb;
        cv::cvtColor(img_yuv, img_rgb, cv::COLOR_YUV2RGB_YUYV); // Convert YUV to RGB
        
        cv::resize(img_rgb, img_rgb, cv::Size(output_width, output_height));

        memcpy(image.virt_addr, img_rgb.data, img_rgb.total() * img_rgb.elemSize());
        
        auto start = std::chrono::high_resolution_clock::now();
        int ret = inference_yolov8_model(&rknn_app_ctx, &image, &od_results);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        printf("infer: %ldus\n", duration);
    
        if (ret != 0) {
            printf("init_yolov8_model fail! ret=%d\n", ret);
            return 1;
        }
        
        char text[256];
        for (int i = 0; i < od_results.count; i++) {
          object_detect_result *det_result = &(od_results.results[i]);
          printf("%s @ (%d %d %d %d) %.3f\n", coco_cls_to_name(det_result->cls_id),
                 det_result->box.left, det_result->box.top,
                 det_result->box.right, det_result->box.bottom,
                 det_result->prop);
          int x1 = det_result->box.left;
          int y1 = det_result->box.top;
          int x2 = det_result->box.right;
          int y2 = det_result->box.bottom;
  
          draw_rectangle(&image, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);
  
          sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
          draw_text(&image, text, x1, y1 - 20, COLOR_RED, 10);
        }

        cv::Mat img_show(image.height, image.width, CV_8UC3, image.virt_addr);
        cv::cvtColor(img_show, img_show, cv::COLOR_RGB2BGR);
        cv::resize(img_show, img_show, cv::Size(800, 600));
        if (!img_show.empty()) {
            cv::imshow("Video", img_show);
        }

        if (ioctl(fd, VIDIOC_QBUF, &buffer) == -1) {
            std::cerr << "Error: Cannot queue buffer" << std::endl;
            break;
        }

        if (cv::waitKey(1) >= 0) break;
    }

    // stop cap
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        std::cerr << "Error: Cannot stop capture" << std::endl;
    }

    // release
    munmap(buffer_start, buffer.length);
    close(fd);
    
    deinit_post_process();

    ret = release_yolov8_model(&rknn_app_ctx);
    if (ret != 0) {
        printf("release_yolov8_model fail! ret=%d\n", ret);
    }

    if (image.virt_addr != NULL) {
        free(image.virt_addr);
    }

    return 0;
}
