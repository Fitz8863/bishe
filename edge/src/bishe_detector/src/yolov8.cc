#include "yolov8.h"

YOLOv8::YOLOv8(std::unique_ptr<TrtEngine> trt_engine, float score_treshold, float nms_treshold) : YOLO(std::move(trt_engine)), score_treshold_(score_treshold), nms_treshold_(nms_treshold) {
    std::vector<size_t> input_dim = trt_engine_->GetInputDim();
    assert(input_dim.size() == 4); // support only dim [batch_size, channels, height, width]
    assert(input_dim[0] == 1); // support only batch size of 1
    assert(input_dim[1] == 3); // support only 3 channels
    assert(input_dim[2] == input_dim[3]); // support only square images

    std::vector<size_t> output_dim = trt_engine_->GetOutputDim();
    assert(output_dim.size() == 3); // support only dim [batch_size, xyxy + classes scores]
    assert(output_dim[0] == 1); // support only batch_size of 1
    assert(output_dim[1] == 84); // support only xyxy + class scores[80]

    input_size_ = input_dim[2];
    bbox_pred_dim_ = output_dim[1];
    num_anchors_ = output_dim[2];
    AllocateBuffers();
}

YOLOv8::~YOLOv8() {
    ReleaseBuffers();
}

void YOLOv8::AllocateBuffers() {
    CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&input_tensor_), sizeof(float) * 3 * input_size_ * input_size_));
    CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&output_tensor_), sizeof(float) * bbox_pred_dim_ * num_anchors_));
    host_output_tensor_ = cv::Mat{ cv::Size{static_cast<int>(num_anchors_), static_cast<int>(bbox_pred_dim_)}, CV_32F };
}

void YOLOv8::ReleaseBuffers() {
    if (input_tensor_ != nullptr) {
        CUDA_CHECK(cudaFree(input_tensor_));
        input_tensor_ = nullptr;
    }
    if (output_tensor_ != nullptr) {
        CUDA_CHECK(cudaFree(output_tensor_));
        output_tensor_ = nullptr;
    }
}

cv::Mat YOLOv8::PreprocessImage(const cv::Mat& original_img) {
    cv::Mat blob;
    cv::dnn::blobFromImage(
        original_img,
        blob,
        1.0 / 255.0,
        cv::Size(input_size_, input_size_),
        cv::Scalar(),
        true,
        false,
        CV_32F);
    return blob;
}


void YOLOv8::DrawBBoxes(const cv::Mat& image, std::vector<cv::Rect> boxes, std::vector<int> class_ids, std::vector<float> confidences) {

    // bboxes, class_ids and confidences need to be the same size
    assert(boxes.size() == class_ids.size());
    assert(boxes.size() == confidences.size());

    for (size_t i = 0; i < boxes.size(); ++i) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(2) << confidences[i];
        std::string conf_str = stream.str();

        cv::rectangle(image, boxes[i], kClassColors[class_ids[i]], 3);
        cv::putText(image, kClassNames[class_ids[i]] + " " + conf_str, cv::Point(boxes[i].x, boxes[i].y - 10),
            cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(255, 255, 255), 2);
    }
}

std::vector<cv::Rect> YOLOv8::RescaleBoxes(const std::vector<cv::Rect>& boxes, const cv::Size& original_size, int input_size) {
    std::vector<cv::Rect> rescaled_boxes;

    // Scale factors
    float scale_x = static_cast<float>(original_size.width) / input_size;
    float scale_y = static_cast<float>(original_size.height) / input_size;

    // Rescale each box
    for (const auto& box : boxes) {
        cv::Rect2f rescaled_box;
        rescaled_box.x = box.x * scale_x;
        rescaled_box.y = box.y * scale_y;
        rescaled_box.width = box.width * scale_x;
        rescaled_box.height = box.height * scale_y;
        rescaled_boxes.push_back(rescaled_box);
    }

    return rescaled_boxes;
}


