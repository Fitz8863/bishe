/**
 * @file mqtt_node.cpp
 * @brief MQTT 通信节点 - 实现与 MQTT broker 的连接及实时状态上报
 *
 * 本节点是整个边缘检测系统的 MQTT 通信中枢，负责：
 * 1. 连接到指定的 MQTT broker
 * 2. 订阅来自上位机的控制命令主题
 * 3. 定时上报所有摄像头的实时状态信息（包括分辨率、帧率、检测阈值等）
 * 4. 通过 ROS2 参数服务动态获取各节点的运行时参数
 *
 * 主要特性：
 * - 支持多摄像头同时管理
 * - 动态参数获取：实时从 camera_node、detector_node、streamer_node 获取参数
 * - 静态配置：位置信息、HTTP URL 等从启动配置中读取
 * - 线程安全：使用互斥锁保护共享数据结构
 * - JSON 格式上报：结构化的状态信息便于上位机解析
 */

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/parameter_client.hpp>
#include <rclcpp/executors/multi_threaded_executor.hpp>
#include <rcl_interfaces/msg/parameter_event.hpp>
#include <std_msgs/msg/string.hpp>

#include <jsoncpp/json/json.h>
#include <mqtt/async_client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief MQTT 消息回调处理类
 *
 * 当收到来自 MQTT broker 的消息时，此类负责处理。
 * 目前主要用于日志记录收到的消息内容。
 */
class MqttCallback : public virtual mqtt::callback
{
public:
  /**
   * @brief 构造函数
   * @param logger ROS2 日志器实例
   */
  explicit MqttCallback(
      rclcpp::Logger logger,
      std::function<void(const std::string &, const std::string &)> on_message)
      : logger_(std::move(logger)), on_message_(std::move(on_message))
  {
  }

  /**
   * @brief 消息到达回调函数
   * @param msg 收到的 MQTT 消息指针
   *
   * 当有消息到达时触发，打印主题和 payload 内容
   */
  void message_arrived(mqtt::const_message_ptr msg) override
  {
    const std::string topic = msg->get_topic();
    const std::string payload = msg->get_payload_str();
    RCLCPP_INFO(logger_, "Received on %s: %s", topic.c_str(), payload.c_str());
    if (on_message_)
    {
      on_message_(topic, payload);
    }
  }

private:
  rclcpp::Logger logger_; ///< ROS2 日志器
  std::function<void(const std::string &, const std::string &)> on_message_;
};

/**
 * @brief MQTT 主节点类
 *
 * 继承自 rclcpp::Node，提供完整的 MQTT 通信功能。
 * 管理 MQTT 连接、参数同步、状态上报等核心逻辑。
 */
class MqttNode : public rclcpp::Node
{
public:
  /**
   * @brief 构造函数
   *
   * 初始化步骤：
   * 1. 声明并加载 ROS2 参数
   * 2. 构建 MQTT broker 地址并创建客户端
   * 3. 设置消息回调并建立连接
   * 4. 订阅指定主题
   * 5. 创建定时上报计时器
   */
  MqttNode()
      : Node("mqtt_node"), cb_(
                               this->get_logger(),
                               [this](const std::string &topic, const std::string &payload)
                               {
                                 handleMqttMessage(topic, payload);
                               })
  {
    // 步骤1: 声明所有可配置参数
    declareParameters();
    // 步骤2: 从参数服务器加载参数值
    loadParameters();

    // 参数校验：确保上报间隔合法
    if (report_interval_sec_ <= 0.0)
    {
      report_interval_sec_ = 1.0;
    }

    // 步骤3: 构建 MQTT broker 地址并创建客户端
    const std::string server_address = buildServerAddress();
    client_ = std::make_unique<mqtt::async_client>(server_address, client_id_);
    client_->set_callback(cb_);

    // 步骤4: 尝试建立 MQTT 连接
    try
    {
      mqtt::connect_options conn_opts;
      conn_opts.set_keep_alive_interval(20); // 20秒保活间隔
      conn_opts.set_clean_session(true);     // 清除旧会话
      client_->connect(conn_opts)->wait();
      RCLCPP_INFO(this->get_logger(), "Connected to MQTT broker: %s", server_address.c_str());

      // 步骤5: 订阅控制命令主题
      client_->subscribe(subscribe_topic_, 1)->wait();
      RCLCPP_INFO(this->get_logger(), "Subscribed to: %s", subscribe_topic_.c_str());
      client_->subscribe(call_topic_, 1)->wait();
      RCLCPP_INFO(this->get_logger(), "Subscribed to: %s", call_topic_.c_str());
    }
    catch (const mqtt::exception &e)
    {
      RCLCPP_ERROR(this->get_logger(), "MQTT connection failed: %s", e.what());
    }

    // 步骤6: 创建定时器用于周期性上报状态
    createReportTimer();

    // 初始化通话控制发布器
    intercom_pub_ = this->create_publisher<std_msgs::msg::String>("intercom/control", 10);
    // 初始化报警事件订阅器 - 监听 monitor_node 发出的报警并转发到 MQTT
    alarm_event_sub_ = this->create_subscription<std_msgs::msg::String>(
        "/alarm/event", 10,
        std::bind(&MqttNode::onAlarmEvent, this, std::placeholders::_1));

    // 步骤7: 订阅全局参数事件，用于被动接收参数变更（优化 IPC 性能）
    rclcpp::SubscriptionOptions sub_options;
    sub_options.callback_group = param_callback_group_;
    param_event_sub_ = this->create_subscription<rcl_interfaces::msg::ParameterEvent>(
        "/parameter_events", rclcpp::QoS(10),
        std::bind(&MqttNode::onParameterEvent, this, std::placeholders::_1),
        sub_options);
    RCLCPP_INFO(this->get_logger(), "MQTT node started");
  }

