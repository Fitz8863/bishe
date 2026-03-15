# ROS 2 YOLO 安全行为检测系统实施计划

> **面向代理：** 使用 superpowers:subagent-driven-development 或 superpowers:executing-plans 执行此计划。步骤使用 checkbox (`- [ ]`) 语法跟踪。

**目标：** 在 Jetson Orin Nano 上构建 ROS 2 安全行为检测系统，5 节点架构，通过 Topic 通信连接摄像头、YOLO 推理、RTSP 推流、监控判定、MQTT 通信

**架构：** 5 节点 + 1 消息包，Topic 驱动架构

**技术栈：** ROS 2 Humble, C++, OpenCV, GStreamer, curl, paho-mqtt

---

## 目录结构

```
src/
├── bishe_msgs/          # 共享消息定义
│   ├── msg/
│   │   └── DetectorResult.msg
│   ├── CMakeLists.txt
│   └── package.xml
├── bishe_camera/        # 摄像头读取
│   ├── src/
│   │   └── camera_node.cpp
│   ├── CMakeLists.txt
│   └── package.xml
├── bishe_detector/     # YOLO 推理
│   ├── src/
│   │   └── detector_node.cpp
│   ├── CMakeLists.txt
│   └── package.xml
├── bishe_streamer/     # RTSP 推流
│   ├── src/
│   │   └── streamer_node.cpp
│   ├── CMakeLists.txt
│   └── package.xml
├── bishe_monitor/     # 判定 + 抓拍 + 上传 + 警报
│   ├── src/
│   │   └── monitor_node.cpp
│   ├── config/
│   │   └── config.yaml
│   ├── CMakeLists.txt
│   └── package.xml
└── bishe_mqtt/        # MQTT 通信
    ├── src/
    │   └── mqtt_node.cpp
    ├── CMakeLists.txt
    └── package.xml
```

---

## Chunk 1: bishe_msgs 消息包

### Task 1: 创建 bishe_msgs 包

**Files:**
- Create: `src/bishe_msgs/CMakeLists.txt`
- Create: `src/bishe_msgs/package.xml`
- Create: `src/bishe_msgs/msg/DetectorResult.msg`

- [ ] **Step 1: 创建 src/bishe_msgs 目录并创建 package.xml**

```bash
mkdir -p src/bishe_msgs/msg
```

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>bishe_msgs</name>
  <version>0.1.0</version>
  <description>Shared messages for bishe safety detection system</description>
  <maintainer email="user@localhost">user</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>
  <buildtool_depend>rosidl_default_generators</buildtool_depend>
  <depend>sensor_msgs</depend>
  <depend>std_msgs</depend>

  <exec_depend>rosidl_default_runtime</exec_depend>

  <member_of_group>rosidl_interface_packages</member_of_group>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

- [ ] **Step 2: 创建 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.8)
project(bishe_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

# Message generation
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/DetectorResult.msg"
  DEPENDENCIES sensor_msgs std_msgs
)

ament_export_dependencies(rosidl_default_runtime)

ament_package()
```

- [ ] **Step 3: 创建 DetectorResult.msg**

```msg
bool has_violation
float32 confidence
string violation_type
sensor_msgs/Image annotated_image
```

- [ ] **Step 4: 编译验证**

```bash
cd ~/ros2_ws
colcon build --packages-select bishe_msgs
```

Expected: BUILD SUCCESS

- [ ] **Step 5: 提交**

```bash
git add src/bishe_msgs/
git commit -m "feat: create bishe_msgs package with DetectorResult message"
```

---

## Chunk 2: bishe_camera 摄像头节点

### Task 2: 创建 bishe_camera 包

**Files:**
- Create: `src/bishe_camera/package.xml`
- Create: `src/bishe_camera/CMakeLists.txt`
- Create: `src/bishe_camera/src/camera_node.cpp`

- [ ] **Step 1: 创建目录结构**

```bash
mkdir -p src/bishe_camera/src
```

- [ ] **Step 2: 创建 package.xml**

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>bishe_camera</name>
  <version>0.1.0</version>
  <description>Camera node for bishe safety detection</description>
  <maintainer email="user@localhost">user</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>
  <depend>rclcpp</depend>
  <depend>sensor_msgs</depend>
  <depend>image_transport</depend>
  <depend>cv_bridge</depend>
  <depend>opencv4nodejs-prebuilt</depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

- [ ] **Step 3: 创建 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.8)
project(bishe_camera)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(camera_node src/camera_node.cpp)

ament_target_dependencies(camera_node
  rclcpp
  sensor_msgs
  image_transport
  cv_bridge
  OpenCV
)

install(TARGETS camera_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
```