DetectionResult YOLOv8::BuildDetectionResult(cv::Mat& output, const cv::Mat& original_img, const cv::Size& original_size) {

    std::vector<cv::Rect> boxes;
    std::vector<int> class_ids;
    std::vector<float> scores;

    cv::Point class_id;
    double max_score;

    for (int i = 0; i < output.rows; ++i) {

        float* row_ptr = output.ptr<float>(i);
        cv::Mat row_scores(1, kClassNames.size(), CV_32F, row_ptr + 4);

        // compute argmax which is class_id
        cv::minMaxLoc(row_scores, 0, &max_score, 0, &class_id);

        if (max_score < score_treshold_) continue;

        // int x1 = static_cast<int>(output.at<float>(i, 0));
        // int y1 = static_cast<int>(output.at<float>(i, 1));
        // int x2 = static_cast<int>(output.at<float>(i, 2));
        // int y2 = static_cast<int>(output.at<float>(i, 3));

        float cx = output.at<float>(i, 0);
        float cy = output.at<float>(i, 1);
        float w = output.at<float>(i, 2);
        float h = output.at<float>(i, 3);

        int x1 = static_cast<int>(cx - w / 2);
        int y1 = static_cast<int>(cy - h / 2);
        int x2 = static_cast<int>(cx + w / 2);
        int y2 = static_cast<int>(cy + h / 2);

        boxes.push_back(cv::Rect{ cv::Point{x1, y1}, cv::Point{x2, y2} });
        class_ids.push_back(class_id.x);
        scores.push_back(max_score);
    }

    std::vector<int> nms_result;
    cv::dnn::NMSBoxes(boxes, scores, score_treshold_, nms_treshold_, nms_result);

    std::vector<cv::Rect> filtered_boxes;
    std::vector<int> filtered_class_ids;
    std::vector<float> filtered_scores;

    // Filter out boxes from NMS
    for (size_t i = 0; i < nms_result.size(); ++i) {
        int idx = nms_result[i];
        filtered_boxes.push_back(boxes[idx]);
        filtered_class_ids.push_back(class_ids[idx]);
        filtered_scores.push_back(scores[idx]);
    }

    boxes = RescaleBoxes(filtered_boxes, original_size, input_size_);

    cv::Mat output_img{ original_img };
    DrawBBoxes(output_img, boxes, filtered_class_ids, filtered_scores);

    DetectionResult result;
    result.annotated_image = output_img;
    result.detections.reserve(boxes.size());

    for (size_t i = 0; i < boxes.size(); ++i) {
        DetectionBox detection;
        detection.bbox = boxes[i];
        detection.class_id = filtered_class_ids[i];
        detection.class_name = kClassNames[filtered_class_ids[i]];
        detection.confidence = filtered_scores[i];
        result.detections.push_back(std::move(detection));
    }

    return result;
}

cv::Mat YOLOv8::PostprocessImage(cv::Mat& output, const cv::Mat& original_img, const cv::Size& original_size) {
    return BuildDetectionResult(output, original_img, original_size).annotated_image;
}

DetectionResult YOLOv8::Detect(cv::Mat& input_img) {

    cv::Size original_size = input_img.size();

    cv::Mat preprocessed_img = PreprocessImage(input_img);

    CUDA_CHECK(cudaMemcpy(input_tensor_, preprocessed_img.ptr<float>(0), sizeof(float) * 3 * input_size_ * input_size_, cudaMemcpyHostToDevice));

    trt_engine_->Inference(input_tensor_, output_tensor_);

    CUDA_CHECK(cudaMemcpy(host_output_tensor_.ptr<float>(0), output_tensor_, sizeof(float) * bbox_pred_dim_ * num_anchors_, cudaMemcpyDeviceToHost));

    // transpose to optimize caching when iterate over rows
    cv::Mat transposed_output = host_output_tensor_.t();

    DetectionResult detection_result = BuildDetectionResult(transposed_output, input_img, original_size);

    return detection_result;
}
