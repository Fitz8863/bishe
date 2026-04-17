#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/image_transport.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <chrono>

class CameraNode : public rclcpp::Node
{
public:
  CameraNode()
      : Node("camera_node")
  {
    this->declare_parameter<std::string>("video_device", "/dev/video0");
    this->declare_parameter<int>("width", 1280);
    this->declare_parameter<int>("height", 720);
    this->declare_parameter<int>("framerate", 60);

    this->get_parameter("video_device", device_);
    this->get_parameter("width", width_);
    this->get_parameter("height", height_);
    this->get_parameter("framerate", framerate_);

    image_pub_ = image_transport::create_publisher(this, "camera/image_raw", rmw_qos_profile_sensor_data);

    initCamera();

    const int safe_framerate = std::max(framerate_, 1);
    const int interval_ms = std::max(1, 1000 / safe_framerate);
    timer_ = this->create_wall_timer(
        std::chrono::milliseconds(interval_ms),
        std::bind(&CameraNode::publishFrame, this));

    RCLCPP_INFO(this->get_logger(), "摄像头打开成功，读取图片发布到 /camera/image_raw at %dx%d@%dfps", width_, height_, framerate_);
  }

  ~CameraNode() override
  {
    RCLCPP_INFO(this->get_logger(), "正在关闭摄像头节点...");
    if (timer_) {
      timer_->cancel();
    }
    if (cap_.isOpened()) {
      cap_.release();
    }
    RCLCPP_INFO(this->get_logger(), "摄像头节点已安全退出");
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
      // RCLCPP_INFO(this->get_logger(), "camera输出FPS: %.2f", fps);
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
    if (!rclcpp::ok() || !cap_.isOpened()) {
      return;
    }

    cv::Mat frame;
    if (cap_.read(frame)) {
      if (!frame.empty()) {
        auto msg = cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", frame).toImageMsg();
        msg->header.stamp = this->now();
        msg->header.frame_id = "camera_frame";
        image_pub_.publish(msg);
        // reportFps();
      }
    }
  }

  image_transport::Publisher image_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  cv::VideoCapture cap_;
  std::string device_;
  int width_;
  int height_;
  int framerate_;
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<CameraNode>());
  rclcpp::shutdown();
  return 0;
}