- [ ] **Step 4: 创建 camera_node.cpp**

```cpp
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/image_transport.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>

class CameraNode : public rclcpp::Node
{
public:
  CameraNode()
      : Node("camera_node"), it_(this)
  {
    // Declare parameters
    this->declare_parameter<std::string>("device", "/dev/video0");
    this->declare_parameter<int>("width", 1280);
    this->declare_parameter<int>("height", 720);
    this->declare_parameter<int>("framerate", 60);

    this->get_parameter("device", device_);
    this->get_parameter("width", width_);
    this->get_parameter("height", height_);
    this->get_parameter("framerate", framerate_);

    // Create publisher
    image_pub_ = it_.advertise("camera/image_raw", 10);

    // Initialize camera
    initCamera();

    // Start publishing timer (publish at specified framerate)
    int interval_ms = 1000 / framerate_;
    timer_ = this->create_wall_timer(
        std::chrono::milliseconds(interval_ms),
        std::bind(&CameraNode::publishFrame, this));

    RCLCPP_INFO(this->get_logger(), "Camera node started, publishing to /camera/image_raw");
  }

private:
  void initCamera()
  {
    // GStreamer pipeline for USB camera
    std::string pipeline =
        "v4l2src device=" + device_ + " ! "
        "image/jpeg, width=" + std::to_string(width_) + ", height=" + std::to_string(height_) + ", framerate=" + std::to_string(framerate_) + "/1 ! "
        "nvv4l2decoder mjpegdecode=1 ! nvvidconv ! video/x-raw, format=BGRx ! "
        "videoconvert ! video/x-raw, format=BGR ! "
        "appsink";

    cap_.open(pipeline, cv::CAP_GSTREAMER);
    if (!cap_.isOpened()) {
      RCLCPP_ERROR(this->get_logger(), "Failed to open camera %s", device_.c_str());
      throw std::runtime_error("Failed to open camera");
    }
    RCLCPP_INFO(this->get_logger(), "Camera opened successfully");
  }

  void publishFrame()
  {
    cv::Mat frame;
    if (cap_.read(frame)) {
      if (!frame.empty()) {
        sensor_msgs::msg::Image::SharedPtr msg =
            cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", frame).toImageMsg();
        image_pub_.publish(msg);
      }
    }
  }

  image_transport::ImageTransport it_;
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
```

- [ ] **Step 5: 编译验证**

```bash
cd ~/ros2_ws
colcon build --packages-select bishe_camera
```

Expected: BUILD SUCCESS

- [ ] **Step 6: 提交**

```bash
git add src/bishe_camera/
git commit -m "feat: add bishe_camera node for USB camera capture"
```

---

## Chunk 3: bishe_detector YOLO 推理节点

### Task 3: 创建 bishe_detector 包（预留 YOLO 接口）

**Files:**
- Create: `src/bishe_detector/package.xml`
- Create: `src/bishe_detector/CMakeLists.txt`
- Create: `src/bishe_detector/src/detector_node.cpp`

- [ ] **Step 1: 创建目录结构**

```bash
mkdir -p src/bishe_detector/src
```

