#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/image_transport.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include "bishe_msgs/msg/detector_result.hpp"
#include <chrono>
#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cmath>

// YOLO Headers
#include "trt_engine.h"
#include "yolov8.h"

class DetectorNode : public rclcpp::Node
{
public:
  DetectorNode()
      : Node("detector_node")
  {
    // Declare parameters
    this->declare_parameter<float>("confidence_threshold", 0.5);
    this->declare_parameter<float>("nms_threshold", 0.5);
    this->declare_parameter<std::string>("engine_path", "/home/jetson/projects/bishe/models/yolov8s.engine");
    this->declare_parameter<int>("worker_threads", 1);
    this->declare_parameter<int>("max_queue_size", 8);

    this->get_parameter("confidence_threshold", confidence_threshold_);
    this->get_parameter("nms_threshold", nms_threshold_);
    this->get_parameter("engine_path", engine_path_);
    this->get_parameter("worker_threads", worker_threads_);
    this->get_parameter("max_queue_size", max_queue_size_);

    if (worker_threads_ < 1) {
      worker_threads_ = 1;
    }
    if (max_queue_size_ < 1) {
      max_queue_size_ = 1;
    }

    RCLCPP_INFO(this->get_logger(), "加载engine模型: %s", engine_path_.c_str());
    for (int i = 0; i < worker_threads_; ++i) {
      Logger logger;
      auto trt_engine = std::make_unique<TrtEngine>(logger);
      trt_engine->LoadEngine(engine_path_);
      workers_.push_back(std::make_unique<WorkerContext>(WorkerContext{std::make_unique<YOLOv8>(std::move(trt_engine), confidence_threshold_, nms_threshold_)}));
    }

    for (int i = 0; i < worker_threads_; ++i) {
      worker_threads_pool_.emplace_back(&DetectorNode::workerLoop, this, i);
    }

    // Subscribe to camera images
    image_sub_ = image_transport::create_subscription(this, "camera/image_raw", std::bind(&DetectorNode::imageCallback, this, std::placeholders::_1), "raw", rmw_qos_profile_sensor_data);

    // Publisher for detection results
    result_pub_ = this->create_publisher<bishe_msgs::msg::DetectorResult>("detector/result", 10);

    RCLCPP_INFO(this->get_logger(), "检测节点开始，置信度阈值: %.2f, NMS阈值: %.2f, worker_threads: %d, max_queue_size: %d", confidence_threshold_, nms_threshold_, worker_threads_, max_queue_size_);
  }

  ~DetectorNode() override
  {
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      stop_workers_ = true;
    }
    queue_cv_.notify_all();
    for (auto &t : worker_threads_pool_) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

private:

  struct FrameTask {
    sensor_msgs::msg::Image::ConstSharedPtr msg;
    std::chrono::steady_clock::time_point enqueue_time;
  };

  struct WorkerContext {
    std::unique_ptr<YOLOv8> yolo;
  };

  image_transport::Subscriber image_sub_;
  rclcpp::Publisher<bishe_msgs::msg::DetectorResult>::SharedPtr result_pub_;
  float confidence_threshold_;
  float nms_threshold_;
  std::string engine_path_;
  int worker_threads_{1};
  int max_queue_size_{8};
  std::vector<std::unique_ptr<WorkerContext>> workers_;
  std::vector<std::thread> worker_threads_pool_;
  std::deque<FrameTask> queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  bool stop_workers_{false};

  std::atomic<uint64_t> input_frames_{0};
  std::atomic<uint64_t> output_frames_{0};
  std::atomic<uint64_t> dropped_frames_{0};
  std::atomic<uint64_t> processed_frames_{0};
  double inference_time_ms_acc_{0.0};
  std::mutex stats_mutex_;
  std::chrono::steady_clock::time_point stats_window_start_{std::chrono::steady_clock::now()};

  void reportStats()
  {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - stats_window_start_).count();
    if (elapsed_ms < 1000) {
      return;
    }

    const double seconds = static_cast<double>(elapsed_ms) / 1000.0;
    const uint64_t in = input_frames_.exchange(0);
    const uint64_t out = output_frames_.exchange(0);
    const uint64_t dropped = dropped_frames_.exchange(0);
    const uint64_t processed = processed_frames_.exchange(0);
    double inf_acc = 0.0;
    {
      std::lock_guard<std::mutex> lock(stats_mutex_);
      inf_acc = inference_time_ms_acc_;
      inference_time_ms_acc_ = 0.0;
    }
    const double avg_inf = processed > 0 ? inf_acc / static_cast<double>(processed) : 0.0;

    size_t qsize = 0;
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      qsize = queue_.size();
    }

    RCLCPP_INFO(this->get_logger(), "detector 输入FPS: %.2f, 输出FPS: %.2f, 平均推理耗时: %.2fms, 丢帧: %lu, 队列: %zu",
                static_cast<double>(in) / seconds,
                static_cast<double>(out) / seconds,
                avg_inf,
                static_cast<unsigned long>(dropped),
                qsize);
    stats_window_start_ = now;
  }

  void workerLoop(int worker_idx)
  {
    auto &worker = workers_.at(static_cast<size_t>(worker_idx));
    while (rclcpp::ok()) {
      FrameTask task;
      {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this]() { return stop_workers_ || !queue_.empty(); });
        if (stop_workers_ && queue_.empty()) {
          return;
        }
        task = std::move(queue_.front());
        queue_.pop_front();
      }

      const auto t0 = std::chrono::steady_clock::now();
      cv::Mat frame = cv_bridge::toCvShare(task.msg, "bgr8")->image;
      DetectionResult detection_result = worker->yolo->Detect(frame);
      const auto t1 = std::chrono::steady_clock::now();
      const double inf_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;
      {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        inference_time_ms_acc_ += inf_ms;
      }
      processed_frames_.fetch_add(1);

      auto result = bishe_msgs::msg::DetectorResult();
      result.has_violation = !detection_result.detections.empty();
      result.confidence = detection_result.detections.empty() ? 0.0f : detection_result.detections.front().confidence;
      result.nms_threshold = this->nms_threshold_;
      result.violation_type = detection_result.detections.empty() ? "" : detection_result.detections.front().class_name;
      result.annotated_image = *cv_bridge::CvImage(task.msg->header, "bgr8", detection_result.annotated_image).toImageMsg();

      result_pub_->publish(result);
      output_frames_.fetch_add(1);
      // reportStats();
    }
  }

  void imageCallback(const sensor_msgs::msg::Image::ConstSharedPtr &msg)
  {
    try
    {
      input_frames_.fetch_add(1);
      FrameTask task;
      task.msg = msg;
      task.enqueue_time = std::chrono::steady_clock::now();

      {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (queue_.size() >= static_cast<size_t>(max_queue_size_)) {
          queue_.pop_front();
          dropped_frames_.fetch_add(1);
        }
        queue_.push_back(std::move(task));
      }
      queue_cv_.notify_one();
      // reportStats();
    }
    catch (const cv_bridge::Exception &e)
    {
      RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
    }
    catch (const std::exception &e)
    {
      RCLCPP_ERROR(this->get_logger(), "YOLO inference exception: %s", e.what());
    }
  }
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<DetectorNode>());
  rclcpp::shutdown();
  return 0;
}
