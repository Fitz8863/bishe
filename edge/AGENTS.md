# 🤖 Agent Guidelines for `edge` Workspace (ROS 2 Humble)

Edge-side detection system on Jetson Orin Nano for Chemical Plant Hazard Detection.

## 🏗️ Architecture

### 7 ROS 2 Packages

| Package | Type | Role |
|---------|------|------|
| `bishe_msgs` | `rosidl` (C) | `.msg` definitions + `SharedFrameRing` (POSIX shared memory ring buffer). **Rebuild first after any `.msg` change.** |
| `bishe_camera` | `ament_cmake` (C++17) | GStreamer v4l2src → shared memory → `SharedFrameRef` metadata topic. Supports `source_type: "v4l2"` (camera) or `"file"` (MP4 video). |
| `bishe_detector` | `ament_cmake` (C++17) | TensorRT YOLOv8 inference. Worker thread pool + `DetectionGate` (sampling/lock mode). **Auto CLAHE**: detects over/under exposure in `PreprocessImage()` and applies adaptive contrast enhancement before inference. |
| `bishe_monitor` | `ament_cmake` (C++17) | Sliding-window alarm logic, HTTP image upload (curl), gst-play-1.0 audio alarm |
| `bishe_mqtt` | `ament_cmake` (C++17) | Paho MQTT async client. Bridges ROS ↔ MQTT. Dynamic parameter polling via `AsyncParametersClient` |
| `bishe_streamer` | `ament_cmake` (C++17) | GStreamer RTSP push via `VideoWriter` + ffplay intercom child process |
| `bishe_launch` | `ament_python` | `multicamera.launch.py` + `multicamera_config.py`. Uses `OpaqueFunction` + `GroupAction`. Supports `compose_camera_detector` flag for composable nodes. |

### ⚡ Inter-Node Communication (ROS Topics)

```
camera_node ──camera/detector_frame_ref──→ detector_node    [SharedFrameRef via shared memory]
camera_node ──camera/image_raw───────────→ (ROS image topic)
detector_node ──detector/result──────────→ monitor_node, streamer_node   [DetectorResult]
monitor_node ──/alarm/event──────────────→ mqtt_node   [String JSON]
mqtt_node ──intercom/control────────────→ intercom_node   [String: "STOP" or RTSP URL]
```

### 🔄 MQTT Bridging (mqtt_node)

| Direction | ROS Topic | MQTT Topic | Format |
|-----------|-----------|------------|--------|
| ROS → MQTT | `/alarm/event` | `jetson/alarm` | JSON `{"camera_id","location","alarm_type","timestamp_ns"}` |
| ROS → MQTT | (periodic) | `jetson/info` | JSON with camera params (resolution, thresholds, fps) |
| MQTT → ROS | — | `jetson/camera/command` | Config update → ROS2 parameter change |
| MQTT → ROS | — | `jetson/call/command` | Intercom control → `intercom/control` ROS topic |

### 🧵 Node Thread Models

- **DetectorNode**: `std::thread` pool (worker_threads param) + mutex/condvar work queue. NOT `rclcpp::TimerBase`.
- **CameraNode**: `rclcpp::TimerBase` for periodic frame capture.
- **MonitorNode**: Callback-driven from `DetectorResult` subscription. Uses `std::system()` for gst-play-1.0 (fire-and-forget).
- **StreamerNode**: Custom `MutuallyExclusive` callback group for result subscription.
- **IntercomNode**: Manages `ffplay` child process via `fork/exec`, tracks PID for `kill()` on STOP.
- **MqttNode**: Paho async client (separate I/O thread) + ROS2 `MultiThreadedExecutor` for parameter polling.

## 🛠️ Build Commands

```bash
# Always from /home/jetson/projects/bishe/edge
source /opt/ros/humble/setup.bash

# Build order matters: bishe_msgs first
colcon build --packages-select bishe_msgs --symlink-install
colcon build --symlink-install

# Fast dev cycle (single package)
colcon build --packages-select bishe_monitor --symlink-install

# With compile_commands.json (for clangd/lsp)
colcon build --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Source after every build
source install/setup.bash
```

## 🧪 Tests