- [ ] **Step 2: 创建 package.xml**

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>bishe_detector</name>
  <version>0.1.0</version>
  <description>YOLO detector node for bishe safety detection</description>
  <maintainer email="user@localhost">user</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>
  <depend>rclcpp</depend>
  <depend>sensor_msgs</depend>
  <depend>image_transport</depend>
  <depend>cv_bridge</depend>
  <depend>bishe_msgs</depend>
  <depend>OpenCV</depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

- [ ] **Step 3: 创建 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.8)
project(bishe_detector)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

# Find bishe_msgs
find_package(bishe_msgs REQUIRED)

include_directories(include)

add_executable(detector_node src/detector_node.cpp)

ament_target_dependencies(detector_node
  rclcpp
  sensor_msgs
  image_transport
  cv_bridge
  OpenCV
  bishe_msgs
)

# 链接 bishe_msgs
ament_target_dependencies(detector_node "bishe_msgs")

install(TARGETS detector_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
```

- [ ] **Step 4: 创建 detector_node.cpp（预留 YOLO 接口）**

```cpp
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/image_transport.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>
#include "bishe_msgs/msg/detector_result.hpp"

class DetectorNode : public rclcpp::Node
{
public:
  DetectorNode()
      : Node("detector_node"), it_(this)
  {
    // Declare parameters
    this->declare_parameter<float>("confidence_threshold", 0.5);
    this->get_parameter("confidence_threshold", confidence_threshold_);

    // Subscribe to camera images
    image_sub_ = it_.subscribe("camera/image_raw", 10,
        std::bind(&DetectorNode::imageCallback, this, std::placeholders::_1));

    // Publisher for detection results
    result_pub_ = this->create_publisher<bishe_msgs::msg::DetectorResult>("detector/result", 10);

    RCLCPP_INFO(this->get_logger(), "Detector node started, threshold: %.2f", confidence_threshold_);

    // ============================================================
    // TODO: 在此处初始化你的 TensorRT YOLO 模型
    // 例如: loadYoloModel("/path/to/yolov8n.engine");
    // ============================================================
  }

private:
  void imageCallback(const sensor_msgs::msg::Image::ConstSharedPtr &msg)
  {
    try {
      cv::Mat frame = cv_bridge::toCvShare(msg, "bgr8")->image;

      // ============================================================
      // TODO: 调用你的 YOLO 推理函数
      // std::vector<Detection> results = runYoloInference(frame);
      // ============================================================

      // 临时示例：假设没有检测到违规
      // 替换为你的实际 YOLO 推理结果
      bool has_violation = false;
      float confidence = 0.0f;
      std::string violation_type = "";
      cv::Mat annotated_frame = frame.clone();

      // 在这里填充你的 YOLO 推理结果
      // has_violation = checkViolation(results);
      // confidence = getMaxConfidence(results);
      // violation_type = getViolationType(results);
      // annotated_frame = drawDetections(frame, results);

      // Publish result
      auto result = bishe_msgs::msg::DetectorResult();
      result.has_violation = has_violation;
      result.confidence = confidence;
      result.violation_type = violation_type;
      result.annotated_image = *cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", annotated_frame).toImageMsg();

      result_pub_->publish(result);

    } catch (const cv_bridge::Exception &e) {
      RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
    }
  }

  image_transport::ImageTransport it_;
  image_transport::ImageSubscriber image_sub_;
  rclcpp::Publisher<bishe_msgs::msg::DetectorResult>::SharedPtr result_pub_;
  float confidence_threshold_;

  // ============================================================
  // TODO: 添加你的 YOLO 推理相关函数和成员变量
  // ============================================================

  // 示例函数签名（你需要实现）:
  // void loadYoloModel(const std::string& model_path);
  // std::vector<Detection> runYoloInference(const cv::Mat& frame);
  // bool checkViolation(const std::vector<Detection>& results);
  // float getMaxConfidence(const std::vector<Detection>& results);
  // std::string getViolationType(const std::vector<Detection>& results);
  // cv::Mat drawDetections(const cv::Mat& frame, const std::vector<Detection>& results);
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<DetectorNode>());
  rclcpp::shutdown();
  return 0;
}
```

- [ ] **Step 5: 编译验证**

```bash
cd ~/ros2_ws
colcon build --packages-select bishe_detector
```

Expected: BUILD SUCCESS（会有 TODO 警告，这是正常的）

- [ ] **Step 6: 提交**

```bash
git add src/bishe_detector/
git commit -m "feat: add bishe_detector node with YOLO inference placeholder"
```

---

## Chunk 4: bishe_streamer RTSP 推流节点

### Task 4: 创建 bishe_streamer 包

**Files:**
- Create: `src/bishe_streamer/package.xml`
- Create: `src/bishe_streamer/CMakeLists.txt`
- Create: `src/bishe_streamer/src/streamer_node.cpp`

- [ ] **Step 1: 创建目录结构**

```bash
mkdir -p src/bishe_streamer/src
```

- [ ] **Step 2: 创建 package.xml**

```xml
<?xml version="1.0"?>
<?xml_model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>bishe_streamer</name>
  <version>0.1.0</version>
  <description>Streamer node for RTSP push</description>
  <maintainer email="user@localhost">user</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>
  <depend>rclcpp</depend>
  <depend>sensor_msgs</depend>
  <depend>image_transport</depend>
  <depend>cv_bridge</depend>
  <depend>OpenCV</depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

