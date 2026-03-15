// yolov5a.h
#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudawarping.hpp>

#include "yolo.h"

class YOLOv5A : public YOLO {

public:
    explicit YOLOv5A(std::unique_ptr<TrtEngine> trt_engine);

    DetectionResult Detect(cv::Mat& input_img) override;
    cv::Mat PreprocessImage(const cv::Mat& original_img) override;
    cv::Mat PostprocessImage(cv::Mat& output, const cv::Mat& original_img, const cv::Size& original_size) override;

private:
    void DrawBBoxes(const cv::Mat& image, std::vector<cv::Rect> bboxes, std::vector<int> class_ids, 
                   std::vector<float> confidences, std::vector<std::vector<cv::Point2f>> landmarks,
                   std::vector<int> colors);
    
    std::vector<cv::Rect> RescaleBoxes(const std::vector<cv::Rect>& boxes, const cv::Size& original_size, int input_size);
    std::vector<std::vector<cv::Point2f>> RescaleLandmarks(const std::vector<std::vector<cv::Point2f>>& landmarks, 
                                                         const cv::Size& original_size, int input_size);
    
    double sigmoid(double x);
    double calculateIOU(const cv::Rect& r1, const cv::Rect& r2);

    size_t input_size_;
    float conf_threshold_;
    float nms_threshold_;
    
    // 颜色类别
    static const std::array<std::string, 4> kColorNames;
    static const std::array<cv::Scalar, 4> kColorColors;
    
    // 数字类别（根据OpenVINO代码）
    static const std::array<std::string, 9> kNumberNames;
};
