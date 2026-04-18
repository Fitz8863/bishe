#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

#include <bishe_msgs/msg/detector_result.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>

#include <curl/curl.h>

#include <chrono>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "bishe_monitor/alarm_event_payload.hpp"
#include "bishe_monitor/alarm_upload_gate.hpp"

namespace
{
size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  const size_t total_size = size * nmemb;
  auto *buffer = static_cast<std::string *>(userp);
  buffer->append(static_cast<char *>(contents), total_size);
  return total_size;
}

std::string escapeForShellDoubleQuotes(const std::string &value)
{
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (char c : value) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') {
      escaped.push_back('\\');
    }
    escaped.push_back(c);
  }
  return escaped;
}
}

class MonitorNode : public rclcpp::Node
{
public:
  MonitorNode()
  : Node("monitor_node")
  {
    declareParameters();
    loadParameters();
    sanitizeParameters();

    event_states_.emplace("fire", EventState{});
    event_states_.emplace("smoking", EventState{});
    updateAlarmGateConfigs();

    result_sub_ = this->create_subscription<bishe_msgs::msg::DetectorResult>(
      "detector/result", 10,
      std::bind(&MonitorNode::resultCallback, this, std::placeholders::_1));
    alarm_event_pub_ = this->create_publisher<std_msgs::msg::String>("/alarm/event", 10);

    param_callback_handle_ = this->add_on_set_parameters_callback(
      std::bind(&MonitorNode::onParameterChange, this, std::placeholders::_1));

    RCLCPP_INFO(this->get_logger(), "Monitor node started");
    RCLCPP_INFO(
      this->get_logger(), "Window: %ds, TriggerFrames: %d, Cooldown: %ds",
      window_seconds_, trigger_frame_threshold_, trigger_cooldown_seconds_);
    RCLCPP_INFO(
      this->get_logger(), "Alarm upload gate: threshold=%d, reset_after_upload=%s, quiet_timeout=%ds",
      upload_after_alarm_count_, reset_alarm_count_after_upload_ ? "true" : "false",
      alarm_count_reset_timeout_seconds_);
    RCLCPP_INFO(this->get_logger(), "Location: %s, Camera: %s", location_.c_str(), camera_id_.c_str());
  }

private:
  struct EventState
  {
    std::deque<std::chrono::steady_clock::time_point> hit_times;
    std::chrono::steady_clock::time_point last_trigger_time = std::chrono::steady_clock::time_point::min();
    cv::Mat latest_frame;
    AlarmUploadGate upload_gate;
  };

  void declareParameters()
  {
    this->declare_parameter<int>("window_seconds", 5);
    this->declare_parameter<float>("violation_ratio_threshold", 0.4);
    this->declare_parameter<std::string>("location", "生产车间A区");
    this->declare_parameter<std::string>("camera_id", "001");
    this->declare_parameter<int>("trigger_frame_threshold", 3);
    this->declare_parameter<int>("trigger_cooldown_seconds", 15);
    this->declare_parameter<int>("upload_after_alarm_count", 3);
    this->declare_parameter<bool>("reset_alarm_count_after_upload", true);
    this->declare_parameter<int>("alarm_count_reset_timeout_seconds", 30);
    this->declare_parameter<std::string>("upload.server_url", "http://localhost:5000/capture/upload");
    this->declare_parameter<int>("upload.timeout_seconds", 10);
    this->declare_parameter<std::string>("alarm.audio_file", "/path/to/alarm.mp3");
    this->declare_parameter<std::string>("alarm.fire_audio_file", "");
    this->declare_parameter<std::string>("alarm.smoking_audio_file", "");
  }

  void loadParameters()
  {
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
  }

  void sanitizeParameters()
  {
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
  }

