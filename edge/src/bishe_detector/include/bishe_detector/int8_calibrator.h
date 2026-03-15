#pragma once

#include <iterator>
#include "NvInfer.h"
#include "utils.h"

// Calibrator class for performing calibration during int8 quantization of a neural network
//
class Int8EntropyCalibrator2 : public nvinfer1::IInt8EntropyCalibrator2
{
public:
    Int8EntropyCalibrator2(int batch_size, int input_w, int input_h, const char* img_dir, const char* calib_table_name, const char* input_blob_name);

    virtual ~Int8EntropyCalibrator2();
    int getBatchSize() const noexcept override;
    bool getBatch(void* bindings[], const char* names[], int nbBindings) noexcept override;
    
    const void* readCalibrationCache(size_t& length) noexcept override;
    void writeCalibrationCache(const void* cache, size_t length) noexcept override;

private:
    int batch_size_;
    int input_w_;
    int input_h_;
    int img_idx_;

    std::string img_dir_; // calibration data dir
    std::vector<std::string> img_files_; // calibration images
    size_t input_count_; 
    std::string calib_table_name_;
    const char* input_blob_name_;
    bool read_cache_;
    void* device_input_;
    std::vector<char> calib_cache_;
};
