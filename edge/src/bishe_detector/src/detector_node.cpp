#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include "bishe_msgs/msg/detector_result.hpp"
#include "bishe_msgs/msg/shared_frame_ref.hpp"
#include <chrono>
#include <thread>
#include <deque>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cmath>
#include <algorithm>

#include <rclcpp_components/register_node_macro.hpp>

#include "bishe_detector/detection_gate.hpp"
#include "bishe_detector/detector_result_utils.hpp"
#include "bishe_detector/detector_node_factory.hpp"
#include "bishe_msgs/shared_frame_ring.hpp"

// YOLO Headers
#include "trt_engine.h"
#include "yolov8.h"

namespace bishe_detector
{

class DetectorNode : public rclcpp::Node
{
public:
  explicit DetectorNode(const rclcpp::NodeOptions &options)
      : Node("detector_node", options)
  {
    // 声明 ROS 2 参数
    // confidence_threshold: AI 检测置信度阈值，低于此值的检测结果将被忽略
    this->declare_parameter<float>("confidence_threshold", 0.5);
    // nms_threshold: 非极大值抑制阈值，用于过滤重叠的边界框
    this->declare_parameter<float>("nms_threshold", 0.5);
    // sampling_interval_ms: 常规采样间隔（毫秒）。在正常巡检状态下，每隔此时间处理一帧，用于节省算力
    this->declare_parameter<int>("sampling_interval_ms", 1000);
    // lock_duration_ms: 检测锁定时间（毫秒）。一旦检测到违规，在此时间内会提高检测频率
    this->declare_parameter<int>("lock_duration_ms", 3000);
    // engine_path: TensorRT 模型引擎文件的绝对路径
    this->declare_parameter<std::string>("engine_path", "/home/jetson/projects/bishe/models/yolov8s.engine");
    // input_topic: 输入图像引用的 ROS 主题
    this->declare_parameter<std::string>("input_topic", "camera/detector_frame_ref");
    // detector_width/height: 模型要求的输入分辨率
    this->declare_parameter<int>("detector_width", 640);
    this->declare_parameter<int>("detector_height", 360);
    // shared_memory_name: 用于零拷贝图像传输的共享内存名称
    this->declare_parameter<std::string>("shared_memory_name", "/camera_001_detector_shm");
    // worker_threads: 并行推理的线程数量
    this->declare_parameter<int>("worker_threads", 1);
    // max_queue_size: 待处理任务队列的最大长度，防止 OOM
    this->declare_parameter<int>("max_queue_size", 8);

    float confidence_threshold = 0.5f;
    float nms_threshold = 0.5f;
    int sampling_interval_ms = sampling_interval_ms_;
    int lock_duration_ms = lock_duration_ms_;
    this->get_parameter("confidence_threshold", confidence_threshold);
    this->get_parameter("nms_threshold", nms_threshold);
    this->get_parameter("sampling_interval_ms", sampling_interval_ms);
    this->get_parameter("lock_duration_ms", lock_duration_ms);
    this->get_parameter("engine_path", engine_path_);
    this->get_parameter("input_topic", input_topic_);
    this->get_parameter("detector_width", detector_width_);
    this->get_parameter("detector_height", detector_height_);
    this->get_parameter("shared_memory_name", shared_memory_name_);
    this->get_parameter("worker_threads", worker_threads_);
    this->get_parameter("max_queue_size", max_queue_size_);
    confidence_threshold_.store(confidence_threshold);
    nms_threshold_.store(nms_threshold);
    sampling_interval_ms_ = std::max(1, sampling_interval_ms);
    lock_duration_ms_ = std::max(0, lock_duration_ms);

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
      workers_.push_back(std::make_unique<WorkerContext>(WorkerContext{std::make_unique<YOLOv8>(std::move(trt_engine), confidence_threshold_.load(), nms_threshold_.load())}));
    }

    for (int i = 0; i < worker_threads_; ++i) {
      worker_threads_pool_.emplace_back(&DetectorNode::workerLoop, this, i);
    }

