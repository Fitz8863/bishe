
#include "int8_calibrator.h"

Int8EntropyCalibrator2::Int8EntropyCalibrator2(int batchsize, int input_w, int input_h, const char* img_dir, const char* calib_table_name, const char* input_blob_name)
    : batch_size_(batchsize)
    , input_w_(input_w)
    , input_h_(input_h)
    , img_idx_(0)
    , img_dir_(img_dir)
    , calib_table_name_(calib_table_name)
    , input_blob_name_(input_blob_name) {
    input_count_ = 3 * input_w * input_h * batchsize;
    cudaMalloc(&device_input_, input_count_ * sizeof(float));
    
    // kinda ugly hardcoding 
    img_files_ = { 
        "image1.jpg",
        "image2.jpg",
        "image3.jpg",
        "image4.jpg",
        "image5.jpg"
    };
}

Int8EntropyCalibrator2::~Int8EntropyCalibrator2() {
    cudaFree(device_input_);
}

int Int8EntropyCalibrator2::getBatchSize() const noexcept {
    return batch_size_;
}

bool Int8EntropyCalibrator2::getBatch(void* bindings[], const char* names[], int nbBindings) noexcept {
    if (img_idx_ + batch_size_ > (int)img_files_.size()) {
        return false;
    }

    std::vector<cv::Mat> input_imgs_;
    for (int i = img_idx_; i < img_idx_ + batch_size_; ++i) {
        cv::Mat img = cv::imread(img_dir_ + img_files_[i]);
        if (img.empty()){
            std::cerr << "Cannot open Image!" << std::endl;
            return false;
        }

        cv::Mat resized_img;
        cv::resize(img, resized_img, cv::Size(input_w_, input_h_));
        input_imgs_.push_back(resized_img);
    }

    img_idx_ += batch_size_;
    cv::Mat blob = cv::dnn::blobFromImages(input_imgs_);

    cudaMemcpy(device_input_, blob.ptr<float>(0), input_count_ * sizeof(float), cudaMemcpyHostToDevice);
    bindings[0] = device_input_;
    
    return true;
}

const void* Int8EntropyCalibrator2::readCalibrationCache(size_t& length) noexcept
{
    std::cout << "Reading calibration cache: " << calib_table_name_ << std::endl;
    calib_cache_.clear();
    std::ifstream input(calib_table_name_, std::ios::binary);
    input >> std::noskipws;

    if (input.good()) {
        std::copy(std::istream_iterator<char>(input), std::istream_iterator<char>(), std::back_inserter(calib_cache_));
    }
    length = calib_cache_.size();
    return length ? calib_cache_.data() : nullptr;
}

void Int8EntropyCalibrator2::writeCalibrationCache(const void* cache, size_t length) noexcept
{
    std::cout << "Writing calibration cache: " << calib_table_name_ << " size: " << length << std::endl;
    std::ofstream output(calib_table_name_, std::ios::binary);
    output.write(reinterpret_cast<const char*>(cache), length);
}