  void updateAlarmGateConfigs()
  {
    const auto quiet_timeout = std::chrono::seconds(alarm_count_reset_timeout_seconds_);
    for (auto &entry : event_states_) {
      entry.second.upload_gate.updateConfig(
        upload_after_alarm_count_, reset_alarm_count_after_upload_, quiet_timeout);
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
        RCLCPP_INFO(this->get_logger(), "upload_after_alarm_count 动态更新: %d", upload_after_alarm_count_);
      } else if (param.get_name() == "reset_alarm_count_after_upload") {
        reset_alarm_count_after_upload_ = param.as_bool();
        RCLCPP_INFO(
          this->get_logger(), "reset_alarm_count_after_upload 动态更新: %s",
          reset_alarm_count_after_upload_ ? "true" : "false");
      } else if (param.get_name() == "alarm_count_reset_timeout_seconds") {
        const int new_val = static_cast<int>(param.as_int());
        if (new_val < 0) {
          result.successful = false;
          result.reason = "alarm_count_reset_timeout_seconds must be >= 0";
          return result;
        }
        alarm_count_reset_timeout_seconds_ = new_val;
        RCLCPP_INFO(
          this->get_logger(), "alarm_count_reset_timeout_seconds 动态更新: %d",
          alarm_count_reset_timeout_seconds_);
      }
    }

    updateAlarmGateConfigs();
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

  void publishAlarmEvent(const std::string &violation_type)
  {
    if (!alarm_event_pub_) {
      return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      now.time_since_epoch()).count();

    std_msgs::msg::String msg;
    msg.data = buildAlarmEventPayload(camera_id_, location_, violation_type, timestamp_ns);
    alarm_event_pub_->publish(msg);
  }

  /**
   * @brief 检测结果处理回调
   * @param msg 包含 AI 检测结果（是否有违规、违规类型、标注图像）
   * 
   * 核心逻辑：
   * 1. 过滤未订阅的违规类型
   * 2. 将检出时刻存入滑动窗口 (hit_times)
   * 3. 统计窗口内检出帧数，若超过阈值且不在冷却期，则触发正式报警
   */
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

    cv_bridge::CvImagePtr cv_ptr;
    try {
      cv_ptr = cv_bridge::toCvCopy(msg->annotated_image, "bgr8");
    } catch (const std::exception &e) {
      RCLCPP_ERROR(this->get_logger(), "Convert image failed: %s", e.what());
      return;
    }

    auto &state = state_it->second;
    state.latest_frame = cv_ptr->image.clone();

    const auto now = std::chrono::steady_clock::now();
    state.hit_times.push_back(now);
    pruneExpiredHits(state, now);

    RCLCPP_INFO_THROTTLE(
      this->get_logger(), *this->get_clock(), 1000,
      "检测到行为 [%s], 窗口内命中次数: %zu/%d",
      normalized_type.c_str(), state.hit_times.size(), trigger_frame_threshold_);

    if (static_cast<int>(state.hit_times.size()) < trigger_frame_threshold_) {
      return;
    }

    const auto cooldown = std::chrono::seconds(trigger_cooldown_seconds_);
    if (
      state.last_trigger_time != std::chrono::steady_clock::time_point::min() &&
      (now - state.last_trigger_time) < cooldown)
    {
      return;
    }

    state.last_trigger_time = now;
    state.hit_times.clear();