  /**
   * @brief 析构函数
   *
   * 确保 MQTT 连接在节点关闭时正确断开
   */
  ~MqttNode() override
  {
    if (client_ == nullptr)
    {
      return;
    }

    try
    {
      if (client_->is_connected())
      {
        client_->disconnect()->wait();
      }
    }
    catch (const mqtt::exception &e)
    {
      RCLCPP_ERROR(this->get_logger(), "MQTT disconnect error: %s", e.what());
    }
  }

private:
  /**
   * @brief 摄像头信息结构体
   *
   * 存储单个摄像头的完整状态信息，用于上报给上位机
   */
  struct CameraInfo
  {
    std::string location; ///< 摄像头物理位置描述
    std::string http_url; ///< HTTP 流地址（用于 Web 预览）
    int width{0};         ///< 视频分辨率宽度
    int height{0};        ///< 视频分辨率高度
    double fps{0.0};
    double scale{1.0};                ///< 流媒体缩放比例
    double confidence_threshold{0.5}; ///< 目标检测置信度阈值
    double nms_threshold{0.5};        ///< NMS 非极大值抑制阈值
  };

  /**
   * @brief 声明所有可配置参数
   *
   * 这些参数可以通过 launch 文件或 ros2 param set 命令动态设置
   */
  void declareParameters()
  {
    // MQTT 连接参数
    this->declare_parameter<std::string>("broker", "100.127.154.73");   // MQTT broker 地址或域名
    this->declare_parameter<int>("port", 1883);                         // MQTT 端口号
    this->declare_parameter<std::string>("client_id", "jetson");        // 客户端标识符
    this->declare_parameter<std::string>("device", "jetson-orin-nano"); // 设备标识符 (如: jetson-orin-nano)

    // MQTT 主题配置
    this->declare_parameter<std::string>("subscribe_topic", "/jetson/camera/command"); // 订阅主题（接收上位机命令）
    this->declare_parameter<std::string>("call_topic", "jetson/call/command");         // 通话控制主题
    this->declare_parameter<std::string>("publish_topic", "/jetson/camera/command");   // 发布主题（目前未使用）

    // 状态上报配置
    this->declare_parameter<std::string>("info_topic", "jetson/info"); // 状态信息发布主题
    this->declare_parameter<std::string>("alarm_topic", "jetson/alarm");
    this->declare_parameter<double>("report_interval_sec", 1.0);       // 上报间隔（秒）

    // 单摄像头配置
    this->declare_parameter<std::string>("camera_id", "001");
    this->declare_parameter<std::string>("camera_location", "");
    this->declare_parameter<std::string>("camera_http_url", "");
  }

  /**
   * @brief 从参数服务器加载参数值
   *
   * 将声明的参数读取到成员变量中，并初始化静态元数据
   */
  void loadParameters()
  {
    // 加载 MQTT 连接参数
    this->get_parameter("broker", broker_);
    this->get_parameter("port", port_);
    this->get_parameter("client_id", client_id_);
    this->get_parameter("device", device_);
    this->get_parameter("subscribe_topic", subscribe_topic_);
    this->get_parameter("call_topic", call_topic_);
    this->get_parameter("publish_topic", publish_topic_);

    // 加载上报配置
    this->get_parameter("info_topic", info_topic_);
    this->get_parameter("alarm_topic", alarm_topic_);
    this->get_parameter("report_interval_sec", report_interval_sec_);
    this->get_parameter("camera_id", camera_id_);
    this->get_parameter("camera_location", camera_location_);
    this->get_parameter("camera_http_url", camera_http_url_);

    if (camera_id_.empty())
    {
      camera_id_ = "001";
    }

    // 同步静态配置（位置、HTTP URL）到缓存
    syncStaticCameraMetadata();
  }

  /**
   * @brief 同步静态摄像头元数据
   *
   * 位置信息和 HTTP URL 是静态配置，从 launch 文件传入后不再变化。
   * 将这些信息同步到内部的 info_cache_ 中备用。
   */
  void syncStaticCameraMetadata()
  {
    std::lock_guard<std::mutex> lock(info_mutex_);
    auto &info = info_cache_[camera_id_];
    info.location = camera_location_;
    info.http_url = camera_http_url_;
  }