- [ ] **Step 3: 创建 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.8)
project(bishe_streamer)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(streamer_node src/streamer_node.cpp)

ament_target_dependencies(streamer_node
  rclcpp
  sensor_msgs
  image_transport
  cv_bridge
  OpenCV
)

install(TARGETS streamer_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
```

- [ ] **Step 4: 创建 streamer_node.cpp（集成现有 push_video.cc）**

```cpp
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/image_transport.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>

class StreamerNode : public rclcpp::Node
{
public:
  StreamerNode()
      : Node("streamer_node"), it_(this)
  {
    // Declare parameters
    this->declare_parameter<std::string>("rtsp_url", "rtsp://localhost:8554/stream");
    this->declare_parameter<float>("scale", 0.9);
    this->get_parameter("rtsp_url", rtsp_url_);
    this->get_parameter("scale", scale_);

    // Subscribe to camera images
    image_sub_ = it_.subscribe("camera/image_raw", 1,
        std::bind(&StreamerNode::imageCallback, this, std::placeholders::_1));

    // Initialize video writer in constructor
    initWriter();

    RCLCPP_INFO(this->get_logger(), "Streamer node started, pushing to %s", rtsp_url_.c_str());
  }

  ~StreamerNode()
  {
    if (writer_.isOpened()) {
      writer_.release();
    }
  }

private:
  void initWriter()
  {
    // Get frame size from parameter or use default
    int width = 1280;
    int height = 720;
    double fps = 60.0;

    int target_width = static_cast<int>(width * scale_);
    int target_height = static_cast<int>(height * scale_);
    cv::Size target_size(target_width, target_height);

    // RTSP push pipeline (without audio, video only)
    // 与 push_video.cc 中的推流逻辑一致
    std::string rtsp_out =
        "appsrc ! videoconvert ! video/x-raw,format=I420 ! "
        "x264enc bitrate=8000 speed-preset=ultrafast tune=zerolatency ! "
        "h264parse ! queue ! rtph264pay name=pay0 pt=96 ! "
        "udpsink host=127.0.0.1 port=8554";

    // 使用 VLC 可播放的 RTSP 服务器管道
    // 或者使用 rtspclientsink（需要 RTSP 服务器）
    std::string rtsp_clientsink =
        "appsrc ! videoconvert ! video/x-raw,format=I420 ! "
        "x264enc bitrate=8000 speed-preset=ultrafast tune=zerolatency ! "
        "h264parse ! queue ! sink. "
        "rtspclientsink location=" + rtsp_url_;

    writer_.open(rtsp_clientsink, cv::CAP_GSTREAMER, 0, fps, target_size, true);

    if (!writer_.isOpened()) {
      RCLCPP_WARN(this->get_logger(), "Failed to open RTSP writer, will retry on first frame");
    } else {
      RCLCPP_INFO(this->get_logger(), "RTSP writer opened successfully");
    }
  }

  void imageCallback(const sensor_msgs::msg::Image::ConstSharedPtr &msg)
  {
    try {
      cv::Mat frame = cv_bridge::toCvShare(msg, "bgr8")->image;

      // Resize if needed
      int target_width = static_cast<int>(frame.cols * scale_);
      int target_height = static_cast<int>(frame.rows * scale_);
      cv::Mat resized;
      cv::resize(frame, resized, cv::Size(target_width, target_height));

      // Write to RTSP stream
      if (writer_.isOpened()) {
        writer_.write(resized);
      } else {
        // Retry opening writer if not opened
        initWriter();
      }

    } catch (const cv_bridge::Exception &e) {
      RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
    }
  }

  image_transport::ImageTransport it_;
  image_transport::ImageSubscriber image_sub_;
  cv::VideoWriter writer_;
  std::string rtsp_url_;
  float scale_;
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<StreamerNode>());
  rclcpp::shutdown();
  return 0;
}
```

- [ ] **Step 5: 编译验证**

```bash
cd ~/ros2_ws
colcon build --packages-select bishe_streamer
```

Expected: BUILD SUCCESS

- [ ] **Step 6: 提交**

```bash
git add src/bishe_streamer/
git commit -m "feat: add bishe_streamer node for RTSP streaming"
```

---

## Chunk 5: bishe_monitor 监控节点（判定+抓拍+上传+警报）

### Task 5: 创建 bishe_monitor 包

**Files:**
- Create: `src/bishe_monitor/package.xml`
- Create: `src/bishe_monitor/CMakeLists.txt`
- Create: `src/bishe_monitor/src/monitor_node.cpp`
- Create: `src/bishe_monitor/config/config.yaml`

- [ ] **Step 1: 创建目录结构**

```bash
mkdir -p src/bishe_monitor/src src/bishe_monitor/config
```

- [ ] **Step 2: 创建 package.xml**

```xml
<?xml version="1.0"?>
<?xml_model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>bishe_monitor</name>
  <version>0.1.0</version>
  <description>Monitor node for violation detection, capture and upload</description>
  <maintainer email="user@localhost">user</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>
  <depend>rclcpp</depend>
  <depend>bishe_msgs</depend>
  <depend>cv_bridge</depend>
  <depend>OpenCV</depend>
  <depend>curl</depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

