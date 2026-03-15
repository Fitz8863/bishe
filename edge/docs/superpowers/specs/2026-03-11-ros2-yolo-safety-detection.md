# ROS 2 YOLO 安全行为检测系统设计

> **面向代理：** 实现前请使用 superpowers:writing-plans 生成详细实施计划

**目标：** 在 Jetson Orin Nano 上构建 ROS 2 安全行为检测系统，使用 TensorRT 推理 YOLOv8，检测到不安全行为后抓拍并上传到服务器

**架构：** 5 节点 + 1 消息包，通过 Topic 通信

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           ROS 2 Workspace                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────────┐    /camera/image_raw    ┌──────────────────┐      │
│  │ camera_node  │ ────────────────────────>│  detector_node   │      │
│  │ (读取摄像头) │                           │  (YOLO推理预留)  │      │
│  └──────────────┘                           └────────┬─────────┘      │
│                                                       │                 │
│                                                       │ /detector/result│
│                                                       ▼                 │
│  ┌──────────────┐    /camera/image_raw            ┌──────────────┐   │
│  │ streamer_node│<───────────────────────────────│ monitor_node  │   │
│  │ (RTSP推流)   │                                 │(判定/抓拍/警报)│   │
│  └──────────────┘                                 └───────┬────────┘   │
│                                                            │            │
│                              MQTT ◄─────────────────────────┤            │
│                              (订阅/发布)                      │            │
│                                                            │ HTTP POST  │
│                                                            ▼            │
│                                                    ┌──────────────┐    │
│                                                    │ Web Server   │    │
│                                                    │ (上传图片)    │    │
│                                                    └──────────────┘    │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 节点设计

### 1. bishe_camera（摄像头节点）

- **功能：** 读取 USB 摄像头视频
- **输入：** 无
- **输出：** `/camera/image_raw` (sensor_msgs/Image)
- **实现：** 使用 V4L2 + GStreamer 管道

### 2. bishe_detector（推理节点）

- **功能：** TensorRT YOLO 推理
- **输入：** `/camera/image_raw`
- **输出：** `/detector/result` (bishe_msgs/DetectorResult)
- **预留：** `run_yolo_inference()` 函数接口，用户自行填充

### 3. bishe_streamer（推流节点）

- **功能：** RTSP 推流
- **输入：** `/camera/image_raw`
- **输出：** RTSP 流到 `rtsp://localhost:8554/stream`
- **实现：** 集成现有 push_video.cc 的 GStreamer 逻辑

### 4. bishe_monitor（监控节点）

- **功能：** 阈值判定、抓拍上传、警报播放
- **输入：** `/detector/result`
- **输出：** HTTP 上传、音频播放
- **判定逻辑：**
  1. 收到 `has_violation == true` → 立即播放警报（循环）
  2. 开始 5 秒滑动窗口计时
  3. 统计窗口内总帧数和违规帧数
  4. ratio >= 0.4 → 抓拍 1 张 → HTTP POST 上传 → 停止警报
  5. ratio < 0.4 → 停止警报

### 5. bishe_mqtt（MQTT节点）

- **功能：** MQTT 通信
- **订阅：** `factory/camera/001/command`
- **发布（预留）：** `factory/camera/001/status`
- **消息格式：** JSON

---

## 消息定义

### DetectorResult.msg

```msg
bool has_violation
float32 confidence
string violation_type
sensor_msgs/Image annotated_image
```

---

## 配置文件（config.yaml）

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

mqtt:
  broker: "YOUR_MQTT_BROKER_IP"
  port: 1883
  subscribe_topic: "factory/camera/001/command"
  publish_topic: "factory/camera/001/status"

alarm:
  audio_file: "/path/to/alarm.mp3"
```

---

## 依赖库

| 包 | 依赖 |
|----|------|
| bishe_camera | OpenCV, image_transport |
| bishe_detector | OpenCV, TensorRT (用户自行添加) |
| bishe_streamer | OpenCV, GStreamer |
| bishe_monitor | curl, OpenCV, GStreamer (音频) |
| bishe_mqtt | paho-mqtt |

---

## 实现顺序

1. bishe_msgs（消息定义）
2. bishe_camera
3. bishe_detector
4. bishe_streamer
5. bishe_monitor
6. bishe_mqtt
7. config.yaml
8. launch 文件
