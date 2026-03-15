#pragma once

#include <vector>
#include <string>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <fstream> 

#include <NvInfer.h>
#include <opencv2/opencv.hpp>


// CUDA API error checking
#define CUDA_CHECK(err)                                                                            \
    do {                                                                                           \
        cudaError_t err_ = (err);                                                                  \
        if (err_ != cudaSuccess) {                                                                 \
            std::printf("CUDA error %d at %s:%d\n", err_, __FILE__, __LINE__);                     \
            throw std::runtime_error("CUDA error");                                                \
        }                                                                                          \
    } while (0)
    

class Logger : public nvinfer1::ILogger {
    void log(Severity severity, const char* msg) noexcept override {
        // Suppress info-level messages
        if (severity <= Severity::kWARNING)
            std::cout << msg << std::endl;
    }
};


template <typename T>
inline void PrintVector(const std::vector<T>& vec) {
    for (const auto& elem : vec) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;
}


template <typename T>
inline size_t getMemSize(nvinfer1::Dims& dim) {
    size_t nbElements{1};
    for(size_t i=0; i<dim.nbDims; ++i) {
        nbElements *= dim.d[i];
    }
    return nbElements * sizeof(T);
}