- [ ] **Step 3: 创建 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.8)
project(bishe_monitor)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(bishe_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

# Find libcurl
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

include_directories(${CURL_INCLUDE_DIRS})

add_executable(monitor_node src/monitor_node.cpp)

ament_target_dependencies(monitor_node
  rclcpp
  bishe_msgs
  cv_bridge
  OpenCV
  CURL
)

install(TARGETS monitor_node
  DESTINATION lib/${PROJECT_NAME}
)

install(FILES config/config.yaml
  DESTINATION share/${PROJECT_NAME}/config
)

ament_package()
```

- [ ] **Step 4: 创建 config.yaml**

```yaml
camera:
  device: "/dev/video0"
  width: 1280
  height: 720
  framerate: 60

detector:
  confidence_threshold: 0.5

monitor:
  window_seconds: 5
  violation_ratio_threshold: 0.4
  location: "生产车间A区"
  camera_id: "001"

upload:
  server_url: "http://YOUR_SERVER_IP:5000/capture/upload"

alarm:
  audio_file: "/path/to/alarm.mp3"
```

- [ ] **Step 5: 创建 monitor_node.cpp**

```cpp
#include <rclcpp/rclcpp.hpp>
#include <bishe_msgs/msg/detector_result.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>
#include <curl/curl.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <deque>

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
    } else if (is_alarming_ && msg->has_violation) {
      // Count violation frames within window
      violation_count_++;
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
```

- [ ] **Step 6: 编译验证**

```bash
cd ~/ros2_ws
colcon build --packages-select bishe_monitor
```

Expected: BUILD SUCCESS

- [ ] **Step 7: 提交**

```bash
git add src/bishe_monitor/
git commit -m "feat: add bishe_monitor node with violation detection, capture and upload"
```

---

## Chunk 6: bishe_mqtt MQTT 通信节点

### Task 6: 创建 bishe_mqtt 包

**Files:**
- Create: `src/bishe_mqtt/package.xml`
- Create: `src/bishe_mqtt/CMakeLists.txt`
- Create: `src/bishe_mqtt/src/mqtt_node.cpp`

- [ ] **Step 1: 创建目录结构**

```bash
mkdir -p src/bishe_mqtt/src
```

- [ ] **Step 2: 创建 package.xml**

```xml
<?xml version="1.0"?>
<?xml_model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>bishe_mqtt</name>
  <version>0.1.0</version>
  <description>MQTT communication node</description>
  <maintainer email="user@localhost">user</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>
  <depend>rclcpp</depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

- [ ] **Step 3: 创建 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.8)
project(bishe_mqtt)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

