#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <chrono>
#include <memory>

#include <bishe_msgs/msg/shared_frame_ref.hpp>
#include <rclcpp_components/register_node_macro.hpp>

#include "bishe_camera/camera_node_factory.hpp"
#include "bishe_msgs/shared_frame_ring.hpp"

namespace bishe_camera
{

class CameraNode : public rclcpp::Node
{
public:
  explicit CameraNode(const rclcpp::NodeOptions &options)
      : Node("camera_node", options)
  {
    this->declare_parameter<std::string>("video_device", "/dev/video0");
    this->declare_parameter<int>("width", 1280);
    this->declare_parameter<int>("height", 720);
    this->declare_parameter<int>("framerate", 60);
    this->declare_parameter<double>("detector_scale", 0.5);
    this->declare_parameter<int>("detector_width", 640);
    this->declare_parameter<int>("detector_height", 360);
    this->declare_parameter<std::string>("shared_memory_name", "/camera_001_detector_shm");
    this->declare_parameter<std::string>("shared_metadata_topic", "camera/detector_frame_ref");

    this->get_parameter("video_device", device_);
    this->get_parameter("width", width_);
    this->get_parameter("height", height_);
    this->get_parameter("framerate", framerate_);
    this->get_parameter("detector_scale", detector_scale_);
    this->get_parameter("detector_width", detector_width_);
    this->get_parameter("detector_height", detector_height_);
    this->get_parameter("shared_memory_name", shared_memory_name_);
    this->get_parameter("shared_metadata_topic", shared_metadata_topic_);

    detector_width_ = std::max(1, detector_width_);
    detector_height_ = std::max(1, detector_height_);

    image_pub_ = this->create_publisher<sensor_msgs::msg::Image>(
      "camera/image_raw", rclcpp::SensorDataQoS());
    detector_ref_pub_ = this->create_publisher<bishe_msgs::msg::SharedFrameRef>(
      shared_metadata_topic_, rclcpp::QoS(16).reliable());

    bishe_msgs::shared_memory::SharedFrameRingConfig ring_config;
    ring_config.shm_name = shared_memory_name_;
    ring_config.slot_count = 8;
    ring_config.width = static_cast<uint32_t>(detector_width_);
    ring_config.height = static_cast<uint32_t>(detector_height_);
    ring_config.channels = 3;
    detector_ring_ = std::make_unique<bishe_msgs::shared_memory::SharedFrameRing>(ring_config, true);

    initCamera();

    int interval_ms = 1000 / framerate_;
    timer_ = this->create_wall_timer(
        std::chrono::milliseconds(interval_ms),
        std::bind(&CameraNode::publishFrame, this));

    RCLCPP_INFO(this->get_logger(), "摄像头打开成功，读取图片发布到 /camera/image_raw at %dx%d@%dfps", width_, height_, framerate_);
  }

private:
  uint64_t published_frames_{0};
  std::chrono::steady_clock::time_point fps_window_start_{std::chrono::steady_clock::now()};

  void reportFps()
  {
    ++published_frames_;
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - fps_window_start_).count();
    if (elapsed_ms >= 1000)
    {
      const double fps = static_cast<double>(published_frames_) * 1000.0 / static_cast<double>(elapsed_ms);
      RCLCPP_INFO(this->get_logger(), "camera 实际发布FPS: %.2f", fps);
      published_frames_ = 0;
      fps_window_start_ = now;
    }
  }

  void initCamera()
  {
    // Robust GStreamer pipeline for Jetson MJPEG capture
    // Added jpegparse and simplified conversion
    std::string pipeline =
        "v4l2src device=" + device_ + " do-timestamp=true ! "
        "image/jpeg, width=" + std::to_string(width_) + ", height=" + std::to_string(height_) + ", framerate=" + std::to_string(framerate_) + "/1 ! "
        "jpegparse ! nvv4l2decoder mjpegdecode=1 ! "
        "nvvidconv ! video/x-raw, format=BGRx ! "
        "videoconvert ! video/x-raw, format=BGR ! appsink drop=true";

    // RCLCPP_INFO(this->get_logger(), "Opening pipeline: %s", pipeline.c_str());
    cap_.open(pipeline, cv::CAP_GSTREAMER);
    if (!cap_.isOpened()) {
      RCLCPP_ERROR(this->get_logger(), "无法打开摄像头 %s  GStreamer", device_.c_str());
      throw std::runtime_error("摄像头打开失败");
    }
    RCLCPP_INFO(this->get_logger(), "摄像头打开成功");
  }

  void publishFrame()
  {
    cv::Mat frame;
    if (cap_.read(frame)) {
      if (!frame.empty()) {
        auto now = this->now();
        std_msgs::msg::Header header;
        header.stamp = now;
        header.frame_id = "camera_frame";

        auto full_msg = cv_bridge::CvImage(header, "bgr8", frame).toImageMsg();
        image_pub_->publish(*full_msg);

        cv::Mat detector_frame;
        if (detector_width_ != frame.cols || detector_height_ != frame.rows) {
          cv::resize(frame, detector_frame, cv::Size(detector_width_, detector_height_), 0.0, 0.0, cv::INTER_LINEAR);
        } else {
          detector_frame = frame;
        }

        const uint32_t bytes_used = static_cast<uint32_t>(detector_frame.step * detector_frame.rows);
        uint32_t slot_index = 0;
        const uint64_t sequence = ++detector_sequence_;
        if (detector_ring_->writeNext(
              sequence,
              detector_frame.data,
              bytes_used,
              static_cast<uint32_t>(detector_frame.cols),
              static_cast<uint32_t>(detector_frame.rows),
              static_cast<uint32_t>(detector_frame.step),
              "bgr8",
              slot_index)) {
          bishe_msgs::msg::SharedFrameRef ref_msg;
          ref_msg.header = header;
          ref_msg.sequence = sequence;
          ref_msg.slot_index = slot_index;
          ref_msg.width = detector_frame.cols;
          ref_msg.height = detector_frame.rows;
          ref_msg.step = detector_frame.step;
          ref_msg.bytes_used = bytes_used;
          ref_msg.encoding = "bgr8";
          detector_ref_pub_->publish(ref_msg);
        } else {
          // RCLCPP_WARN_THROTTLE(
          //   this->get_logger(), *this->get_clock(), 2000,
          //   "共享内存检测帧写入失败，丢弃当前 detector 帧");
        }
        // reportFps();
      }
    }
  }

  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
  rclcpp::Publisher<bishe_msgs::msg::SharedFrameRef>::SharedPtr detector_ref_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  cv::VideoCapture cap_;
  std::string device_;
  int width_;
  int height_;
  int framerate_;
  double detector_scale_;
  int detector_width_;
  int detector_height_;
  std::string shared_memory_name_;
  std::string shared_metadata_topic_;
  uint64_t detector_sequence_{0};
  std::unique_ptr<bishe_msgs::shared_memory::SharedFrameRing> detector_ring_;
};

std::shared_ptr<rclcpp::Node> make_camera_node(const rclcpp::NodeOptions &options)
{
  return std::make_shared<CameraNode>(options);
}

}  // namespace bishe_camera

RCLCPP_COMPONENTS_REGISTER_NODE(bishe_camera::CameraNode)