  /**
   * @brief 构建 MQTT broker 地址
   * @return 完整的主机地址字符串
   *
   * 支持两种格式：
   * - 已包含协议前缀：如 "tcp://localhost:1883"
   * - 仅主机名：如 "localhost"（自动添加 tcp:// 前缀）
   */
  std::string buildServerAddress() const
  {
    if (broker_.find("://") != std::string::npos)
    {
      return broker_; // 已有协议前缀，直接返回
    }
    return "tcp://" + broker_ + ":" + std::to_string(port_);
  }

  /**
   * @brief 创建状态上报定时器
   *
   * 创建一个 ROS2  wall timer，按照指定的间隔周期性触发状态上报
   */
  void createReportTimer()
  {
    // 将秒转换为毫秒，确保间隔至少 100ms
    const int interval_ms = std::max(100, static_cast<int>(std::round(report_interval_sec_ * 1000.0)));
    report_timer_ = this->create_wall_timer(
        std::chrono::milliseconds(interval_ms),
        std::bind(&MqttNode::reportInfoTimerCallback, this),
        report_callback_group_);
    RCLCPP_INFO(this->get_logger(), "Info report timer started, interval: %.2f sec", report_interval_sec_);
  }

  /**
   * @brief 刷新本地动态参数
   *
   * 通过 ROS2 参数服务检查是否有参数被外部动态修改。
   * 支持动态修改的参数包括：
   * - camera_id: 摄像头 ID
   * - camera_location: 位置信息
   * - camera_http_url: HTTP URL
   * - report_interval_sec: 上报间隔
   * - info_topic: 上报主题
   *
   * 如果检测到上报间隔变化，会重建定时器
   */
  void refreshLocalDynamicParameters()
  {
    // 用于存储从参数服务器获取的最新值
    std::string latest_camera_id = camera_id_;
    std::string latest_location = camera_location_;
    std::string latest_http_url = camera_http_url_;
    double latest_interval = report_interval_sec_;
    std::string latest_info_topic = info_topic_;

    // 从参数服务器获取最新值
    (void)this->get_parameter("camera_id", latest_camera_id);
    (void)this->get_parameter("camera_location", latest_location);
    (void)this->get_parameter("camera_http_url", latest_http_url);
    (void)this->get_parameter("report_interval_sec", latest_interval);
    (void)this->get_parameter("info_topic", latest_info_topic);

    if (!latest_camera_id.empty())
    {
      camera_id_ = latest_camera_id;
    }

    // 更新上报主题
    if (!latest_info_topic.empty())
    {
      info_topic_ = latest_info_topic;
    }

    camera_location_ = latest_location;
    camera_http_url_ = latest_http_url;

    // 同步静态元数据
    syncStaticCameraMetadata();

    // 检查并处理上报间隔变化
    if (latest_interval <= 0.0)
    {
      latest_interval = 1.0;
    }

    if (std::abs(latest_interval - report_interval_sec_) > 1e-6)
    {
      report_interval_sec_ = latest_interval;
      createReportTimer();

      RCLCPP_INFO(this->get_logger(), "Report interval updated to %.2f sec", report_interval_sec_);
    }
  }

  /**
   * @brief 生成 camera_node 可能的节点名称列表
   * @param camera_id 摄像头 ID
   * @return 节点名称向量
   *
   * 由于摄像头节点可能运行在不同命名空间下，生成多种可能的节点名：
   * - 有命名空间：/camera_xxx/camera_node
   * - 无命名空间：/camera_node
   */
  std::vector<std::string> cameraNodeCandidates(const std::string &camera_id) const
  {
    std::vector<std::string> names;
    if (!camera_id.empty())
    {
      names.push_back("/camera_" + camera_id + "/camera_node");
    }
    names.push_back("/camera_node");
    return names;
  }

  /**
   * @brief 生成 detector_node 可能的节点名称列表
   * @param camera_id 摄像头 ID
   * @return 节点名称向量
   */
  std::vector<std::string> detectorNodeCandidates(const std::string &camera_id) const
  {
    std::vector<std::string> names;
    if (!camera_id.empty())
    {
      names.push_back("/camera_" + camera_id + "/detector_node");
    }
    names.push_back("/detector_node");
    return names;
  }

  /**
   * @brief 生成 streamer_node 可能的节点名称列表
   * @param camera_id 摄像头 ID
   * @return 节点名称向量
   */
  std::vector<std::string> streamerNodeCandidates(const std::string &camera_id) const
  {
    std::vector<std::string> names;
    if (!camera_id.empty())
    {
      names.push_back("/camera_" + camera_id + "/streamer_node");
    }
    names.push_back("/streamer_node");
    return names;
  }

  /**
   * @brief 生成 monitor_node 可能的节点名称列表
   * @param camera_id 摄像头 ID
   * @return 节点名称向量
   */
  std::vector<std::string> monitorNodeCandidates(const std::string &camera_id) const
  {
    std::vector<std::string> names;
    if (!camera_id.empty())
    {
      names.push_back("/camera_" + camera_id + "/monitor_node");
    }
    names.push_back("/monitor_node");
    return names;
  }

