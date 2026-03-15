#include <rclcpp/rclcpp.hpp>
#include <bishe_msgs/msg/detector_result.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <curl/curl.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <sstream>

class MonitorNode : public rclcpp::Node
{
public:
  MonitorNode()
      : Node("monitor_node")
  {
    // Declare and load parameters
    this->declare_parameter<int>("window_seconds", 5);
    this->declare_parameter<float>("violation_ratio_threshold", 0.4);
    this->declare_parameter<std::string>("location", "生产车间A区");
    this->declare_parameter<std::string>("camera_id", "001");
    this->declare_parameter<std::string>("upload.server_url", "http://localhost:5000/capture/upload");
    this->declare_parameter<std::string>("alarm.audio_file", "/path/to/alarm.mp3");

    this->get_parameter("window_seconds", window_seconds_);
    this->get_parameter("violation_ratio_threshold", violation_ratio_threshold_);
    this->get_parameter("location", location_);
    this->get_parameter("camera_id", camera_id_);
    this->get_parameter("upload.server_url", server_url_);
    this->get_parameter("alarm.audio_file", alarm_audio_file_);

    // Subscribe to detection results
    result_sub_ = this->create_subscription<bishe_msgs::msg::DetectorResult>(
        "detector/result", 10,
        std::bind(&MonitorNode::resultCallback, this, std::placeholders::_1));

    RCLCPP_INFO(this->get_logger(), "Monitor node started");
    RCLCPP_INFO(this->get_logger(), "Window: %ds, Threshold: %.2f", window_seconds_, violation_ratio_threshold_);
    RCLCPP_INFO(this->get_logger(), "Location: %s, Camera: %s", location_.c_str(), camera_id_.c_str());
  }

private:
  void resultCallback(const bishe_msgs::msg::DetectorResult::SharedPtr msg)
  {
    auto now = std::chrono::steady_clock::now();

    // Check for violation
    if (msg->has_violation) {
      // First violation detected - start alarm
      if (!is_alarming_) {
        startAlarm();
        window_start_time_ = now;
        violation_count_ = 0;
        total_count_ = 0;
        RCLCPP_INFO(this->get_logger(), "First violation detected, starting alarm and window");
      }

      // Store the current frame for capture (will capture on trigger)
      latest_violation_frame_ = cv_bridge::toCvCopy(msg->annotated_image, "bgr8")->image;
      latest_violation_type_ = msg->violation_type;
      violation_count_++;
    }

    // Count frames in window
    total_count_++;

    // Check if window expired
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - window_start_time_).count();

    if (is_alarming_ && elapsed >= window_seconds_) {
      // Calculate ratio
      float ratio = (total_count_ > 0) ? (float)violation_count_ / total_count_ : 0.0f;

      RCLCPP_INFO(this->get_logger(), "Window ended. Violations: %d/%d = %.2f",
                  violation_count_, total_count_, ratio);

      if (ratio >= violation_ratio_threshold_) {
        // Trigger capture and upload
        RCLCPP_WARN(this->get_logger(), "Violation ratio %.2f >= %.2f, triggering capture!",
                     ratio, violation_ratio_threshold_);
        captureAndUpload();
      } else {
        RCLCPP_INFO(this->get_logger(), "Violation ratio below threshold, stopping alarm");
      }

      // Reset window
      stopAlarm();
      violation_count_ = 0;
      total_count_ = 0;
    }
  }

  void startAlarm()
  {
    is_alarming_ = true;
    RCLCPP_WARN(this->get_logger(), "Starting alarm: playing %s", alarm_audio_file_.c_str());

    // Play alarm in loop using GStreamer
    std::string cmd = "gst-launch-1.0 filesrc location=" + alarm_audio_file_ +
                      " ! decodebin ! audioconvert ! auto-sink &";
    alarm_pid_ = system(cmd.c_str());
  }

  void stopAlarm()
  {
    if (is_alarming_) {
      is_alarming_ = false;
      RCLCPP_INFO(this->get_logger(), "Stopping alarm");

      // Kill the alarm process
      if (alarm_pid_ > 0) {
        std::string kill_cmd = "kill " + std::to_string(alarm_pid_);
        system(kill_cmd.c_str());
      }
    }
  }

  void captureAndUpload()
  {
    if (latest_violation_frame_.empty()) {
      RCLCPP_ERROR(this->get_logger(), "No violation frame available for capture");
      return;
    }

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&time_t));

    // Save to temporary file
    std::string temp_file = "/tmp/capture_" + std::to_string(std::time(nullptr)) + ".jpg";
    cv::imwrite(temp_file, latest_violation_frame_);

    // Upload via HTTP
    uploadCapture(temp_file, time_str);

    // Clean up temp file
    std::filesystem::remove(temp_file);
  }

  void uploadCapture(const std::string& file_path, const std::string& time_str)
  {
    CURL *curl = curl_easy_init();

    if (curl) {
      curl_mime *form = curl_mime_init(curl);
      curl_mimepart *field = curl_mime_addpart(form);

      curl_mime_name(field, "file");
      curl_mime_filedata(field, file_path.c_str());

      field = curl_mime_addpart(form);
      curl_mime_name(field, "camera_id");
      curl_mime_data(field, camera_id_.c_str(), CURL_ZERO_TERMINATED);

      field = curl_mime_addpart(form);
      curl_mime_name(field, "location");
      curl_mime_data(field, location_.c_str(), CURL_ZERO_TERMINATED);

      field = curl_mime_addpart(form);
      curl_mime_name(field, "violation_type");
      curl_mime_data(field, latest_violation_type_.c_str(), CURL_ZERO_TERMINATED);

      curl_easy_setopt(curl, CURLOPT_URL, server_url_.c_str());
      curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

      CURLcode res = curl_easy_perform(curl);

      if (res != CURLE_OK) {
        RCLCPP_ERROR(this->get_logger(), "Upload failed: %s", curl_easy_strerror(res));
      } else {
        RCLCPP_INFO(this->get_logger(), "Capture uploaded successfully!");
      }

      curl_mime_free(form);
      curl_easy_cleanup(curl);
    }
  }

  rclcpp::Subscription<bishe_msgs::msg::DetectorResult>::SharedPtr result_sub_;

  int window_seconds_;
  float violation_ratio_threshold_;
  std::string location_;
  std::string camera_id_;
  std::string server_url_;
  std::string alarm_audio_file_;

  bool is_alarming_ = false;
  pid_t alarm_pid_ = 0;

  std::chrono::steady_clock::time_point window_start_time_;
  int violation_count_ = 0;
  int total_count_ = 0;

  cv::Mat latest_violation_frame_;
  std::string latest_violation_type_;
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MonitorNode>());
  rclcpp::shutdown();
  return 0;
}
