# 化工厂危险行为检测系统 (Chemical Plant Hazard Detection System)

## 项目概述

本项目是一套完整的化工厂危险行为检测与监控系统，采用多层级分布式架构实现边缘计算、云端管理和远程控制。系统涵盖 ESP8266 智能控制器、Jetson Edge 边缘检测、Web 后端管理和 STM32 下位机控制四大核心模块。

## 系统架构

```
bishe/                           # 项目根目录
├── web/                         # Flask Web 后端
├── edge/                        # ROS2 边缘计算节点
├── esp8266/                     # ESP8266 智能控制器
├── stm32/                      # STM32 下位机
├── docs/                       # 设计文档
├── PCB/                        # 硬件设计
└── README.md                   # 本文档
```

---

## MQTT 主题总表

本系统所有通信均基于 MQTT 协议，以下是完整的主题列表：

### 上行主题（Edge/ESP8266 -> Web）

| 主题 | 发布者 | 说明 | Payload 示例 |
|-----|-------|------|-------------|
| `jetson/info` | Edge (mqtt_node) | 边缘设备心跳，包含摄像头列表 | `{"device": "001", "cameras": [...], "timestamp_ns": ...}` |
| `jetson/esp8266/info` | ESP8266 | 舵机设备心跳 | `{"id": "001", ...}` |
| `jetson/alarm` | Edge (mqtt_node) | 远程告警事件 | `{"alarm_type": "fire", "camera_id": "001", "location": "生产车间A区", "timestamp_ns": ...}` |

### 下行主题（Web -> Edge）

| 主题 | 订阅者 | 说明 | Payload 示例 |
|-----|-------|------|-------------|
| `jetson/camera/{camera_id}/command` | Edge (mqtt_node) | 配置指令下发 | `{"type": "parameters", "value": {"confidence_threshold": 0.7, ...}}` |
| `jetson/call/command` | Edge (mqtt_node) | 对讲控制 | `{"type": "intercom_start", "url": "rtsp://..."}` 或 `{"type": "intercom_stop"}` |
| `jetson/esp8266/cmd` | ESP8266 | 舵机控制指令 | `{"id": "001", "col": 5, "row": 3}` |

---

## 模块详解

### 1. Web 后端 (web/)

基于 Flask 3.1+ 的 Web 管理平台，负责数据展示、用户认证和 MQTT 消息转发。

**核心技术**：
- Flask 3.1+ (Blueprint 模块化)
- Flask-SocketIO (实时通信)
- Flask-Login (用户认证)
- SQLAlchemy (ORM)
- paho-mqtt (MQTT 客户端)

**订阅的主题**：
- `jetson/info` - 设备心跳
- `jetson/esp8266/info` - 舵机状态
- `jetson/alarm` - 火灾告警

**发布的主题**：
- `jetson/camera/{id}/command` - 配置指令
- `jetson/call/command` - 对讲控制
- `jetson/esp8266/cmd` - 舵机控制

**核心功能**：
- 用户注册/登录 (RBAC)
- 实时视频监控 (WebRTC)
- 危险行为抓拍记录
- 舵机远程控制 (D-Pad + 滑块)
- 火灾警报联动 (收到 `alarm_type: fire` 播放音频 3 次)

---

### 2. Edge 边缘计算 (edge/)

基于 ROS2 的边缘计算节点，负责视频流处理、目标检测和 MQTT 通信。

**核心技术**：
- ROS2 Humble
- OpenCV (视频捕获)
- YOLOv5/v8/v10 (目标检测)
- Paho MQTT (C++) (消息队列)

**订阅的主题**：
- `jetson/camera/command` - 配置指令
- `jetson/call/command` - 对讲控制

**发布的主题**：
- `jetson/info` - 设备心跳 + 摄像头列表
- `jetson/alarm` - 告警事件

**核心节点**：

| 节点 | 功能 |
|-----|-----|
| `camera_node` | 视频流采集与发布 |
| `detector_node` | YOLO 目标检测 |
| `mqtt_node` | MQTT 消息桥接 |
| `streamer_node` | RTSP 流媒体服务 |
| `monitor_node` | 监控与告警触发 |

---

### 3. ESP8266 控制器 (esp8266/)

ESP8266 微控制器，负责舵机控制和数据通信。

