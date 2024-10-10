# Brief

rk3588 is a good chip to run some AI application. This repo shows you how to execute a 2d object detection and tracking system with the input of a video. The source code of course will be given in the repo.

# Utils

* LubanCat5，the device which use the chip rk3588 [https://doc.embedfire.com/products/link/zh/latest/linux/ebf_lubancat.html](https://doc.embedfire.com/products/link/zh/latest/linux/ebf_lubancat.html)
* `yolov8 of airockchip version`, support only detection, segmentation, pose and obb(has problems), which used to get the onnx of yolov8 model [https://github.com/airockchip/ultralytics_yolov8](https://github.com/airockchip/ultralytics_yolov8)
* airrockchip `rknn_model_zoo` which gives the postprocessing code of yolov8 detection [https://github.com/airockchip/rknn_model_zoo](https://github.com/airockchip/rknn_model_zoo)
* airrockchip `rknn_toolchain2` which used to transform the onnx file to rknn file [https://github.com/airockchip/rknn-toolkit2](https://github.com/airockchip/rknn-toolkit2)
* byte_track_cpp which used to tracking the object. [https://github.com/Vertical-Beach/ByteTrack-cpp](https://github.com/Vertical-Beach/ByteTrack-cpp)
* camera_drivers if need online streaming, ffmpeg lib if need offline streaming

# How to use

## On Board Runtime

`suppose you have your own rknn file`

* install dependencies: `apt install libeigen3-dev ffmpeg libavcodec-dev libavformat-dev libavutil-dev libswscale-dev`

* clone ByteTrack-cpp repo use cmake & make to build the project, get so library and ByteTrack/BYTETRACKER.h
* clone the rknn_model_zoo repo
* put yolov8 in the rknn_model_zoo/exmaples/
* build yolov8/cpp, if meet inlude or library errors try to set the correct path in the yolov8/cpp/CMakeLists.txt
* put .rknn, source, in the yolov8/cpp/build/
* execute
    * ./rknn_yolov8_demo [rknn] [image]
    * ./rknn_yolov8_demo_video [rknn] [video]