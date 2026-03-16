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

#include <mqtt/async_client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <mutex>
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
  explicit MqttCallback(rclcpp::Logger logger)
  : logger_(std::move(logger))
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
  }

private:
  rclcpp::Logger logger_;  ///< ROS2 日志器
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
  : Node("mqtt_node"), cb_(this->get_logger())
  {
    // 步骤1: 声明所有可配置参数
    declareParameters();
    // 步骤2: 从参数服务器加载参数值
    loadParameters();

    // 参数校验：确保上报间隔合法
    if (report_interval_sec_ <= 0.0) {
      report_interval_sec_ = 1.0;
    }

    // 步骤3: 构建 MQTT broker 地址并创建客户端
    const std::string server_address = buildServerAddress();
    client_ = std::make_unique<mqtt::async_client>(server_address, client_id_);
    client_->set_callback(cb_);

    // 步骤4: 尝试建立 MQTT 连接
    try {
      mqtt::connect_options conn_opts;
      conn_opts.set_keep_alive_interval(20);  // 20秒保活间隔
      conn_opts.set_clean_session(true);       // 清除旧会话
      client_->connect(conn_opts)->wait();
      RCLCPP_INFO(this->get_logger(), "Connected to MQTT broker: %s", server_address.c_str());

      // 步骤5: 订阅控制命令主题
      client_->subscribe(subscribe_topic_, 1)->wait();
      RCLCPP_INFO(this->get_logger(), "Subscribed to: %s", subscribe_topic_.c_str());
    } catch (const mqtt::exception &e) {
      RCLCPP_ERROR(this->get_logger(), "MQTT connection failed: %s", e.what());
    }

    // 步骤6: 创建定时器用于周期性上报状态
    createReportTimer();
    RCLCPP_INFO(this->get_logger(), "MQTT node started");
  }

  /**
   * @brief 析构函数
   *
   * 确保 MQTT 连接在节点关闭时正确断开
   */
  ~MqttNode() override
  {
    if (client_ == nullptr) {
      return;
    }

    try {
      if (client_->is_connected()) {
        client_->disconnect()->wait();
      }
    } catch (const mqtt::exception &e) {
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
    std::string location;              ///< 摄像头物理位置描述
    std::string http_url;              ///< HTTP 流地址（用于 Web 预览）
    int width{0};                      ///< 视频分辨率宽度
    int height{0};                     ///< 视频分辨率高度
    int fps{0};                        ///< 帧率
    double scale{1.0};                 ///< 流媒体缩放比例
    double confidence_threshold{0.5};  ///< 目标检测置信度阈值
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
    this->declare_parameter<std::string>("broker", "fnas");           // MQTT broker 地址或域名
    this->declare_parameter<int>("port", 1883);                      // MQTT 端口号
    this->declare_parameter<std::string>("client_id", "jetson");     // 客户端标识符

    // MQTT 主题配置
    this->declare_parameter<std::string>("subscribe_topic", "factory/camera/001/command");   // 订阅主题（接收上位机命令）
    this->declare_parameter<std::string>("publish_topic", "factory/camera_001/status");    // 发布主题（目前未使用）

    // 状态上报配置
    this->declare_parameter<std::string>("info_topic", "/jetson/info");       // 状态信息发布主题
    this->declare_parameter<double>("report_interval_sec", 1.0);              // 上报间隔（秒）

    // 摄像头配置（支持多摄像头）
    this->declare_parameter<std::vector<std::string>>("camera_ids", std::vector<std::string>{"001"});           // 摄像头 ID 列表
    this->declare_parameter<std::vector<std::string>>("camera_locations", std::vector<std::string>{});         // 摄像头位置列表
    this->declare_parameter<std::vector<std::string>>("camera_http_urls", std::vector<std::string>{});         // HTTP 流地址列表
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
    this->get_parameter("subscribe_topic", subscribe_topic_);
    this->get_parameter("publish_topic", publish_topic_);

    // 加载上报配置
    this->get_parameter("info_topic", info_topic_);
    this->get_parameter("report_interval_sec", report_interval_sec_);
    this->get_parameter("camera_ids", camera_ids_);
    this->get_parameter("camera_locations", camera_locations_);
    this->get_parameter("camera_http_urls", camera_http_urls_);

    // 默认为空列表提供保障
    if (camera_ids_.empty()) {
      camera_ids_.push_back("001");
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
    for (size_t i = 0; i < camera_ids_.size(); ++i) {
      auto &info = info_cache_[camera_ids_[i]];
      // 位置信息：从 camera_locations 数组中按索引获取
      info.location = i < camera_locations_.size() ? camera_locations_[i] : "";
      // HTTP URL：从 camera_http_urls 数组中按索引获取
      info.http_url = i < camera_http_urls_.size() ? camera_http_urls_[i] : "";
    }
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
    if (broker_.find("://") != std::string::npos) {
      return broker_;  // 已有协议前缀，直接返回
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
      std::bind(&MqttNode::reportInfoTimerCallback, this));
    RCLCPP_INFO(this->get_logger(), "Info report timer started, interval: %.2f sec", report_interval_sec_);
  }

  /**
   * @brief 刷新本地动态参数
   *
   * 通过 ROS2 参数服务检查是否有参数被外部动态修改。
   * 支持动态修改的参数包括：
   * - camera_ids: 摄像头 ID 列表
   * - camera_locations: 位置信息
   * - camera_http_urls: HTTP URL 列表
   * - report_interval_sec: 上报间隔
   * - info_topic: 上报主题
   *
   * 如果检测到上报间隔变化，会重建定时器
   */
  void refreshLocalDynamicParameters()
  {
    // 用于存储从参数服务器获取的最新值
    std::vector<std::string> latest_camera_ids;
    std::vector<std::string> latest_locations;
    std::vector<std::string> latest_http_urls;
    double latest_interval = report_interval_sec_;
    std::string latest_info_topic = info_topic_;

    // 从参数服务器获取最新值
    (void)this->get_parameter("camera_ids", latest_camera_ids);
    (void)this->get_parameter("camera_locations", latest_locations);
    (void)this->get_parameter("camera_http_urls", latest_http_urls);
    (void)this->get_parameter("report_interval_sec", latest_interval);
    (void)this->get_parameter("info_topic", latest_info_topic);

    // 更新摄像头 ID 列表
    if (!latest_camera_ids.empty()) {
      camera_ids_ = latest_camera_ids;
    }

    // 更新上报主题
    if (!latest_info_topic.empty()) {
      info_topic_ = latest_info_topic;
    }

    // 更新位置信息
    if (!latest_locations.empty()) {
      camera_locations_ = latest_locations;
    }

    // 更新 HTTP URL
    if (!latest_http_urls.empty()) {
      camera_http_urls_ = latest_http_urls;
    }

    // 同步静态元数据
    syncStaticCameraMetadata();

    // 检查并处理上报间隔变化
    if (latest_interval <= 0.0) {
      latest_interval = 1.0;
    }

    if (std::abs(latest_interval - report_interval_sec_) > 1e-6) {
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
    if (!camera_id.empty()) {
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
    if (!camera_id.empty()) {
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
    if (!camera_id.empty()) {
      names.push_back("/camera_" + camera_id + "/streamer_node");
    }
    names.push_back("/streamer_node");
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
    if (it != param_clients_.end()) {
      return it->second;
    }

    auto client = std::make_shared<rclcpp::AsyncParametersClient>(this, node_name);
    param_clients_[node_name] = client;
    return client;
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
    auto client = getOrCreateParamClient(node_name);
    if (!client->service_is_ready()) {
      return false;
    }

    auto future = client->get_parameters(parameter_names);
    const auto status = future.wait_for(std::chrono::milliseconds(200));
    if (status != std::future_status::ready) {
      RCLCPP_WARN_THROTTLE(
        this->get_logger(), *this->get_clock(), 3000,
        "Timed out fetching parameters from %s", node_name.c_str());
      return false;
    }

    try {
      parameters = future.get();
      return true;
    } catch (const std::exception &e) {
      RCLCPP_WARN(this->get_logger(), "Failed to fetch parameters from %s: %s", node_name.c_str(), e.what());
      return false;
    }
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
    for (const char c : value) {
      switch (c) {
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

  /**
   * @brief 请求摄像头分辨率和帧率参数
   * @param camera_id 摄像头 ID
   *
   * 通过异步参数服务从 camera_node 获取：
   * - width: 视频宽度
   * - height: 视频高度
   * - framerate: 帧率
   */
  void requestCameraResolution(const std::string &camera_id)
  {
    const auto candidates = cameraNodeCandidates(camera_id);
    for (const auto &node_name : candidates) {
      std::vector<rclcpp::Parameter> params;
      if (!fetchParametersSync(node_name, {"width", "height", "framerate"}, params)) {
        continue;
      }

      CameraInfo info;
      {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info = info_cache_[camera_id];
      }

      // 解析参数值
      for (const auto &p : params) {
        if (p.get_name() == "width" && p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
          info.width = static_cast<int>(p.as_int());
        } else if (p.get_name() == "height" && p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
          info.height = static_cast<int>(p.as_int());
        } else if (p.get_name() == "framerate" && p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
          info.fps = static_cast<int>(p.as_int());
        }
      }

      {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info_cache_[camera_id] = info;
      }
      return;
    }
  }

  /**
   * @brief 请求目标检测阈值参数
   * @param camera_id 摄像头 ID
   *
   * 通过异步参数服务从 detector_node 获取：
   * - confidence_threshold: 置信度阈值
   * - nms_threshold: NMS 阈值
   */
  void requestDetectorThresholds(const std::string &camera_id)
  {
    const auto candidates = detectorNodeCandidates(camera_id);
    for (const auto &node_name : candidates) {
      std::vector<rclcpp::Parameter> params;
      if (!fetchParametersSync(node_name, {"confidence_threshold", "nms_threshold"}, params)) {
        continue;
      }

      CameraInfo info;
      {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info = info_cache_[camera_id];
      }

      for (const auto &p : params) {
        if (p.get_name() == "confidence_threshold") {
          if (p.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE) {
            info.confidence_threshold = p.as_double();
          } else if (p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
            info.confidence_threshold = static_cast<double>(p.as_int());
          }
        } else if (p.get_name() == "nms_threshold") {
          if (p.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE) {
            info.nms_threshold = p.as_double();
          } else if (p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
            info.nms_threshold = static_cast<double>(p.as_int());
          }
        }
      }

      {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info_cache_[camera_id] = info;
      }
      return;
    }
  }

  /**
   * @brief 请求流媒体缩放参数
   * @param camera_id 摄像头 ID
   *
   * 通过异步参数服务从 streamer_node 获取：
   * - scale: 视频流缩放比例
   */
  void requestStreamerScale(const std::string &camera_id)
  {
    const auto candidates = streamerNodeCandidates(camera_id);
    for (const auto &node_name : candidates) {
      std::vector<rclcpp::Parameter> params;
      if (!fetchParametersSync(node_name, {"scale"}, params)) {
        continue;
      }

      CameraInfo info;
      {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info = info_cache_[camera_id];
      }

      for (const auto &p : params) {
        if (p.get_name() == "scale") {
          if (p.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE) {
            info.scale = p.as_double();
          } else if (p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
            info.scale = static_cast<double>(p.as_int());
          }
        }
      }

      {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info_cache_[camera_id] = info;
      }
      return;
    }
  }

  /**
   * @brief 请求所有摄像头的运行时信息
   *
   * 遍历所有摄像头，分别获取：
   * 1. 分辨率和帧率（从 camera_node）
   * 2. 流媒体缩放比例（从 streamer_node）
   * 3. 检测阈值（从 detector_node）
   */
  void requestRuntimeInfo()
  {
    for (const auto &camera_id : camera_ids_) {
      requestCameraResolution(camera_id);
      requestStreamerScale(camera_id);
      requestDetectorThresholds(camera_id);
    }
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
   *     },
   *     ...
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
    oss << "{\"timestamp_ns\":" << this->now().nanoseconds() << ",\"cameras\":[";
    for (size_t i = 0; i < camera_ids_.size(); ++i) {
      const auto &camera_id = camera_ids_[i];
      const auto it = snapshot.find(camera_id);
      CameraInfo info;
      if (it != snapshot.end()) {
        info = it->second;
      }

      oss << "{";
      oss << "\"id\":\"" << camera_id << "\",";
      oss << "\"location\":\"" << escapeJson(info.location) << "\",";
      oss << "\"http_url\":\"" << escapeJson(info.http_url) << "\",";
      oss << "\"resolution\":{\"width\":" << info.width << ",\"height\":" << info.height << ",\"fps\":" << info.fps << "},";
      oss << "\"scale\":" << info.scale << ",";
      oss << "\"confidence_threshold\":" << info.confidence_threshold << ",";
      oss << "\"nms_threshold\":" << info.nms_threshold;
      oss << "}";

      if (i + 1 < camera_ids_.size()) {
        oss << ",";
      }
    }
    oss << "]}";
    return oss.str();
  }

  /**
   * @brief 发布状态信息到 MQTT
   * @param payload JSON 格式的状态数据
   *
   * 将构建好的 JSON payload 发布到 info_topic 指定的 MQTT 主题
   */
  void publishToInfoTopic(const std::string &payload)
  {
    if (client_ == nullptr || !client_->is_connected()) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 3000, "Cannot publish /jetson/info, MQTT not connected");
      return;
    }

    try {
      auto msg = mqtt::make_message(info_topic_, payload, 1, false);
      client_->publish(msg)->wait();
    } catch (const mqtt::exception &e) {
      RCLCPP_ERROR(this->get_logger(), "Publish /jetson/info failed: %s", e.what());
    }
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
    refreshLocalDynamicParameters();  // 检查参数是否有更新
    requestRuntimeInfo();               // 获取最新运行时状态
    const std::string payload = buildInfoPayload();  // 构建 JSON
    publishToInfoTopic(payload);        // 发布到 MQTT
  }

  // ==================== 成员变量 ====================

  std::unique_ptr<mqtt::async_client> client_;  ///< Paho MQTT 异步客户端
  MqttCallback cb_;                              ///< MQTT 消息回调处理

  // MQTT 连接配置
  std::string broker_;            ///< MQTT broker 地址
  int port_{1883};                 ///< MQTT 端口
  std::string client_id_;         ///< 客户端 ID
  std::string subscribe_topic_;   ///< 订阅主题（命令）
  std::string publish_topic_;      ///< 发布主题（状态）

  // 上报配置
  std::string info_topic_;              ///< 信息发布主题
  double report_interval_sec_{1.0};     ///< 上报间隔（秒）
  std::vector<std::string> camera_ids_;           ///< 摄像头 ID 列表
  std::vector<std::string> camera_locations_;     ///< 摄像头位置列表
  std::vector<std::string> camera_http_urls_;    ///< HTTP URL 列表

  // 内部状态
  rclcpp::TimerBase::SharedPtr report_timer_;      ///< 定时上报计时器
  std::unordered_map<std::string, std::shared_ptr<rclcpp::AsyncParametersClient>> param_clients_;  ///< 参数客户端缓存
  std::unordered_map<std::string, CameraInfo> info_cache_;  ///< 摄像头信息缓存
  std::mutex info_mutex_;  ///< 保护 info_cache_ 的互斥锁
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
  rclcpp::spin(std::make_shared<MqttNode>());
  rclcpp::shutdown();
  return 0;
}
