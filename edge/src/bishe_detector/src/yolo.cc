#include "yolo.h"

YOLO::YOLO(std::unique_ptr<TrtEngine> trt_engine) : trt_engine_{std::move(trt_engine)} {}

const std::array<std::string, 80> YOLO::kClassNames = {
    "person",         "bicycle",    "car",           "motorcycle",    "airplane",     "bus",           "train",
    "truck",          "boat",       "traffic light", "fire hydrant",  "stop sign",    "parking meter", "bench",
    "bird",           "cat",        "dog",           "horse",         "sheep",        "cow",           "elephant",
    "bear",           "zebra",      "giraffe",       "backpack",      "umbrella",     "handbag",       "tie",
    "suitcase",       "frisbee",    "skis",          "snowboard",     "sports ball",  "kite",          "baseball bat",
    "baseball glove", "skateboard", "surfboard",     "tennis racket", "bottle",       "wine glass",    "cup",
    "fork",           "knife",      "spoon",         "bowl",          "banana",       "apple",         "sandwich",
    "orange",         "broccoli",   "carrot",        "hot dog",       "pizza",        "donut",         "cake",
    "chair",          "couch",      "potted plant",  "bed",           "dining table", "toilet",        "tv",
    "laptop",         "mouse",      "remote",        "keyboard",      "cell phone",   "microwave",     "oven",
    "toaster",        "sink",       "refrigerator",  "book",          "clock",        "vase",          "scissors",
    "teddy bear",     "hair drier", "toothbrush"
};

const std::array<cv::Scalar, 80> YOLO::kClassColors = {
    cv::Scalar(0, 114, 189),   cv::Scalar(217, 83, 25),   cv::Scalar(237, 177, 32),  cv::Scalar(126, 47, 142),
    cv::Scalar(119, 172, 48),  cv::Scalar(77, 190, 238),  cv::Scalar(162, 20, 47),   cv::Scalar(76, 76, 76),
    cv::Scalar(153, 153, 153), cv::Scalar(255, 0, 0),     cv::Scalar(255, 128, 0),   cv::Scalar(191, 191, 0),
    cv::Scalar(0, 255, 0),     cv::Scalar(0, 0, 255),     cv::Scalar(170, 0, 255),   cv::Scalar(85, 85, 0),
    cv::Scalar(85, 170, 0),    cv::Scalar(85, 255, 0),    cv::Scalar(170, 85, 0),    cv::Scalar(170, 170, 0),
    cv::Scalar(170, 255, 0),   cv::Scalar(255, 85, 0),    cv::Scalar(255, 170, 0),   cv::Scalar(255, 255, 0),
    cv::Scalar(0, 85, 128),    cv::Scalar(0, 170, 128),   cv::Scalar(0, 255, 128),   cv::Scalar(85, 0, 128),
    cv::Scalar(85, 85, 128),   cv::Scalar(85, 170, 128),  cv::Scalar(85, 255, 128),  cv::Scalar(170, 0, 128),
    cv::Scalar(170, 85, 128),  cv::Scalar(170, 170, 128), cv::Scalar(170, 255, 128), cv::Scalar(255, 0, 128),
    cv::Scalar(255, 85, 128),  cv::Scalar(255, 170, 128), cv::Scalar(255, 255, 128), cv::Scalar(0, 85, 255),
    cv::Scalar(0, 170, 255),   cv::Scalar(0, 255, 255),   cv::Scalar(85, 0, 255),    cv::Scalar(85, 85, 255),
    cv::Scalar(85, 170, 255),  cv::Scalar(85, 255, 255),  cv::Scalar(170, 0, 255),   cv::Scalar(170, 85, 255),
    cv::Scalar(170, 170, 255), cv::Scalar(170, 255, 255), cv::Scalar(255, 0, 255),   cv::Scalar(255, 85, 255),
    cv::Scalar(255, 170, 255), cv::Scalar(85, 0, 0),      cv::Scalar(128, 0, 0),     cv::Scalar(170, 0, 0),
    cv::Scalar(212, 0, 0),     cv::Scalar(255, 0, 0),     cv::Scalar(0, 43, 0),      cv::Scalar(0, 85, 0),
    cv::Scalar(0, 128, 0),     cv::Scalar(0, 170, 0),     cv::Scalar(0, 212, 0),     cv::Scalar(0, 255, 0),
    cv::Scalar(0, 0, 43),      cv::Scalar(0, 0, 85),      cv::Scalar(0, 0, 128),     cv::Scalar(0, 0, 170),
    cv::Scalar(0, 0, 212),     cv::Scalar(0, 0, 255),     cv::Scalar(0, 0, 0),       cv::Scalar(36, 36, 36),
    cv::Scalar(73, 73, 73),    cv::Scalar(109, 109, 109), cv::Scalar(146, 146, 146), cv::Scalar(182, 182, 182),
    cv::Scalar(219, 219, 219), cv::Scalar(0, 114, 189),   cv::Scalar(80, 183, 189),  cv::Scalar(128, 128, 0)
};