```bash
# All
colcon test

# Single package GTest
colcon test --packages-select bishe_monitor

# View test results
colcon test-result --verbose
```

**GTest packages**: `bishe_detector` (2 tests: `test_detection_gate`, `test_shared_frame_ring`), `bishe_monitor` (4 tests: `test_alarm_event_payload`, `test_alarm_playback_command`, `test_alarm_upload_gate`, `test_async_task_worker`)
**Pytest packages**: `bishe_launch` (1 test: `test_multicamera_config`)
**Empty test dir**: `bishe_streamer/test/` (no tests written)

## 📦 Key Dependencies

| Dependency | Usage | Config Source |
|------------|-------|---------------|
| TensorRT 10.7 | YOLO inference engine | **Hardcoded** `TensorRT_DIR` in `bishe_detector/CMakeLists.txt:10` as `/home/ad/TensorRT-10.7.0.23/` |
| OpenCV (CUDA) | Frame capture, preprocessing, encoding | `find_package(OpenCV)` |
| Paho MQTT C++ | `mqtt::async_client` | `paho-mqtt3as paho-mqttpp3` (note: `paho-mqtt3as` = async + SSL) |
| libcurl | HTTP multipart image upload in `bishe_monitor` | `pkg_check_modules(CURL libcurl)` |
| jsoncpp | JSON in `bishe_mqtt` + `bishe_monitor` | `pkg_check_modules(JSONCPP jsoncpp)` |
| GStreamer | RTSP push via `VideoWriter("appsrc ! ...")` + v4l2src capture | Runtime piped string |
| FFmpeg | `ffplay` for intercom audio pull | Spawned as child process |

## ⚠️ Environment Quirks

- **TensorRT path**: Hardcoded `/home/ad/TensorRT-10.7.0.23/` in `bishe_detector/CMakeLists.txt`. Override via `-DTensorRT_DIR=<path>`.
- **Hardcoded include dir**: `bishe_streamer/CMakeLists.txt:18` has `include_directories(/opt/ros/humble/include)`.
- **MQTT broker**: Default `100.127.154.73:1883` in launch file. Override via launch arg or `ros2 param set`.
- **Upload server**: Default `http://100.82.58.128:5001/capture/upload` in `cameras.yaml`.
- **Shared memory**: POSIX `/dev/shm` — `SharedFrameRing` names must start with `/`. Uses `shm_open` + `mmap`. **Must be cleaned up on crash** (call `SharedFrameRing::unlink()`).
- **Model files**: Located at `edge/models/` — both `yolov8s.engine` and `best.engine` + `best.onnx`.
- **Alarm audio**: Played via **external** `gst-play-1.0` process (`std::system()`), not a library. Files at `assets/` directory (configurable in `cameras.yaml`).
- **Intercom audio**: Played via external `ffplay` child process, tracked by PID.

## 📝 Code & Debug

### Launch
```bash
# Full system (1 camera)
ros2 launch bishe_launch multicamera.launch.py

# Without monitor (debug only)
ros2 launch bishe_launch multicamera.launch.py enable_monitor:=false

# With composable nodes (intra-process)
ros2 launch bishe_launch multicamera.launch.py compose_camera_detector:=true
```

### Debug Topics
```bash
ros2 topic echo /camera_001/detector/result
ros2 topic pub /alarm/event std_msgs/msg/String "{data: 'test alarm'}"
ros2 param set /camera_001/monitor_node trigger_frame_threshold 5
```

### Config
- Camera pipeline: `edge/src/bishe_launch/config/cameras.yaml`
- Monitor standalone defaults: `edge/src/bishe_monitor/config/config.yaml` (fallback, overridden by launch)

### Code Style
- **Keep existing**: 2-space indent, braces on new line for class/func, same line for control flow
- **Naming**: `CamelCase` classes, `snake_case` vars/funcs, trailing `_` for members, `k` prefix for constants
- **Thread safety**: ALL background threads must be joined in destructors; `DetectorNode` is the reference pattern
- **Logging**: `RCLCPP_INFO/ERROR` — never `std::cout` except in TensorRT tensor dumps
- **Comments**: Chinese strongly encouraged for business logic