  /**
   * @brief 获取或创建异步参数客户端
   * @param node_name 目标节点名称
   * @return 异步参数客户端智能指针
   *
   * 使用缓存机制避免重复创建相同的参数客户端
   */
  std::shared_ptr<rclcpp::AsyncParametersClient> getOrCreateParamClient(const std::string &node_name)
  {
    const auto it = param_clients_.find(node_name);
    if (it != param_clients_.end())
    {
      return it->second;
    }

    auto client = std::make_shared<rclcpp::AsyncParametersClient>(
        this, node_name, rmw_qos_profile_parameters, param_callback_group_);
    param_clients_[node_name] = client;
    return client;
  }

  bool nodeExists(const std::string &node_name)
  {
    auto graph = this->get_node_graph_interface();
    const auto node_names_and_namespaces = graph->get_node_names_and_namespaces();
    for (const auto &item : node_names_and_namespaces)
    {
      std::string full_name = item.second;
      if (full_name.empty() || full_name.back() != '/')
      {
        full_name += '/';
      }
      full_name += item.first;
      if (full_name == node_name)
      {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief 同步方式获取节点参数
   * @param node_name 目标节点名称
   * @param parameter_names 要获取的参数名列表
   * @param parameters 输出：获取到的参数向量
   * @return 是否成功获取参数
   *
   * 使用 200ms 超时等待，避免异步调用导致的阻塞问题
   */
  bool fetchParametersSync(
      const std::string &node_name,
      const std::vector<std::string> &parameter_names,
      std::vector<rclcpp::Parameter> &parameters)
  {
    const auto now = std::chrono::steady_clock::now();
    const auto backoff_it = next_query_time_.find(node_name);
    if (backoff_it != next_query_time_.end() && now < backoff_it->second)
    {
      return false;
    }

    if (!nodeExists(node_name))
    {
      next_query_time_[node_name] = now + std::chrono::seconds(10);
      return false;
    }

    auto client = getOrCreateParamClient(node_name);
    if (!client->service_is_ready())
    {
      next_query_time_[node_name] = now + std::chrono::seconds(2);
      return false;
    }

    auto future = client->get_parameters(parameter_names);
    const auto status = future.wait_for(std::chrono::milliseconds(1000));
    if (status != std::future_status::ready)
    {
      next_query_time_[node_name] = now + std::chrono::seconds(30);
      RCLCPP_WARN_THROTTLE(
          this->get_logger(), *this->get_clock(), 120000,
          "Timed out fetching parameters from %s", node_name.c_str());
      return false;
    }

    try
    {
      parameters = future.get();
      next_query_time_[node_name] = now;
      return true;
    }
    catch (const std::exception &e)
    {
      next_query_time_[node_name] = now + std::chrono::seconds(2);
      RCLCPP_WARN(this->get_logger(), "Failed to fetch parameters from %s: %s", node_name.c_str(), e.what());
      return false;
    }
  }

  bool setParametersSync(
      const std::string &node_name,
      const std::vector<rclcpp::Parameter> &parameters)
  {
    if (!nodeExists(node_name))
    {
      return false;
    }

    auto client = getOrCreateParamClient(node_name);
    if (!client->service_is_ready())
    {
      return false;
    }

    auto future = client->set_parameters(parameters);
    const auto status = future.wait_for(std::chrono::milliseconds(1500));
    if (status != std::future_status::ready)
    {
      RCLCPP_WARN(this->get_logger(), "Timed out setting parameters on %s", node_name.c_str());
      return false;
    }

    try
    {
      const auto results = future.get();
      for (size_t i = 0; i < results.size(); ++i)
      {
        if (!results[i].successful)
        {
          RCLCPP_WARN(
              this->get_logger(), "Failed to set %s on %s: %s",
              parameters[i].get_name().c_str(), node_name.c_str(), results[i].reason.c_str());
          return false;
        }
      }
      return true;
    }
    catch (const std::exception &e)
    {
      RCLCPP_WARN(this->get_logger(), "Failed to set parameters on %s: %s", node_name.c_str(), e.what());
      return false;
    }
  }

  bool cameraIdConfigured(const std::string &camera_id) const
  {
    return camera_id == camera_id_;
  }

  void updateCacheForCommand(
      const std::string &camera_id,
      const std::optional<double> &confidence_threshold,
      const std::optional<double> &nms_threshold)
  {
    std::lock_guard<std::mutex> lock(info_mutex_);
    auto &info = info_cache_[camera_id];
    if (confidence_threshold.has_value())
    {
      info.confidence_threshold = *confidence_threshold;
    }
    if (nms_threshold.has_value())
    {
      info.nms_threshold = *nms_threshold;
    }
  }

  bool extractNumericField(const Json::Value &value, const char *field_name, std::optional<double> &output)
  {
    if (!value.isMember(field_name) || value[field_name].isNull())
    {
      return true;
    }

    const auto &field = value[field_name];
    if (!field.isNumeric())
    {
      RCLCPP_WARN(this->get_logger(), "Field %s must be numeric", field_name);
      return false;
    }

    output = field.asDouble();
    return true;
  }

  bool handleParameterCommand(const Json::Value &root)
  {
    if (!root["camera_id"].isString())
    {
      RCLCPP_WARN(this->get_logger(), "Missing valid camera_id in parameters command");
      return false;
    }
    const bool has_value = root["value"].isObject();
    const bool has_duration = root.isMember("duration") && root["duration"].isNumeric();
    if (!has_value && !has_duration)
    {
      RCLCPP_WARN(this->get_logger(), "Missing valid value or duration in parameters command");
      return false;
    }

    const std::string camera_id = root["camera_id"].asString();
    if (!cameraIdConfigured(camera_id))
    {
      RCLCPP_WARN(this->get_logger(), "Unknown camera_id in MQTT command: %s", camera_id.c_str());
      return false;
    }

    bool success = true;
    bool any_change_applied = false;

    // 处理检测器参数（confidence_threshold, nms_threshold）
    if (has_value)
    {
      const Json::Value &value = root["value"];
      std::optional<double> confidence_threshold;
      std::optional<double> nms_threshold;
      if (!extractNumericField(value, "confidence_threshold", confidence_threshold) ||
          !extractNumericField(value, "iou_threshold", nms_threshold))
      {
        return false;
      }

      if (confidence_threshold.has_value() && (*confidence_threshold < 0.0 || *confidence_threshold > 1.0))
      {
        RCLCPP_WARN(this->get_logger(), "confidence_threshold must be between 0 and 1");
        return false;
      }
      if (nms_threshold.has_value() && (*nms_threshold < 0.0 || *nms_threshold > 1.0))
      {
        RCLCPP_WARN(this->get_logger(), "iou_threshold must be between 0 and 1");
        return false;
      }

      if (confidence_threshold.has_value() || nms_threshold.has_value())
      {
        bool detector_updated = false;
        for (const auto &node_name : detectorNodeCandidates(camera_id))
        {
          std::vector<rclcpp::Parameter> current_parameters;
          bool current_loaded = fetchParametersSync(node_name, {"confidence_threshold", "nms_threshold"}, current_parameters);
          std::vector<rclcpp::Parameter> detector_parameters;
          if (current_loaded)
          {
            std::optional<double> current_confidence_threshold;
            std::optional<double> current_nms_threshold;
            for (const auto &parameter : current_parameters)
            {
              if (parameter.get_name() == "confidence_threshold")
              {
                if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE)
                {
                  current_confidence_threshold = parameter.as_double();
                }
                else if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER)
                {
                  current_confidence_threshold = static_cast<double>(parameter.as_int());
                }
              }
              else if (parameter.get_name() == "nms_threshold")
              {
                if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE)
                {
                  current_nms_threshold = parameter.as_double();
                }
                else if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER)
                {
                  current_nms_threshold = static_cast<double>(parameter.as_int());
                }
              }
            }

            if (confidence_threshold.has_value() && (!current_confidence_threshold.has_value() || std::abs(*current_confidence_threshold - *confidence_threshold) > 1e-6))
            {
              detector_parameters.emplace_back("confidence_threshold", *confidence_threshold);
            }
            if (nms_threshold.has_value() && (!current_nms_threshold.has_value() || std::abs(*current_nms_threshold - *nms_threshold) > 1e-6))
            {
              detector_parameters.emplace_back("nms_threshold", *nms_threshold);
            }
          }
          else
          {
            if (confidence_threshold.has_value())
            {
              detector_parameters.emplace_back("confidence_threshold", *confidence_threshold);
            }
            if (nms_threshold.has_value())
            {
              detector_parameters.emplace_back("nms_threshold", *nms_threshold);
            }
          }

          if (detector_parameters.empty())
          {
            detector_updated = true;
            break;
          }

          if (setParametersSync(node_name, detector_parameters))
          {
            detector_updated = true;
            any_change_applied = true;
            break;
          }
        }
        success = success && detector_updated;
        if (success)
        {
          updateCacheForCommand(camera_id, confidence_threshold, nms_threshold);
        }
      }
    }

