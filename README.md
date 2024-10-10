# Brief

rk3588 is a good chip to run some AI application. This repo shows you how to execute a 2d object detection and tracking system with the input of a video. The source code of course will be given in the repo.

# Utils

* LubanCat5，the device which use the chip rk3588 [https://doc.embedfire.com/products/link/zh/latest/linux/ebf_lubancat.html](https://doc.embedfire.com/products/link/zh/latest/linux/ebf_lubancat.html)
* `yolov8 of airockchip version`, support only detection, segmentation, pose and obb(has problems), which used to get the onnx of yolov8 model [https://github.com/airockchip/ultralytics_yolov8](https://github.com/airockchip/ultralytics_yolov8)
* airrockchip `rknn_model_zoo` which gives the postprocessing code of yolov8 detection [https://github.com/airockchip/rknn_model_zoo](https://github.com/airockchip/rknn_model_zoo)
* airrockchip `rknn_toolchain2` which used to transform the onnx file to rknn file [https://github.com/airockchip/rknn-toolkit2](https://github.com/airockchip/rknn-toolkit2)
* byte_track_cpp which used to tracking the object. [https://github.com/Vertical-Beach/ByteTrack-cpp](https://github.com/Vertical-Beach/ByteTrack-cpp)
* camera_drivers if need online streaming, ffmpeg lib if need offline streaming