    bishe_msgs::shared_memory::SharedFrameRingConfig ring_config;
    ring_config.shm_name = shared_memory_name_;
    ring_config.slot_count = 8;
    ring_config.width = static_cast<uint32_t>(detector_width_);
    ring_config.height = static_cast<uint32_t>(detector_height_);
    ring_config.channels = 3;
    detector_ring_ = std::make_unique<bishe_msgs::shared_memory::SharedFrameRing>(ring_config, false);

    // Subscribe to camera shared-frame metadata
    image_sub_ = this->create_subscription<bishe_msgs::msg::SharedFrameRef>(
      input_topic_,
      rclcpp::QoS(16).reliable(),
      std::bind(&DetectorNode::imageCallback, this, std::placeholders::_1));

    // Publisher for detection results
    result_pub_ = this->create_publisher<bishe_msgs::msg::DetectorResult>("detector/result", 10);

    parameter_callback_handle_ = this->add_on_set_parameters_callback(
      std::bind(&DetectorNode::handleParameterUpdate, this, std::placeholders::_1));

    {
      std::lock_guard<std::mutex> lock(gate_mutex_);
      detection_gate_.update_config(
        std::chrono::milliseconds{sampling_interval_ms_},
        std::chrono::milliseconds{lock_duration_ms_},
        std::chrono::steady_clock::time_point{});
    }

    RCLCPP_INFO(this->get_logger(),
      "检测节点开始，置信度阈值: %.2f, NMS阈值: %.2f, sampling_interval_ms: %d, lock_duration_ms: %d, worker_threads: %d, max_queue_size: %d",
      confidence_threshold_.load(), nms_threshold_.load(), sampling_interval_ms_, lock_duration_ms_, worker_threads_, max_queue_size_);
    RCLCPP_INFO(this->get_logger(), "检测输入topic: %s", input_topic_.c_str());
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
    bishe_msgs::msg::SharedFrameRef ref;
    std::chrono::steady_clock::time_point enqueue_time;
  };

  struct WorkerContext {
    std::unique_ptr<YOLOv8> yolo;
  };

  rclcpp::Subscription<bishe_msgs::msg::SharedFrameRef>::SharedPtr image_sub_;
  rclcpp::Publisher<bishe_msgs::msg::DetectorResult>::SharedPtr result_pub_;
  std::atomic<float> confidence_threshold_{0.5f};
  std::atomic<float> nms_threshold_{0.5f};
  std::string engine_path_;
  std::string input_topic_;
  int detector_width_{640};
  int detector_height_{360};
  std::string shared_memory_name_;
  int worker_threads_{1};
  int max_queue_size_{8};
  std::vector<std::unique_ptr<WorkerContext>> workers_;
  std::vector<std::thread> worker_threads_pool_;
  std::deque<FrameTask> queue_;
  std::mutex queue_mutex_;
  std::mutex gate_mutex_;
  std::condition_variable queue_cv_;
  bool stop_workers_{false};

  std::atomic<uint64_t> input_frames_{0};
  std::atomic<uint64_t> output_frames_{0};
  std::atomic<uint64_t> dropped_frames_{0};
  std::atomic<uint64_t> processed_frames_{0};
  double inference_time_ms_acc_{0.0};
  std::mutex stats_mutex_;
  std::chrono::steady_clock::time_point stats_window_start_{std::chrono::steady_clock::now()};
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr parameter_callback_handle_;
  int sampling_interval_ms_{1000};
  int lock_duration_ms_{3000};
  DetectionGate detection_gate_{std::chrono::milliseconds{1000}, std::chrono::milliseconds{3000}};
  std::unique_ptr<bishe_msgs::shared_memory::SharedFrameRing> detector_ring_;

  sensor_msgs::msg::Image::UniquePtr buildImageFromSharedFrame(const bishe_msgs::msg::SharedFrameRef &ref)
  {
    bishe_msgs::shared_memory::SharedFrameView view;
    if (!detector_ring_->viewSlot(ref.slot_index, ref.sequence, view)) {
      return nullptr;
    }

    auto image = std::make_unique<sensor_msgs::msg::Image>();
    image->header = ref.header;
    image->width = view.width;
    image->height = view.height;
    image->step = view.step;
    image->encoding = view.encoding;
    image->is_bigendian = false;
    image->data.resize(view.bytes_used);
    std::memcpy(image->data.data(), view.data, view.bytes_used);
    return image;
  }

