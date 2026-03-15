# pragma once

#include "trt_engine.h"

struct DetectionBox {
    cv::Rect bbox;
    int class_id;
    std::string class_name;
    float confidence;
};

struct DetectionResult {
    cv::Mat annotated_image;
    std::vector<DetectionBox> detections;
};


class YOLO {

public:
    YOLO(std::unique_ptr<TrtEngine> trt_engine);
    virtual ~YOLO() = default;

    virtual DetectionResult Detect(cv::Mat& input_img) = 0;
    virtual cv::Mat PreprocessImage(const cv::Mat& original_img) = 0;
    virtual cv::Mat PostprocessImage(cv::Mat& output, const cv::Mat& original_img, const cv::Size& original_size) = 0;

protected:

    void DrawBBoxes(const cv::Mat& image, std::vector<cv::Rect> bboxes, std::vector<int> class_ids, std::vector<float> confidences);
    std::vector<cv::Rect2f> RescaleBoxes(const std::vector<cv::Rect>& boxes, const cv::Size& original_size, int input_size);

    std::unique_ptr<TrtEngine> trt_engine_;

    static const std::array<std::string, 80> kClassNames;
    static const std::array<cv::Scalar, 80> kClassColors;
};