    RCLCPP_WARN(
      this->get_logger(), "触发警报: type=%s, window=%ds, hits=%d",
      normalized_type.c_str(), window_seconds_, trigger_frame_threshold_);
    triggerCaptureAndAlarm(normalized_type, state, now);
  }

  /**
   * @brief 播放报警音并发布报警事件
   * @param violation_type 违规类型
   * 
   * 使用 gst-play-1.0 播放音频，并向本地 /alarm/event 主题发布消息
   */
  void playAlarm(const std::string &violation_type)
  {
    const std::string audio_file = violation_type == "fire" ? fire_alarm_audio_file_ : smoking_alarm_audio_file_;
    if (audio_file.empty()) {
      RCLCPP_WARN(this->get_logger(), "报警音文件未配置，跳过播放: type=%s", violation_type.c_str());
      return;
    }

    const std::string cmd = "gst-play-1.0 -q \"" + escapeForShellDoubleQuotes(audio_file) +
      "\" >/dev/null 2>&1 &";
    const int ret = std::system(cmd.c_str());
    if (ret != 0) {
      RCLCPP_WARN(
        this->get_logger(), "播放报警音失败: type=%s, file=%s",
        violation_type.c_str(), audio_file.c_str());
      return;
    }

    publishAlarmEvent(violation_type);
  }

  void triggerCaptureAndAlarm(
    const std::string &violation_type,
    EventState &state,
    const std::chrono::steady_clock::time_point &trigger_time)
  {
    playAlarm(violation_type);

    const auto gate_result = state.upload_gate.onAlarmTriggered(trigger_time);
    if (!gate_result.should_upload) {
      RCLCPP_INFO(
        this->get_logger(), "本次仅播放警报，不抓拍上传: type=%s, alarm_count=%d/%d",
        violation_type.c_str(), gate_result.new_count, upload_after_alarm_count_);
      return;
    }

    if (state.latest_frame.empty()) {
      RCLCPP_ERROR(this->get_logger(), "无可用抓拍图像: type=%s", violation_type.c_str());
      return;
    }

    if (captureAndUpload(state.latest_frame, violation_type)) {
      state.upload_gate.onUploadCompleted();
    }
  }

  /**
   * @brief 执行图像抓拍并上传至 Web 后端
   * @param frame 待上传的 OpenCV 图像
   * @param violation_type 违规类型
   * 
   * 流程：保存本地临时文件 -> 通过 CURL 执行 HTTP POST 上传 -> 删除临时文件
   */
  bool captureAndUpload(const cv::Mat &frame, const std::string &violation_type)
  {
    auto now = std::chrono::system_clock::now();
    const auto now_secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const std::string temp_file =
      "/tmp/capture_" + camera_id_ + "_" + violation_type + "_" + std::to_string(now_secs) + ".jpg";
    if (!cv::imwrite(temp_file, frame)) {
      RCLCPP_ERROR(this->get_logger(), "抓拍保存失败: %s", temp_file.c_str());
      return false;
    }

    const bool upload_ok = uploadCapture(temp_file, violation_type);
    std::filesystem::remove(temp_file);
    return upload_ok;
  }

  bool uploadCapture(const std::string &file_path, const std::string &violation_type)
  {
    CURL *curl = curl_easy_init();
    if (!curl) {
      RCLCPP_ERROR(this->get_logger(), "初始化 CURL 失败");
      return false;
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
      RCLCPP_ERROR(
        this->get_logger(), "上传返回非200: code=%ld body=%s",
        response_code, response_body.c_str());
    } else {
      RCLCPP_INFO(this->get_logger(), "抓拍上传成功: type=%s", violation_type.c_str());
    }

    const bool success = res == CURLE_OK && response_code == 200;
    curl_mime_free(form);
    curl_easy_cleanup(curl);
    return success;
  }

  rclcpp::Subscription<bishe_msgs::msg::DetectorResult>::SharedPtr result_sub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr alarm_event_pub_;
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_callback_handle_;

  int window_seconds_{};
  int trigger_frame_threshold_{};
  int trigger_cooldown_seconds_{};
  int upload_after_alarm_count_{};
  bool reset_alarm_count_after_upload_{};
  int alarm_count_reset_timeout_seconds_{};
  float violation_ratio_threshold_{};
  std::string location_;
  std::string camera_id_;
  std::string server_url_;
  int upload_timeout_seconds_{};
  std::string alarm_audio_file_;
  std::string fire_alarm_audio_file_;
  std::string smoking_alarm_audio_file_;
  std::unordered_map<std::string, EventState> event_states_;
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MonitorNode>());
  rclcpp::shutdown();
  return 0;
}