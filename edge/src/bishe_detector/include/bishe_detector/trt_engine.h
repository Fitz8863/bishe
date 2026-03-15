#pragma once

#include <string>
#include <memory>

#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime.h>

#include "utils.h"
#include "int8_calibrator.h"

enum class TrtPrecision {
    kFP32,
    kFP16,
    kINT8
};


// Encapsulate most of TensorRT usage
// TrtEngine can build and run any model from onnx file
class TrtEngine {

public:
    TrtEngine(Logger logger);

    void BuildEngine(const std::string& onnx_filepath, const std::string& engine_filepath, TrtPrecision precision);
    void LoadEngine(const std::string& engine_filepath);
    void Inference(void *input_tensor, void *output_tensor);
    void AsyncInference(cudaStream_t stream, void *input_tensor, void *output_tensor);

    // Need to call `LoadEngine` before, otherwise return 0
    std::vector<size_t> GetInputDim();
    
    // Need to call `LoadEngine` before, otherwise return 0
    std::vector<size_t> GetOutputDim();

private:
    Logger logger_;

    std::string input_tensor_name_;
    std::string output_tensor_name_;

    std::unique_ptr<nvinfer1::IRuntime> runtime_;
    std::unique_ptr<nvinfer1::ICudaEngine> engine_;
    std::unique_ptr<nvinfer1::IExecutionContext> context_;

    std::unique_ptr<Int8EntropyCalibrator2> calibrator_;
};