#include "trt_engine.h"

TrtEngine::TrtEngine(Logger logger) : logger_{logger} {}


void TrtEngine::BuildEngine(const std::string& onnx_filepath, const std::string& engine_filepath, TrtPrecision precision) {

    auto builder = std::unique_ptr<nvinfer1::IBuilder>(nvinfer1::createInferBuilder(logger_));
    if(!builder) throw std::runtime_error("Error during builder instantiation");

    auto network = std::unique_ptr<nvinfer1::INetworkDefinition>(builder->createNetworkV2(0));
    if(!network) throw std::runtime_error("Error during network instantiation");

    auto parser = std::unique_ptr<nvonnxparser::IParser>(nvonnxparser::createParser(*network, logger_));
    if(!parser) throw std::runtime_error("Error during parser instantiation");

    // parse onnx file and save it into network
    auto parsed = parser->parseFromFile(onnx_filepath.c_str(), 2);
    if(!parsed) {
        for (size_t i=0; i < parser->getNbErrors(); ++i) {
            std::cerr << "Parser error: " << parser->getError(i)->desc() << std::endl;
        }
        throw std::runtime_error("onnx file not parsed");
    }


    auto config = std::unique_ptr<nvinfer1::IBuilderConfig>(builder->createBuilderConfig());
    if(!config) throw std::runtime_error("Error during config instantiation");

    if(precision == TrtPrecision::kFP16) {
        if(builder->platformHasFastFp16()) {
            config->setFlag(nvinfer1::BuilderFlag::kFP16); 
        } else {
            std::cerr << "fp16 quantization not supported by device \n";
        }
    }

    if(precision == TrtPrecision::kINT8) {
        if(builder->platformHasFastInt8()) {
            config->setFlag((nvinfer1::BuilderFlag::kINT8));
            calibrator_ = std::make_unique<Int8EntropyCalibrator2>(1, 640, 640, "../samples/", "calib_table", "input_image");
            config->setInt8Calibrator(calibrator_.get());
        } else {
            std::cerr << "int8 quantization not supported by device \n";
        }
    }


    // serialize network
    auto plan = std::unique_ptr<nvinfer1::IHostMemory>(builder->buildSerializedNetwork(*network, *config));
    if(!plan) throw std::runtime_error("Error during plan instantiation");

    std::ofstream engine_file{engine_filepath.c_str(), std::ios::binary};
    if(!engine_file) throw std::runtime_error("Could not open plan output file");

    engine_file.write(reinterpret_cast<const char*>(plan->data()), plan->size());
}

void TrtEngine::LoadEngine(const std::string& engine_filepath) {

    std::ifstream file{engine_filepath, std::ios::binary | std::ios::ate};
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        auto msg = "Error, unable to read engine file";
        throw std::runtime_error("Error, unable to read engine file");
    }

    runtime_ = std::unique_ptr<nvinfer1::IRuntime>(nvinfer1::createInferRuntime(logger_));
    if (!runtime_) throw std::runtime_error("Error during parser instantiation");

    engine_ = std::unique_ptr<nvinfer1::ICudaEngine>(runtime_->deserializeCudaEngine(buffer.data(), buffer.size()));

    context_ = std::unique_ptr<nvinfer1::IExecutionContext>(engine_->createExecutionContext());

    assert(engine_->getNbIOTensors() == 2); // only support 1 input and 1 output tensor
    assert(engine_->getTensorIOMode(engine_->getIOTensorName(0)) == nvinfer1::TensorIOMode::kINPUT); // first tensor must be input
    assert(engine_->getTensorIOMode(engine_->getIOTensorName(1)) == nvinfer1::TensorIOMode::kOUTPUT); // second tensor must be output

    input_tensor_name_ = engine_->getIOTensorName(0);
    output_tensor_name_ = engine_->getIOTensorName(1);
}



void TrtEngine::AsyncInference(cudaStream_t stream, void *input_tensor, void *output_tensor) {
    context_->setInputTensorAddress(input_tensor_name_.c_str(), input_tensor);
    context_->setOutputTensorAddress(output_tensor_name_.c_str(), output_tensor);

    // Ensure all dynamic bindings have been defined.
    if (!context_->allInputDimensionsSpecified()) {
        throw std::runtime_error("Not all input dimensions specified");
    }

    context_->enqueueV3(stream);
}



void TrtEngine::Inference(void *input_tensor, void *output_tensor) {
    void* bindings[] = { input_tensor, output_tensor };
    context_->executeV2(bindings);
}


std::vector<size_t> TrtEngine::GetInputDim() {
    std::vector<size_t> input_dim;
    if(engine_) {
        const nvinfer1::Dims dim = engine_->getTensorShape(input_tensor_name_.c_str());
        for(size_t i=0; i<dim.nbDims; ++i) {
            input_dim.push_back(dim.d[i]);
        }
    } else {
        input_dim.push_back(0);
    }
    return input_dim;
}

std::vector<size_t> TrtEngine::GetOutputDim() {
    std::vector<size_t> output_dim;
    if(engine_) {
        const nvinfer1::Dims dim = engine_->getTensorShape(output_tensor_name_.c_str());
        for(size_t i=0; i<dim.nbDims; ++i) {
            output_dim.push_back(dim.d[i]);
        }
    } else {
        output_dim.push_back(0);
    }
    return output_dim;
}