# Find Paho MQTT
find_package(PkgConfig REQUIRED)
pkg_check_modules(PAHO_MQTT_CPP REQUIRED paho-mqtt3cpp)

include_directories(${PAHO_MQTT_CPP_INCLUDE_DIRS})

add_executable(mqtt_node src/mqtt_node.cpp)

target_link_libraries(mqtt_node ${PAHO_MQTT_CPP_LIBRARIES})

ament_target_dependencies(mqtt_node
  rclcpp
  PAHO_MQTT_CPP
)

install(TARGETS mqtt_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
```

- [ ] **Step 4: 创建 mqtt_node.cpp**

```cpp
#include <rclcpp/rclcpp.hpp>
#include <mqtt/client.h>
#include <string>
#include <iostream>

class MqttNode : public rclcpp::Node
{
public:
  MqttNode()
      : Node("mqtt_node")
  {
    // Declare parameters
    this->declare_parameter<std::string>("broker", "localhost");
    this->declare_parameter<int>("port", 1883);
    this->declare_parameter<std::string>("client_id", "bishe_camera_001");
    this->declare_parameter<std::string>("subscribe_topic", "factory/camera/001/command");
    this->declare_parameter<std::string>("publish_topic", "factory/camera/001/status");

    this->get_parameter("broker", broker_);
    this->get_parameter("port", port_);
    this->get_parameter("client_id", client_id_);
    this->get_parameter("subscribe_topic", subscribe_topic_);
    this->get_parameter("publish_topic", publish_topic_);

    // Initialize MQTT client
    std::string server_uri = "tcp://" + broker_ + ":" + std::to_string(port_);
    client_ = std::make_unique<mqtt::client>(server_uri, client_id_);

    // Set callback
    client_->set_callback(*this);

    // Connect
    try {
      mqtt::connect_options conn_opts;
      conn_opts.set_keep_alive_interval(20);
      conn_opts.set_clean_session(true);
      client_->connect(conn_opts);
      RCLCPP_INFO(this->get_logger(), "Connected to MQTT broker: %s", server_uri.c_str());

      // Subscribe to command topic
      client_->subscribe(subscribe_topic_, 0);
      RCLCPP_INFO(this->get_logger(), "Subscribed to: %s", subscribe_topic_.c_str());

    } catch (const mqtt::exception &e) {
      RCLCPP_ERROR(this->get_logger(), "MQTT connection failed: %s", e.what());
    }

    // Start message processing timer
    timer_ = this->create_wall_timer(
        std::chrono::milliseconds(100),
        std::bind(&MqttNode::processMessages, this));

    RCLCPP_INFO(this->get_logger(), "MQTT node started");
  }

  ~MqttNode()
  {
    try {
      client_->disconnect();
    } catch (const mqtt::exception &e) {
      RCLCPP_ERROR(this->get_logger(), "MQTT disconnect error: %s", e.what());
    }
  }

private:
  void processMessages()
  {
    // Process incoming messages
    // Note: actual message delivery is via callback
  }

  // MQTT callback interface implementation
  void message_arrived(mqtt::const_message_ptr msg) override
  {
    std::string topic = msg->get_topic();
    std::string payload = msg->to_string();

    RCLCPP_INFO(this->get_logger(), "Received on %s: %s", topic.c_str(), payload.c_str());

    // Parse JSON command (simple parsing)
    // Example: {"action": "start"} or {"action": "stop"}
    handleCommand(payload);
  }

  void handleCommand(const std::string& json_payload)
  {
    // Simple JSON parsing (could use nlohmann::json for full JSON support)
    if (json_payload.find("\"action\":\"start\"") != std::string::npos ||
        json_payload.find("\"action\": \"start\"") != std::string::npos) {
      RCLCPP_INFO(this->get_logger(), "Command: START");
      // TODO: Implement start action
    } else if (json_payload.find("\"action\":\"stop\"") != std::string::npos ||
               json_payload.find("\"action\": \"stop\"") != std::string::npos) {
      RCLCPP_INFO(this->get_logger(), "Command: STOP");
      // TODO: Implement stop action
    } else {
      RCLCPP_WARN(this->get_logger(), "Unknown command: %s", json_payload.c_str());
    }
  }

  //预留的发布函数
  void publishStatus(const std::string& status)
  {
    if (client_->is_connected()) {
      try {
        auto msg = mqtt::make_message(publish_topic_, status);
        client_->publish(msg);
        RCLCPP_INFO(this->get_logger(), "Published to %s: %s", publish_topic_.c_str(), status.c_str());
      } catch (const mqtt::exception &e) {
        RCLCPP_ERROR(this->get_logger(), "Publish failed: %s", e.what());
      }
    } else {
      RCLCPP_WARN(this->get_logger(), "Cannot publish, not connected");
    }
  }

  // 预留：发布抓拍结果
  void publishCaptureResult(const std::string& capture_id, const std::string& violation_type)
  {
    std::string payload = "{\"capture_id\":\"" + capture_id + "\",\"violation_type\":\"" + violation_type + "\"}";
    publishStatus(payload);
  }

  // 预留：发布系统状态
  void publishSystemStatus(const std::string& status, int violation_count)
  {
    std::string payload = "{\"status\":\"" + status + "\",\"violations\":" + std::to_string(violation_count) + "}";
    publishStatus(payload);
  }

  std::unique_ptr<mqtt::client> client_;
  std::string broker_;
  int port_;
  std::string client_id_;
  std::string subscribe_topic_;
  std::string publish_topic_;
  rclcpp::TimerBase::SharedPtr timer_;
};

// Callback class for MQTT
class MqttCallback : public mqtt::callback
{
  void message_arrived(mqtt::const_message_ptr msg) override
  {
    // This will be called by the client
  }
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MqttNode>());
  rclcpp::shutdown();
  return 0;
}
```

- [ ] **Step 5: 编译验证**

```bash
cd ~/ros2_ws
colcon build --packages-select bishe_mqtt
```

Expected: BUILD SUCCESS

- [ ] **Step 6: 提交**

```bash
git add src/bishe_mqtt/
git commit -m "feat: add bishe_mqtt node for MQTT communication"
```

---

## Chunk 7: Launch 启动文件

### Task 7: 创建 launch 启动文件

**Files:**
- Create: `src/bishe_launch/launch/bishe.launch.py`

- [ ] **Step 1: 创建 launch 包**

```bash
mkdir -p src/bishe_launch/launch
```

- [ ] **Step 2: 创建 package.xml**

```xml
<?xml version="1.0"?>
<?xml_model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>bishe_launch</name>
  <version>0.1.0</version>
  <description>Launch files for bishe safety detection system</description>
  <maintainer email="user@localhost">user</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_python</buildtool_depend>

  <exec_depend>ros2launch</exec_depend>

  <export>
    <build_type>ament_python</build_type>
  </export>
