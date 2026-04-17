#include <rclcpp/rclcpp.hpp>
#include <bishe_msgs/msg/detector_result.hpp>
#include <std_msgs/msg/string.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <curl/curl.h>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <sstream>
#include <deque>
#include <unordered_map>

#include "bishe_monitor/alarm_upload_gate.hpp"
#include "bishe_monitor/alarm_playback_command.hpp"
#include "bishe_monitor/alarm_event_payload.hpp"
#include "bishe_monitor/async_task_worker.hpp"

namespace
{
size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  const size_t total_size = size * nmemb;
  auto *buffer = static_cast<std::string *>(userp);
  buffer->append(static_cast<char *>(contents), total_size);
  return total_size;
}
}

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
    this->declare_parameter<int>("trigger_frame_threshold", 3);
    this->declare_parameter<int>("trigger_cooldown_seconds", 15);
    this->declare_parameter<int>("upload_after_alarm_count", 2);
    this->declare_parameter<bool>("reset_alarm_count_after_upload", true);
    this->declare_parameter<int>("alarm_count_reset_timeout_seconds", 30);
    this->declare_parameter<std::string>("upload.server_url", "http://localhost:5000/capture/upload");
    this->declare_parameter<int>("upload.timeout_seconds", 10);
    this->declare_parameter<std::string>("alarm.audio_file", "/path/to/alarm.mp3");
    this->declare_parameter<std::string>("alarm.fire_audio_file", "");
    this->declare_parameter<std::string>("alarm.smoking_audio_file", "");

    this->get_parameter("window_seconds", window_seconds_);
    this->get_parameter("violation_ratio_threshold", violation_ratio_threshold_);
    this->get_parameter("location", location_);
    this->get_parameter("camera_id", camera_id_);
    this->get_parameter("trigger_frame_threshold", trigger_frame_threshold_);
    this->get_parameter("trigger_cooldown_seconds", trigger_cooldown_seconds_);
    this->get_parameter("upload_after_alarm_count", upload_after_alarm_count_);
    this->get_parameter("reset_alarm_count_after_upload", reset_alarm_count_after_upload_);
    this->get_parameter("alarm_count_reset_timeout_seconds", alarm_count_reset_timeout_seconds_);
    this->get_parameter("upload.server_url", server_url_);
    this->get_parameter("upload.timeout_seconds", upload_timeout_seconds_);
    this->get_parameter("alarm.audio_file", alarm_audio_file_);
    this->get_parameter("alarm.fire_audio_file", fire_alarm_audio_file_);
    this->get_parameter("alarm.smoking_audio_file", smoking_alarm_audio_file_);

    if (fire_alarm_audio_file_.empty()) {
      fire_alarm_audio_file_ = alarm_audio_file_;
    }
    if (smoking_alarm_audio_file_.empty()) {
      smoking_alarm_audio_file_ = alarm_audio_file_;
    }

    if (trigger_frame_threshold_ < 1) {
      trigger_frame_threshold_ = 1;
    }
    if (window_seconds_ < 1) {
      window_seconds_ = 1;
    }
    if (trigger_cooldown_seconds_ < 0) {
      trigger_cooldown_seconds_ = 0;
    }
    if (upload_after_alarm_count_ < 1) {
      upload_after_alarm_count_ = 1;
    }
    if (alarm_count_reset_timeout_seconds_ < 0) {
      alarm_count_reset_timeout_seconds_ = 0;
    }
    if (upload_timeout_seconds_ < 1) {
      upload_timeout_seconds_ = 10;
    }

    event_states_.emplace("fire", makeEventState());
    event_states_.emplace("smoking", makeEventState());

    result_sub_ = this->create_subscription<bishe_msgs::msg::DetectorResult>(
        "detector/result", 10,
        std::bind(&MonitorNode::resultCallback, this, std::placeholders::_1));

    alarm_event_pub_ = this->create_publisher<std_msgs::msg::String>("/alarm/event", 10);

    param_callback_handle_ = this->add_on_set_parameters_callback(
        std::bind(&MonitorNode::onParameterChange, this, std::placeholders::_1));

    RCLCPP_INFO(this->get_logger(), "Monitor node started");
    RCLCPP_INFO(this->get_logger(), "Window: %ds, TriggerFrames: %d, Cooldown: %ds", window_seconds_, trigger_frame_threshold_, trigger_cooldown_seconds_);
    RCLCPP_INFO(this->get_logger(), "Alarm upload gate: count=%d, reset_after_upload=%s, quiet_timeout=%ds",
                upload_after_alarm_count_, reset_alarm_count_after_upload_ ? "true" : "false", alarm_count_reset_timeout_seconds_);
    RCLCPP_INFO(this->get_logger(), "Location: %s, Camera: %s", location_.c_str(), camera_id_.c_str());
  }

  ~MonitorNode() override
  {
    RCLCPP_INFO(this->get_logger(), "Monitor node shutting down, waiting async tasks...");
  }