    // 处理 duration → monitor_node 的 window_seconds 参数
    if (has_duration)
    {
      const int duration = root["duration"].asInt();
      if (duration >= 1)
      {
        bool monitor_updated = false;
        for (const auto &node_name : monitorNodeCandidates(camera_id))
        {
          if (setParametersSync(node_name, {rclcpp::Parameter("window_seconds", duration)}))
          {
            monitor_updated = true;
            any_change_applied = true;
            RCLCPP_INFO(this->get_logger(), "Updated window_seconds=%d for camera_%s", duration, camera_id.c_str());
            break;
          }
        }
        success = success && monitor_updated;
      }
      else
      {
        RCLCPP_WARN(this->get_logger(), "duration must be >= 1, got %d", duration);
      }
    }

    if (success)
    {
      if (any_change_applied)
      {
        RCLCPP_INFO(this->get_logger(), "Applied MQTT parameter command to camera_%s", camera_id.c_str());
      }
      else
      {
        RCLCPP_INFO(this->get_logger(), "Skipped MQTT parameter command for camera_%s because all values are unchanged", camera_id.c_str());
      }
    }

    return success;
  }

  bool handleCallCommand(const Json::Value &root)
  {
    if (!root["device"].isString())
    {
      RCLCPP_WARN(this->get_logger(), "Call command missing device field");
      return false;
    }

    const std::string target_device = root["device"].asString();
    if (target_device != device_)
    {
      RCLCPP_INFO(this->get_logger(), "Call command ignored. Target device '%s' does not match current device '%s'", target_device.c_str(), device_.c_str());
      return false;
    }

    const std::string type = root["type"].asString();
    std_msgs::msg::String msg;

    if (type == "intercom_start")
    {
      if (!root["url"].isString())
      {
        RCLCPP_WARN(this->get_logger(), "intercom_start missing url field");
        return false;
      }
      msg.data = root["url"].asString();
      RCLCPP_INFO(this->get_logger(), "Starting intercom with url: %s", msg.data.c_str());
      intercom_pub_->publish(msg);
      return true;
    }
    else if (type == "intercom_stop")
    {
      msg.data = "STOP";
      RCLCPP_INFO(this->get_logger(), "Stopping intercom");
      intercom_pub_->publish(msg);
      return true;
    }

    return false;
  }

  void handleMqttMessage(const std::string &topic, const std::string &payload)
  {
    if (topic != subscribe_topic_ && topic != call_topic_)
    {
      return;
    }

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    std::string errors;
    if (!reader->parse(payload.data(), payload.data() + payload.size(), &root, &errors) || !root.isObject())
    {
      RCLCPP_WARN(this->get_logger(), "Failed to parse MQTT JSON: %s", errors.c_str());
      return;
    }

    if (!root["type"].isString())
    {
      RCLCPP_WARN(this->get_logger(), "MQTT command missing type field");
      return;
    }

    const std::string type = root["type"].asString();

    if (topic == subscribe_topic_)
    {
      if (type == "parameters")
      {
        (void)handleParameterCommand(root);
        return;
      }
    }
    else if (topic == call_topic_)
    {
      if (type == "intercom_start" || type == "intercom_stop")
      {
        (void)handleCallCommand(root);
        return;
      }
    }

    RCLCPP_WARN(this->get_logger(), "Unsupported MQTT command type %s on topic %s", type.c_str(), topic.c_str());
  }

  /**
   * @brief JSON 字符串转义处理
   * @param value 输入字符串
   * @return 转义后的字符串
   *
   * 处理 JSON 特殊字符：\ " \n \r \t
   * 防止 JSON 解析错误
   */
  static std::string escapeJson(const std::string &value)
  {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char c : value)
    {
      switch (c)
      {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += c;
        break;
      }
    }
    return escaped;
  }

  void onParameterEvent(const rcl_interfaces::msg::ParameterEvent::SharedPtr event)
  {
    const std::string &node_name = event->node;
    std::string target_camera_id;
    bool is_streamer = false;
    bool is_detector = false;

    auto det_candidates = detectorNodeCandidates(camera_id_);
    if (std::find(det_candidates.begin(), det_candidates.end(), node_name) != det_candidates.end())
    {
      target_camera_id = camera_id_;
      is_detector = true;
    }
    auto str_candidates = streamerNodeCandidates(camera_id_);
    if (!is_detector && std::find(str_candidates.begin(), str_candidates.end(), node_name) != str_candidates.end())
    {
      target_camera_id = camera_id_;
      is_streamer = true;
    }

    if (target_camera_id.empty())
    {
      return;
    }

    std::lock_guard<std::mutex> lock(info_mutex_);
    auto &info = info_cache_[target_camera_id];
    bool updated = false;

    auto process_params = [&](const std::vector<rcl_interfaces::msg::Parameter> &params)
    {
      for (const auto &p : params)
      {
        if (is_detector)
        {
          if (p.name == "confidence_threshold" && p.value.type == rcl_interfaces::msg::ParameterType::PARAMETER_DOUBLE)
          {
            info.confidence_threshold = p.value.double_value;
            updated = true;
          }
          else if (p.name == "nms_threshold" && p.value.type == rcl_interfaces::msg::ParameterType::PARAMETER_DOUBLE)
          {
            info.nms_threshold = p.value.double_value;
            updated = true;
          }
        }
        else if (is_streamer)
        {
          if (p.name == "output_width" && p.value.type == rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER)
          {
            info.width = static_cast<int>(p.value.integer_value);
            updated = true;
          }
          else if (p.name == "output_height" && p.value.type == rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER)
          {
            info.height = static_cast<int>(p.value.integer_value);
            updated = true;
          }
          else if (p.name == "output_fps")
          {
            if (p.value.type == rcl_interfaces::msg::ParameterType::PARAMETER_DOUBLE)
            {
              info.fps = p.value.double_value;
              updated = true;
            }
            else if (p.value.type == rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER)
            {
              info.fps = static_cast<double>(p.value.integer_value);
              updated = true;
            }
          }
          else if (p.name == "scale")
          {
            if (p.value.type == rcl_interfaces::msg::ParameterType::PARAMETER_DOUBLE)
            {
              info.scale = p.value.double_value;
              updated = true;
            }
            else if (p.value.type == rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER)
            {
              info.scale = static_cast<double>(p.value.integer_value);
              updated = true;
            }
          }
        }
      }
    };

    process_params(event->new_parameters);
    process_params(event->changed_parameters);

    if (updated)
    {
      // RCLCPP_INFO(this->get_logger(), "Cache updated via parameter event from %s", node_name.c_str());
    }
  }

  bool requestStreamerRuntimeInfo(const std::string &camera_id)
  {
    const auto candidates = streamerNodeCandidates(camera_id);
    for (const auto &node_name : candidates)
    {
      std::vector<rclcpp::Parameter> params;
      if (!fetchParametersSync(node_name, {"output_width", "output_height", "output_fps", "scale"}, params))
      {
        continue;
      }

      CameraInfo info;
      {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info = info_cache_[camera_id];
      }

      // 解析参数值
      for (const auto &p : params)
      {
        if (p.get_name() == "output_width" && p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER)
        {
          info.width = static_cast<int>(p.as_int());
        }
        else if (p.get_name() == "output_height" && p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER)
        {
          info.height = static_cast<int>(p.as_int());
        }
        else if (p.get_name() == "output_fps")
        {
          if (p.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE)
          {
            info.fps = p.as_double();
          }
          else if (p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER)
          {
            info.fps = static_cast<double>(p.as_int());
          }
        }
        else if (p.get_name() == "scale")
        {
          if (p.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE)
          {
            info.scale = p.as_double();
          }
          else if (p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER)
          {
            info.scale = static_cast<double>(p.as_int());
          }
        }
      }

      {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info_cache_[camera_id] = info;
      }
      return true;
    }
    return false;
  }

  /**
   * @brief 请求目标检测阈值参数
   * @param camera_id 摄像头 ID
   *
   * 通过异步参数服务从 detector_node 获取：
   * - confidence_threshold: 置信度阈值
   * - nms_threshold: NMS 阈值
   */
  bool requestDetectorThresholds(const std::string &camera_id)
  {
    const auto candidates = detectorNodeCandidates(camera_id);
    for (const auto &node_name : candidates)
    {
      std::vector<rclcpp::Parameter> params;
      if (!fetchParametersSync(node_name, {"confidence_threshold", "nms_threshold"}, params))
      {
        continue;
      }

      CameraInfo info;
      {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info = info_cache_[camera_id];
      }

      for (const auto &p : params)
      {
        if (p.get_name() == "confidence_threshold")
        {
          if (p.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE)
          {
            info.confidence_threshold = p.as_double();
          }
          else if (p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER)
          {
            info.confidence_threshold = static_cast<double>(p.as_int());
          }
        }
        else if (p.get_name() == "nms_threshold")
        {
          if (p.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE)
          {
            info.nms_threshold = p.as_double();
          }
          else if (p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER)
          {
            info.nms_threshold = static_cast<double>(p.as_int());
          }
        }
      }

      {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info_cache_[camera_id] = info;
      }
      return true;
    }
    return false;
  }

  /**
   * @brief 构建状态信息 JSON payload
   * @return JSON 格式的字符串
   *
   * 生成的上报 JSON 结构：
   * {
   *   "timestamp_ns": 1234567890123456789,
   *   "cameras": [
   *     {
   *       "id": "001",
   *       "location": "生产车间A区",
   *       "http_url": "http://192.168.1.100:8080/stream",
   *       "resolution": {"width": 1280, "height": 720, "fps": 60},
   *       "scale": 1.0,
   *       "confidence_threshold": 0.5,
   *       "nms_threshold": 0.5
   *     }
   *   ]
   * }
   */
  std::string buildInfoPayload()
  {
    // 获取当前状态快照（加锁保护）
    std::unordered_map<std::string, CameraInfo> snapshot;
    {
      std::lock_guard<std::mutex> lock(info_mutex_);
      snapshot = info_cache_;
    }

    // 构建 JSON 字符串
    std::ostringstream oss;
    oss << "{\"timestamp_ns\":" << this->now().nanoseconds() << ",";
    oss << "\"device\":\"" << escapeJson(device_) << "\",";
    oss << "\"cameras\":[";
    const auto it = snapshot.find(camera_id_);
    CameraInfo info;
    if (it != snapshot.end())
    {
      info = it->second;
    }

    oss << "{";
    oss << "\"id\":\"" << camera_id_ << "\",";
    oss << "\"location\":\"" << escapeJson(info.location) << "\",";
    oss << "\"http_url\":\"" << escapeJson(info.http_url) << "\",";
    oss << "\"resolution\":{\"width\":" << info.width << ",\"height\":" << info.height << ",\"fps\":" << info.fps << "},";
    oss << "\"scale\":" << info.scale << ",";
    oss << "\"confidence_threshold\":" << info.confidence_threshold << ",";
    oss << "\"nms_threshold\":" << info.nms_threshold;
    oss << "}";
    oss << "]}";
    return oss.str();
  }

  /**
   * @brief 发布状态信息到 MQTT
   * @param payload JSON 格式的状态数据
   *
   * 将构建好的 JSON payload 发布到 info_topic 指定的 MQTT 主题
   */
  void publishToTopic(const std::string &topic, const std::string &payload, const char *topic_label)
  {
    if (client_ == nullptr || !client_->is_connected())
    {
      RCLCPP_WARN_THROTTLE(
          this->get_logger(), *this->get_clock(), 3000,
          "Cannot publish %s, MQTT not connected", topic_label);
      return;
    }

    try
    {
      auto msg = mqtt::make_message(topic, payload, 1, false);
      client_->publish(msg)->wait();
    }
    catch (const mqtt::exception &e)
    {
      RCLCPP_ERROR(this->get_logger(), "Publish %s failed: %s", topic_label, e.what());
    }
  }

  void publishToInfoTopic(const std::string &payload)
  {
    publishToTopic(info_topic_, payload, info_topic_.c_str());
  }

  void publishToAlarmTopic(const std::string &payload)
  {
    publishToTopic(alarm_topic_, payload, alarm_topic_.c_str());
  }

  void onAlarmEvent(const std_msgs::msg::String::SharedPtr msg)
  {
    if (!msg || msg->data.empty())
    {
      RCLCPP_WARN(this->get_logger(), "收到空的报警事件，跳过 MQTT 转发");
      return;
    }

    publishToAlarmTopic(msg->data);
  }

  /**
   * @brief 定时上报回调函数
   *
   * 每次定时器触发时执行：
   * 1. 刷新本地动态参数
   * 2. 从各节点获取最新运行时信息
   * 3. 构建 JSON payload
   * 4. 发布到 MQTT
   */
  void reportInfoTimerCallback()
  {
    refreshLocalDynamicParameters(); // 检查参数是否有更新

    // 仅在未成功获取过初始参数时，才去主动拉取（彻底消除定时的 IPC 轮询开销）
    if (!initial_fetch_streamer_)
    {
      if (requestStreamerRuntimeInfo(camera_id_))
      {
        initial_fetch_streamer_ = true;
      }
    }
    if (!initial_fetch_detector_)
    {
      if (requestDetectorThresholds(camera_id_))
      {
        initial_fetch_detector_ = true;
      }
    }

    const std::string payload = buildInfoPayload(); // 构建 JSON
    publishToInfoTopic(payload);                    // 发布到 MQTT
  }

  // ==================== 成员变量 ====================

  std::unique_ptr<mqtt::async_client> client_; ///< Paho MQTT 异步客户端
  MqttCallback cb_;                            ///< MQTT 消息回调处理

  // MQTT 连接配置
  std::string broker_;          ///< MQTT broker 地址
  int port_{1883};              ///< MQTT 端口
  std::string client_id_;       ///< 客户端 ID
  std::string device_;          ///< 设备标识符
  std::string subscribe_topic_; ///< 订阅主题（命令）
  std::string call_topic_;      ///< 远程通话主题
  std::string publish_topic_;   ///< 发布主题（状态）

  // 上报配置
  std::string info_topic_;                    ///< 信息发布主题
  std::string alarm_topic_;                   ///< 报警事件发布主题
  double report_interval_sec_{1.0};           ///< 上报间隔（秒）
  std::string camera_id_;                     ///< 摄像头 ID
  std::string camera_location_;               ///< 摄像头位置
  std::string camera_http_url_;               ///< HTTP URL
  rclcpp::CallbackGroup::SharedPtr report_callback_group_{
      this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive)};
  rclcpp::CallbackGroup::SharedPtr param_callback_group_{
      this->create_callback_group(rclcpp::CallbackGroupType::Reentrant)};

  // 内部状态
  rclcpp::TimerBase::SharedPtr report_timer_;                                                     ///< 定时上报计时器
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr intercom_pub_;                              ///< 通话控制发布器
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr alarm_event_sub_;                        ///< 报警事件订阅器
  rclcpp::Subscription<rcl_interfaces::msg::ParameterEvent>::SharedPtr param_event_sub_;          ///< 参数事件订阅器
  bool initial_fetch_streamer_{false};                                                            ///< 推流器是否已完成初始参数获取
  bool initial_fetch_detector_{false};                                                            ///< 检测器是否已完成初始参数获取
  std::unordered_map<std::string, std::shared_ptr<rclcpp::AsyncParametersClient>> param_clients_; ///< 参数客户端缓存
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> next_query_time_;
  std::unordered_map<std::string, CameraInfo> info_cache_; ///< 摄像头信息缓存
  std::mutex info_mutex_;                                  ///< 保护 info_cache_ 的互斥锁
};

/**
 * @brief 程序入口
 * @param argc 参数个数
 * @param argv 参数列表
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<MqttNode>();
  rclcpp::executors::MultiThreadedExecutor executor;
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