</package>
```

- [ ] **Step 3: 创建 setup.py**

```python
from setuptools import setup

package_name = 'bishe_launch'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name + '/launch',
            ['launch/bishe.launch.py']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='user',
    maintainer_email='user@localhost',
    description='Launch files for bishe safety detection system',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'bishe_launch = bishe_launch:main',
        ],
    },
)
```

- [ ] **Step 4: 创建 bishe.launch.py**

```python
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        # Camera node
        Node(
            package='bishe_camera',
            executable='camera_node',
            name='camera_node',
            parameters=[{
                'device': '/dev/video0',
                'width': 1280,
                'height': 720,
                'framerate': 60,
            }],
            output='screen',
        ),

        # Detector node
        Node(
            package='bishe_detector',
            executable='detector_node',
            name='detector_node',
            parameters=[{
                'confidence_threshold': 0.5,
            }],
            output='screen',
        ),

        # Streamer node
        Node(
            package='bishe_streamer',
            executable='streamer_node',
            name='streamer_node',
            parameters=[{
                'rtsp_url': 'rtsp://localhost:8554/stream',
                'scale': 0.9,
            }],
            output='screen',
        ),

        # Monitor node
        Node(
            package='bishe_monitor',
            executable='monitor_node',
            name='monitor_node',
            parameters=[{
                'window_seconds': 5,
                'violation_ratio_threshold': 0.4,
                'location': '生产车间A区',
                'camera_id': '001',
                'upload.server_url': 'http://YOUR_SERVER_IP:5000/capture/upload',
                'alarm.audio_file': '/path/to/alarm.mp3',
            }],
            output='screen',
        ),

        # MQTT node
        Node(
            package='bishe_mqtt',
            executable='mqtt_node',
            name='mqtt_node',
            parameters=[{
                'broker': 'YOUR_MQTT_BROKER_IP',
                'port': 1883,
                'client_id': 'bishe_camera_001',
                'subscribe_topic': 'factory/camera/001/command',
                'publish_topic': 'factory/camera_001/status',
            }],
            output='screen',
        ),
    ])
```

- [ ] **Step 5: 创建资源文件**

```bash
mkdir -p src/bishe_launch/resource/bishe_launch
touch src/bishe_launch/resource/bishe_launch/ros2
```

- [ ] **Step 6: 编译验证**

```bash
cd ~/ros2_ws
colcon build --packages-select bishe_launch
```

Expected: BUILD SUCCESS

- [ ] **Step 7: 提交**

```bash
git add src/bishe_launch/
git commit -m "feat: add bishe_launch package with launch file"
```

---

## 总结

完成以上所有步骤后，你可以：

1. **编译整个工作区：**
```bash
cd ~/ros2_ws
colcon build
```

2. **运行所有节点：**
```bash
cd ~/ros2_ws
source install/setup.bash
ros2 launch bishe_launch bishe.launch.py
```

3. **单独运行某个节点测试：**
```bash
source install/setup.bash
ros2 run bishe_camera camera_node
```

4. **查看 topic：**
```bash
ros2 topic list
ros2 topic echo /detector/result
```
