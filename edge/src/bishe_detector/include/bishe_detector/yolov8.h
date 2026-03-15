# pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudawarping.hpp>

#include "yolo.h"

class YOLOv8 : public YOLO {

public:
    explicit YOLOv8(std::unique_ptr<TrtEngine> trt_engine, float score_treshold = 0.5f, float nms_treshold = 0.5f);
    ~YOLOv8() override;

    // Perform preprocessing, inference and postprocessing
    DetectionResult Detect(cv::Mat& input_img) override;
    cv::Mat PreprocessImage(const cv::Mat& original_img) override;
    cv::Mat PostprocessImage(cv::Mat& output, const cv::Mat& original_img, const cv::Size& original_size) override;

protected:

    void DrawBBoxes(const cv::Mat& image, std::vector<cv::Rect> bboxes, std::vector<int> class_ids, std::vector<float> confidences);
    std::vector<cv::Rect> RescaleBoxes(const std::vector<cv::Rect>& boxes, const cv::Size& original_size, int input_size);
    DetectionResult BuildDetectionResult(cv::Mat& output, const cv::Mat& original_img, const cv::Size& original_size);

    size_t bbox_pred_dim_; //dimension of bounding box encoding
    size_t num_anchors_; 
    size_t input_size_; // input_size of the nn
    float score_treshold_; 
    float nms_treshold_;
    float* input_tensor_{nullptr};
    float* output_tensor_{nullptr};
    cv::Mat host_output_tensor_;

    void AllocateBuffers();
    void ReleaseBuffers();
};