  rcl_interfaces::msg::SetParametersResult handleParameterUpdate(const std::vector<rclcpp::Parameter> &parameters)
  {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;

    float new_confidence_threshold = confidence_threshold_.load();
    float new_nms_threshold = nms_threshold_.load();
    int new_sampling_interval_ms = sampling_interval_ms_;
    int new_lock_duration_ms = lock_duration_ms_;
    bool thresholds_changed = false;
    bool gate_changed = false;

    for (const auto &parameter : parameters) {
      if (parameter.get_name() == "confidence_threshold") {
        double value = 0.0;
        if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE) {
          value = parameter.as_double();
        } else if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
          value = static_cast<double>(parameter.as_int());
        } else {
          result.successful = false;
          result.reason = "confidence_threshold 必须是数字";
          return result;
        }

        if (value < 0.0 || value > 1.0) {
          result.successful = false;
          result.reason = "confidence_threshold 必须在 0 到 1 之间";
          return result;
        }

        new_confidence_threshold = static_cast<float>(value);
        thresholds_changed = true;
      } else if (parameter.get_name() == "nms_threshold") {
        double value = 0.0;
        if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE) {
          value = parameter.as_double();
        } else if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
          value = static_cast<double>(parameter.as_int());
        } else {
          result.successful = false;
          result.reason = "nms_threshold 必须是数字";
          return result;
        }

        if (value < 0.0 || value > 1.0) {
          result.successful = false;
          result.reason = "nms_threshold 必须在 0 到 1 之间";
          return result;
        }

        new_nms_threshold = static_cast<float>(value);
        thresholds_changed = true;
      } else if (parameter.get_name() == "sampling_interval_ms") {
        if (parameter.get_type() != rclcpp::ParameterType::PARAMETER_INTEGER) {
          result.successful = false;
          result.reason = "sampling_interval_ms 必须是整数";
          return result;
        }

        const int value = static_cast<int>(parameter.as_int());
        if (value < 1) {
          result.successful = false;
          result.reason = "sampling_interval_ms 必须 >= 1";
          return result;
        }

        new_sampling_interval_ms = value;
        gate_changed = true;
      } else if (parameter.get_name() == "lock_duration_ms") {
        if (parameter.get_type() != rclcpp::ParameterType::PARAMETER_INTEGER) {
          result.successful = false;
          result.reason = "lock_duration_ms 必须是整数";
          return result;
        }

        const int value = static_cast<int>(parameter.as_int());
        if (value < 0) {
          result.successful = false;
          result.reason = "lock_duration_ms 必须 >= 0";
          return result;
        }

        new_lock_duration_ms = value;
        gate_changed = true;
      }
    }

    if (thresholds_changed) {
      confidence_threshold_.store(new_confidence_threshold);
      nms_threshold_.store(new_nms_threshold);
      for (auto &worker : workers_) {
        worker->yolo->SetThresholds(new_confidence_threshold, new_nms_threshold);
      }
      RCLCPP_INFO(this->get_logger(), "检测参数已更新: confidence_threshold=%.2f, nms_threshold=%.2f", new_confidence_threshold, new_nms_threshold);
    }

    if (gate_changed) {
      sampling_interval_ms_ = new_sampling_interval_ms;
      lock_duration_ms_ = new_lock_duration_ms;
      const auto now = std::chrono::steady_clock::now();
      {
        std::lock_guard<std::mutex> lock(gate_mutex_);
        detection_gate_.update_config(
          std::chrono::milliseconds{sampling_interval_ms_},
          std::chrono::milliseconds{lock_duration_ms_},
          now);
      }
      RCLCPP_INFO(this->get_logger(), "检测门控参数已更新: sampling_interval_ms=%d, lock_duration_ms=%d",
        sampling_interval_ms_, lock_duration_ms_);
    }

    return result;
  }

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

  /**
   * @brief 推理工作线程主循环
   * 从任务队列中获取图像引用，通过共享内存读取数据并调用 YOLO 执行检测
   */
  void workerLoop(int worker_idx)
  {
    auto &worker = workers_.at(static_cast<size_t>(worker_idx));
    while (rclcpp::ok()) {
      FrameTask task;
      {
        // 等待队列有新任务或停止信号
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this]() { return stop_workers_ || !queue_.empty(); });
        if (stop_workers_ && queue_.empty()) {
          return;
        }
        task = std::move(queue_.front());
        queue_.pop_front();
      }

      const auto t0 = std::chrono::steady_clock::now();
      // 获取共享内存插槽访问权
      if (!detector_ring_->acquire(task.ref.slot_index, task.ref.sequence)) {
        dropped_frames_.fetch_add(1);
        continue;
      }

      bishe_msgs::shared_memory::SharedFrameView view;
      if (!detector_ring_->viewSlot(task.ref.slot_index, task.ref.sequence, view)) {
        detector_ring_->release(task.ref.slot_index);
        dropped_frames_.fetch_add(1);
        continue;
      }

      // 将共享内存指针包装为 OpenCV Mat (零拷贝)
      cv::Mat frame(
        static_cast<int>(view.height),
        static_cast<int>(view.width),
        CV_8UC3,
        const_cast<uint8_t *>(view.data),
        view.step);
      // 执行 AI 推理
      DetectionResult detection_result = worker->yolo->Detect(frame);
      const auto t1 = std::chrono::steady_clock::now();

      // 如果发现违规，更新检测门控状态
      if (!detection_result.detections.empty()) {
        std::lock_guard<std::mutex> gate_lock(gate_mutex_);
        detection_gate_.on_detection(t1);
      }

      const double inf_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;
      {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        inference_time_ms_acc_ += inf_ms;
      }
      processed_frames_.fetch_add(1);

      // 构建并发布检测结果消息
      auto result = bishe_msgs::msg::DetectorResult();
      result.has_violation = !detection_result.detections.empty();
      result.confidence = detection_result.detections.empty() ? 0.0f : detection_result.detections.front().confidence;
      result.nms_threshold = this->nms_threshold_.load();
      result.violation_type = detection_result.detections.empty() ? "" : detection_result.detections.front().class_name;
      result.annotated_image = *cv_bridge::CvImage(task.ref.header, "bgr8", detection_result.annotated_image).toImageMsg();

      result_pub_->publish(result);
      // 释放共享内存插槽
      detector_ring_->release(task.ref.slot_index);
      output_frames_.fetch_add(1);
      // reportStats();
    }
  }

  /**
   * @brief 图像主题回调函数
   * 负责接收相机发出的图像引用，并根据采样门控逻辑决定是否放入推理队列
   */
  void imageCallback(const bishe_msgs::msg::SharedFrameRef::SharedPtr ref)
  {
    try
    {
      input_frames_.fetch_add(1);
      const auto now = std::chrono::steady_clock::now();
      bool should_process = false;
      {
        // 门控逻辑：判断当前帧是否需要进行推理
        std::lock_guard<std::mutex> lock(gate_mutex_);
        should_process = detection_gate_.should_process(now);
      }

      // 如果不需要推理，则执行“透传”逻辑，直接发布无违规的结果
      if (!should_process) {
        if (!detector_ring_->acquire(ref->slot_index, ref->sequence)) {
          dropped_frames_.fetch_add(1);
          return;
        }
        auto image = buildImageFromSharedFrame(*ref);
        detector_ring_->release(ref->slot_index);
        if (!image) {
          dropped_frames_.fetch_add(1);
          return;
        }
        result_pub_->publish(buildOwnedPassThroughResult(std::move(image), nms_threshold_.load()));
        output_frames_.fetch_add(1);
        // reportStats();
        return;
      }

      // 需要推理，将任务放入队列
      FrameTask task;
      task.ref = *ref;
      task.enqueue_time = now;

      {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (queue_.size() >= static_cast<size_t>(max_queue_size_)) {
          queue_.pop_front(); // 丢弃最老的任务
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

std::shared_ptr<rclcpp::Node> make_detector_node(const rclcpp::NodeOptions &options)
{
  return std::make_shared<DetectorNode>(options);
}

}  // namespace bishe_detector

RCLCPP_COMPONENTS_REGISTER_NODE(bishe_detector::DetectorNode)