**核心技术**：
- Arduino / ESP8266 Core
- SoftwareSerial ( UART 通信)
- PubSubClient ( MQTT)

**订阅的主题**：
- `jetson/esp8266/cmd` - 舵机控制指令

**发布的主题**：
- `jetson/esp8266/info` - 设备心跳状态

**控制指令格式**：
```json
{"id": "001", "col": 5, "row": 3}
```
- COL 范围: -8 ~ 8
- ROW 范围: -10 ~ 10

---

### 4. STM32 下位机 (stm32/)

基于 STM32F103 的下位机控制板，负责舵机驱动和传感器采集。

**核心技术**：
- STM32F103C8T6
- FreeRTOS (实时操作系统)
- HAL 库

**通信方式**：
- USART (与 ESP8266 通信)
- PWM (舵机控制)
- GPIO (传感器/继电器)

---

## 数据流向图

```
┌─────────────────┐     jetson/info      ┌─────────────────┐
│   Edge (ROS2)   │─────────────────────▶│   Web (Flask)   │
│  detector_node  │                     │   SocketIO      │
└────────┬────────┘                     └────────┬────────┘
         │ jetson/alarm                          │
         │ (alarm_type: fire)                  │
         ▼                                      ▼
┌─────────────────┐                     ┌─────────────────┐
│   MQTT Broker    │◀───────────────────│  Browser        │
│   (paho-mqtt)   │                     │  (alarm.mp3)    │
└────────┬────────┘                     └─────────────────┘
         │ jetson/esp8266/cmd
         ▼
┌─────────────────┐     jetson/esp8266/info
│  ESP8266 + STM32  │─────────────────────▶
│   舵机控制      │
└─────────────────┘
```

---

## 火灾告警流程

1. **Edge 检测到火灾**：`detector_node` 通过 `monitor_node` 发布告警
2. **MQTT 转发**：`mqtt_node` 收到告警后发布到 `jetson/alarm`
3. **Web 接收**：`mqtt_manager.py` 订阅 `jetson/alarm`，检测 `alarm_type == "fire"`
4. **SocketIO 广播**：通过 Flask-SocketIO 向前端广播 `fire_alarm` 事件
5. **前端响应**：浏览器接收到事件，播放 `/static/alarm/alarm.mp3` 三次

---

## 舵机控制流程

1. **Web 前端操作**：用户在监控页面点击 D-Pad 或拖动滑块
2. **HTTP 请求**：前端 POST 到 `/settings/api/servo/control`
3. **MQTT 发布**：`settings.py` 调用 `mqtt_manager.send_servo_command()`
4. **ESP8266 接收**：订阅 `jetson/esp8266/cmd`，解析 JSON
5. **UART 转发**：ESP8266 通过 UART 转发给 STM32
6. **STM32 执行**：PWM 控制舵机转动到指定角度

---

## 快速开始

### 环境要求

- Python 3.10+
- ROS2 Humble
- MySQL 5.7+
- MQTT Broker (e.g., EMQX, Mosquitto)

### Web 后端启动

```bash
cd web
conda activate bishe
python app.py
# 访问 http://localhost:5001
```

### Edge 启动

```bash
cd edge
source install/setup.bash
ros2 launch bishebringup bringup.launch.py
```

---

## 配置文件

| 配置项 | 文件 | 说明 |
|-------|-----|------|
| MQTT Broker | `web/config.py` | `MQTT_BROKER`, `MQTT_PORT` |
| 主题前缀 | `web/config.py` | `MQTT_TOPIC_PREFIX` |
| 摄像头配置 | `web/cameras.json` | 摄像头 ID、名称、WebRTC URL |

---

## 依赖版本

### Web 后端

```
Flask==3.1.3
Flask-SQLAlchemy==3.1.1
Flask-Login==0.6.3
Flask-Bcrypt==1.0.1
Flask-SocketIO==5.x
paho-mqtt==1.6.1
PyMySQL==1.1.2
```

### Edge

```
ROS2 Humble
OpenCV 4.x
YOLOv5/v8/v10
Paho MQTT C++ Library
```

---

## 开发指南

- 遵循 Flask 工厂模式 + Blueprint 架构
- Edge 节点采用 ROS2 标准包结构
- 新增功能需更新本文档的主题表格
- 确保 MQTT 主题命名规范统一

---

## 许可证

MIT License