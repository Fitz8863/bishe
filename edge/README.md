# 化工厂危险行为检测系统 - 边缘端 (Edge)

本项目是基于 **ROS 2 Humble** 开发的工业安全监控系统边缘端，专门适配 **NVIDIA Jetson Orin Nano** 平台。通过深度集成 TensorRT 加速的 YOLOv8 模型，实现对化工厂内危险行为（如未戴安全帽、违规吸烟、明火等）的实时检测与告警。

## 🚀 核心功能

- **实时 AI 检测**：基于 TensorRT 10.7 加速的 YOLOv8 推理，支持多路摄像头并发。
- **智能告警判定**：采用滑动窗口算法 (Window-based Logic) 过滤误报，支持自定义判定阈值。
- **多媒体通信**：
  - **RTSP 推流**：将标注后的检测画面实时推送至后端流媒体服务器。
  - **远程对讲 (Intercom)**：支持从 Web 后端发起语音指令，通过 USB 扬声器实时播放。
  - **抓拍上传**：告警瞬间自动抓拍高清图像并通过 HTTP POST 上传至管理后台。
- **云端同步**：通过 MQTT 与 Web 后端保持实时状态同步与指令下发。

## 🏗️ 系统架构

系统由多个相互协作的 ROS 2 节点组成：

1.  **bishe_camera**: 负责 GStreamer 视频流采集。
2.  **bishe_detector**: 执行 AI 推理。使用**共享内存 (Shared Memory)** 技术实现与相机节点的零拷贝图像传输。
3.  **bishe_monitor**: 行为判定中枢，负责逻辑过滤、声音报警及图片上传。
4.  **bishe_mqtt**: 桥接本地 ROS 主题与外部 MQTT Broker，实现状态上报与参数控制。
5.  **bishe_streamer**: 负责标注画面的 H.264 编码与 RTSP 推流。
6.  **bishe_launch**: 包含系统的集成启动脚本与配置文件。

## 🛠️ 环境要求

- **硬件**: NVIDIA Jetson Orin Nano (或更高版本)
- **操作系统**: Ubuntu 22.04 LTS
- **中间件**: ROS 2 Humble
- **关键库**:
  - TensorRT 10.7.x (硬编码路径: `/home/ad/TensorRT-10.7.0.23/`)
  - OpenCV (需 CUDA 支持)
  - FFmpeg (用于 ffplay 远程对讲)
  - Paho MQTT C++
  - libcurl & jsoncpp

## 📦 安装与构建

```bash
# 进入工作区根目录
cd /home/jetson/projects/bishe/edge

# 1. 优先构建消息定义包
colcon build --packages-select bishe_msgs --symlink-install

# 2. 构建其余所有包
colcon build --symlink-install

# 3. 刷新环境
source install/setup.bash
```

## 🚦 运行指南

### 1. 启动完整监控系统
使用默认配置文件（含检测、推流、监控与 MQTT）：
```bash
ros2 launch bishe_launch multicamera.launch.py
```

### 2. 常用调试命令
- **查看实时检测状态**: `ros2 topic echo /camera_001/detector/result`
- **手动触发报警消息**: `ros2 topic pub /alarm/event std_msgs/msg/String "{data: 'test alarm'}"`

## ⚙️ 参数配置 (`cameras.yaml`)

核心配置文件位于 `src/bishe_launch/config/cameras.yaml`：

| 参数 | 含义 | 建议值 |
| :--- | :--- | :--- |
| `sampling_interval_ms` | AI 推理间隔（毫秒）。控制检测频率。 | 200 (高频) / 1000 (省电) |
| `window_seconds` | 告警判定时间窗口（秒）。 | 5 - 10 |
| `trigger_frame_threshold` | 窗口内累计检测到多少次违规即触发报警。 | 3 - 5 |
| `audio_device` | 麦克风输入设备 (ALSA)。 | `hw:0,0` |
| `engine_path` | TensorRT 模型文件的绝对路径。 | `models/best.engine` |

## 📝 开发规范

- **注释**: 逻辑代码使用英文命名，但**强烈鼓励使用中文注释**以提升团队协作效率。
- **资源管理**: 所有 C++ 节点必须在析构函数中正确显式 join/stop 背景线程与定时器。
- **构建依赖**: 任何对 `.msg` 文件的修改必须首先单独重新构建 `bishe_msgs`。

## 🤝 协作指南

有关更多 Agent 协作细节与环境坑点，请参考 [AGENTS.md](./AGENTS.md)。