private:
  struct EventState {
    std::deque<std::chrono::steady_clock::time_point> hit_times;
    std::chrono::steady_clock::time_point last_trigger_time = std::chrono::steady_clock::time_point::min();
    AlarmUploadGate alarm_upload_gate{2, true, std::chrono::seconds(30)};
  };

  EventState makeEventState() const
  {
    EventState state;
    state.alarm_upload_gate.updateConfig(
      upload_after_alarm_count_,
      reset_alarm_count_after_upload_,
      std::chrono::seconds(alarm_count_reset_timeout_seconds_));
    return state;
  }

  void refreshAlarmUploadGateConfig()
  {
    for (auto &entry : event_states_) {
      entry.second.alarm_upload_gate.updateConfig(
        upload_after_alarm_count_,
        reset_alarm_count_after_upload_,
        std::chrono::seconds(alarm_count_reset_timeout_seconds_));
    }
  }

  rcl_interfaces::msg::SetParametersResult onParameterChange(
      const std::vector<rclcpp::Parameter> &parameters)
  {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;
    for (const auto &param : parameters) {
      if (param.get_name() == "window_seconds") {
        const int new_val = static_cast<int>(param.as_int());
        if (new_val < 1) {
          result.successful = false;
          result.reason = "window_seconds must be >= 1";
          return result;
        }
        window_seconds_ = new_val;
        RCLCPP_INFO(this->get_logger(), "window_seconds 动态更新: %d", window_seconds_);
      } else if (param.get_name() == "upload_after_alarm_count") {
        const int new_val = static_cast<int>(param.as_int());
        if (new_val < 1) {
          result.successful = false;
          result.reason = "upload_after_alarm_count must be >= 1";
          return result;
        }
        upload_after_alarm_count_ = new_val;
      } else if (param.get_name() == "reset_alarm_count_after_upload") {
        reset_alarm_count_after_upload_ = param.as_bool();
      } else if (param.get_name() == "alarm_count_reset_timeout_seconds") {
        const int new_val = static_cast<int>(param.as_int());
        if (new_val < 0) {
          result.successful = false;
          result.reason = "alarm_count_reset_timeout_seconds must be >= 0";
          return result;
        }
        alarm_count_reset_timeout_seconds_ = new_val;
      }
    }

    refreshAlarmUploadGateConfig();
    return result;
  }

  std::string normalizeViolationType(const std::string &input) const
  {
    if (input == "fire" || input == "火焰" || input == "明火") {
      return "fire";
    }
    if (input == "smoking" || input == "吸烟") {
      return "smoking";
    }
    return "";
  }

  std::string toUploadViolationType(const std::string &normalized_type) const
  {
    if (normalized_type == "smoking") {
      return "吸烟";
    }
    if (normalized_type == "fire") {
      return "明火";
    }
    return normalized_type;
  }

  void pruneExpiredHits(EventState &state, const std::chrono::steady_clock::time_point &now)
  {
    const auto window = std::chrono::seconds(window_seconds_);
    while (!state.hit_times.empty() && (now - state.hit_times.front()) > window) {
      state.hit_times.pop_front();
    }
  }

  void resultCallback(const bishe_msgs::msg::DetectorResult::SharedPtr msg)
  {
    if (!msg->has_violation) {
      return;
    }

    const std::string normalized_type = normalizeViolationType(msg->violation_type);
    if (normalized_type.empty()) {
      RCLCPP_DEBUG(this->get_logger(), "忽略未订阅的违规类型: %s", msg->violation_type.c_str());
      return;
    }

    const auto state_it = event_states_.find(normalized_type);
    if (state_it == event_states_.end()) {
      return;
    }

    auto &state = state_it->second;
    const auto now = std::chrono::steady_clock::now();
    state.hit_times.push_back(now);
    pruneExpiredHits(state, now);

    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
                         "检测到行为 [%s], 窗口内命中次数: %zu/%d", 
                         normalized_type.c_str(), state.hit_times.size(), trigger_frame_threshold_);

    if (static_cast<int>(state.hit_times.size()) < trigger_frame_threshold_) {
      return;
    }

    const auto cooldown = std::chrono::seconds(trigger_cooldown_seconds_);
    if (state.last_trigger_time != std::chrono::steady_clock::time_point::min() &&
        (now - state.last_trigger_time) < cooldown) {
      return;
    }

    state.last_trigger_time = now;
    state.hit_times.clear();

    const auto gate_result = state.alarm_upload_gate.onAlarmTriggered(now);

    RCLCPP_WARN(this->get_logger(), "触发告警: type=%s, window=%ds, hits=%d, alarm_count=%d/%d",
                normalized_type.c_str(), window_seconds_, trigger_frame_threshold_,
                gate_result.new_count, upload_after_alarm_count_);
    enqueueAlarmPlayback(normalized_type);

    if (!gate_result.should_upload) {
      return;
    }

    cv_bridge::CvImageConstPtr cv_ptr;
    try {
      cv_ptr = cv_bridge::toCvShare(msg->annotated_image, msg, "bgr8");
    } catch (const std::exception &e) {
      RCLCPP_ERROR(this->get_logger(), "Convert image failed: %s", e.what());
      return;
    }

    RCLCPP_WARN(this->get_logger(), "达到抓拍上传门槛: type=%s, alarm_count=%d/%d",
                normalized_type.c_str(), gate_result.new_count, upload_after_alarm_count_);
    enqueueCaptureUpload(normalized_type, cv_ptr->image);
    state.alarm_upload_gate.onUploadCompleted();
  }

  void playAlarm(const std::string &violation_type)
  {
    const std::string audio_file = violation_type == "fire" ? fire_alarm_audio_file_ : smoking_alarm_audio_file_;
    if (audio_file.empty()) {
      RCLCPP_WARN(this->get_logger(), "报警音文件未配置，跳过播放: type=%s", violation_type.c_str());
      return;
    }

    const std::string cmd = buildAlarmPlaybackCommand(audio_file);
    const int ret = std::system(cmd.c_str());
    if (ret != 0) {
      RCLCPP_WARN(this->get_logger(), "播放报警音失败: type=%s, file=%s, exit_code=%d",
                  violation_type.c_str(), audio_file.c_str(), ret);
    }
  }

  void enqueueAlarmPlayback(const std::string &violation_type)
  {
    publishAlarmEvent(violation_type);

    const bool accepted = alarm_task_worker_.enqueue(
        [this, violation_type]() {
          try {
            playAlarm(violation_type);
          } catch (const std::exception &e) {
            RCLCPP_ERROR(this->get_logger(), "异步播放报警音失败: %s", e.what());
          } catch (...) {
            RCLCPP_ERROR(this->get_logger(), "异步播放报警音失败: 未知异常");
          }
        });

    if (!accepted) {
      RCLCPP_WARN(this->get_logger(), "异步报警音队列已停止，丢弃任务: type=%s", violation_type.c_str());
    }
  }

  void publishAlarmEvent(const std::string &violation_type)
  {
    if (!alarm_event_pub_) {
      return;
    }

    std_msgs::msg::String msg;
    const auto now = std::chrono::system_clock::now();
    const auto timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    msg.data = buildAlarmEventPayload(camera_id_, location_, violation_type, timestamp_ns);
    alarm_event_pub_->publish(msg);
  }

  void enqueueCaptureUpload(const std::string &violation_type, const cv::Mat &frame)
  {
    if (frame.empty()) {
      RCLCPP_ERROR(this->get_logger(), "无可用抓拍图像: type=%s", violation_type.c_str());
      return;
    }

    cv::Mat frame_copy = frame.clone();
    if (frame_copy.empty()) {
      RCLCPP_ERROR(this->get_logger(), "抓拍图像复制失败: type=%s", violation_type.c_str());
      return;
    }

    const bool accepted = task_worker_.enqueue(
        [this, violation_type, frame = std::move(frame_copy)]() mutable {
          try {
            captureAndUpload(frame, violation_type);
          } catch (const std::exception &e) {
            RCLCPP_ERROR(this->get_logger(), "异步抓拍任务失败: %s", e.what());
          } catch (...) {
            RCLCPP_ERROR(this->get_logger(), "异步抓拍任务失败: 未知异常");
          }
        });

    if (!accepted) {
      RCLCPP_WARN(this->get_logger(), "异步抓拍队列已停止，丢弃任务: type=%s", violation_type.c_str());
    }
  }

  void captureAndUpload(const cv::Mat &frame, const std::string &violation_type)
  {
    auto now = std::chrono::system_clock::now();
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    std::string temp_file = "/tmp/capture_" + camera_id_ + "_" + violation_type + "_" + std::to_string(now_ns) + ".jpg";
    if (!cv::imwrite(temp_file, frame)) {
      RCLCPP_ERROR(this->get_logger(), "抓拍保存失败: %s", temp_file.c_str());
      return;
    }

    uploadCapture(temp_file, violation_type);
    std::error_code ec;
    std::filesystem::remove(temp_file, ec);
    if (ec) {
      RCLCPP_WARN(this->get_logger(), "清理临时抓拍文件失败: %s, err=%s", temp_file.c_str(), ec.message().c_str());
    }
  }

  void uploadCapture(const std::string &file_path, const std::string &violation_type)
  {
    CURL *curl = curl_easy_init();
    if (!curl) {
      RCLCPP_ERROR(this->get_logger(), "初始化 CURL 失败");
      return;
    }

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
    const std::string upload_violation_type = toUploadViolationType(violation_type);
    curl_mime_name(field, "violation_type");
    curl_mime_data(field, upload_violation_type.c_str(), CURL_ZERO_TERMINATED);

    std::string response_body;
    curl_easy_setopt(curl, CURLOPT_URL, server_url_.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, upload_timeout_seconds_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    const CURLcode res = curl_easy_perform(curl);
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (res != CURLE_OK) {
      RCLCPP_ERROR(this->get_logger(), "上传失败: %s", curl_easy_strerror(res));
    } else if (response_code != 200) {
      RCLCPP_ERROR(this->get_logger(), "上传返回非200: code=%ld body=%s", response_code, response_body.c_str());
    } else {
      RCLCPP_INFO(this->get_logger(), "抓拍上传成功: type=%s", violation_type.c_str());
    }

    curl_mime_free(form);
    curl_easy_cleanup(curl);
  }

  rclcpp::Subscription<bishe_msgs::msg::DetectorResult>::SharedPtr result_sub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr alarm_event_pub_;
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_callback_handle_;

  int window_seconds_;
  int trigger_frame_threshold_;
  int trigger_cooldown_seconds_;
  int upload_after_alarm_count_;
  bool reset_alarm_count_after_upload_;
  int alarm_count_reset_timeout_seconds_;
  float violation_ratio_threshold_;
  std::string location_;
  std::string camera_id_;
  std::string server_url_;
  int upload_timeout_seconds_;
  std::string alarm_audio_file_;
  std::string fire_alarm_audio_file_;
  std::string smoking_alarm_audio_file_;
  std::unordered_map<std::string, EventState> event_states_;
  AsyncTaskWorker alarm_task_worker_;
  AsyncTaskWorker task_worker_;
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MonitorNode>());
  rclcpp::shutdown();
  return 0;
}
