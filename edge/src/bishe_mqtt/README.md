# bishe_mqtt 功能包

## 概述

`bishe_mqtt` 是边缘检测系统的 MQTT 通信模块，负责与上位机（MQTT Broker）建立连接，实现状态上报和命令接收功能。该节点定时收集系统中各摄像头节点的运行时参数，并通过 JSON 格式上报给上位机，使运维人员能够实时监控边缘设备的运行状态。

## 功能特性

### 1. MQTT 连接管理
- 支持连接至指定的 MQTT Broker（支持 `tcp://` 和 `mqtt://` 协议）
- 自动重连机制（通过 Paho MQTT 库实现）
- 连接保活检测（Keep-Alive interval: 20秒）
- 完整的连接/断开日志记录

### 2. 多摄像头状态管理
- 支持同时管理多个摄像头实例
- 每个摄像头的独立状态信息收集
- 命名空间感知的节点发现机制

### 3. 动态参数获取
本节点通过 ROS2 的异步参数服务（AsyncParametersClient）实时获取以下节点的运行时参数：

| 源节点 | 获取参数 | 说明 |
|--------|----------|------|
| camera_node | width, height, framerate | 视频分辨率和帧率 |
| detector_node | confidence_threshold, nms_threshold | 目标检测阈值 |
| streamer_node | scale | 流媒体缩放比例 |

### 4. 定时状态上报
- 可配置的上报间隔（默认 1 秒，最小 100ms）
- JSON 格式的状态 payload
- 上报主题：`/jetson/info`（可配置）

### 5. 静态配置
- 摄像头位置信息（location）
- HTTP 流地址（http_url）
- 通过 launch 文件在启动时配置

## JSON 上报格式

```json
{
  "timestamp_ns": 1234567890123456789,
  "cameras": [
    {
      "id": "001",
      "location": "生产车间A区",
      "http_url": "http://192.168.1.100:8080/stream",
      "resolution": {
        "width": 1280,
        "height": 720,
        "fps": 60
      },
      "scale": 1.0,
      "confidence_threshold": 0.5,
      "nms_threshold": 0.5
    }
  ]
}
```

## 核心类说明

### MqttCallback
MQTT 消息回调处理类，负责接收来自 Broker 的消息。目前主要用于日志记录收到的消息内容，未来可扩展为命令解析和执行。

### MqttNode
主节点类，继承自 `rclcpp::Node`，核心逻辑包括：
- 参数声明与加载
- MQTT 客户端初始化与连接
- 定时器创建与回调处理
- 异步参数获取
- JSON payload 构建与发布

## 配置参数

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| broker | string | "fnas" | MQTT Broker 地址 |
| port | int | 1883 | MQTT 端口 |
| client_id | string | "jetson" | 客户端标识符 |
| subscribe_topic | string | "factory/camera/001/command" | 订阅主题（命令接收） |
| publish_topic | string | "factory/camera_001/status" | 发布主题（状态） |
| info_topic | string | "/jetson/info" | 状态信息发布主题 |
| report_interval_sec | double | 1.0 | 上报间隔（秒） |
| camera_ids | string[] | ["001"] | 摄像头 ID 列表 |
| camera_locations | string[] | [] | 摄像头位置列表 |
| camera_http_urls | string[] | [] | HTTP URL 列表 |

## 使用方法

### 启动方式
通过 launch 文件启动：

```bash
# 基础启动
ros2 launch bishe_launch multicamera.launch.py

# 指定 MQTT 参数
ros2 launch bishe_launch multicamera.launch.py camera_ids:="001,002"
```

### 动态参数修改
节点支持运行时参数动态修改，可通过 `ros2 param` 命令调整：

```bash
# 修改上报间隔
ros2 param set /mqtt_node report_interval_sec 2.0

# 修改上报主题
ros2 param set /mqtt_node info_topic "/jetson/info"
```

## 依赖

- ROS2 Humble
- rclcpp
- Paho MQTT C++ Library

## 文件结构

```
bishe_mqtt/
├── CMakeLists.txt      # 构建配置
├── package.xml         # 包描述文件
└── src/
    └── mqtt_node.cpp  # 主节点源代码
```

## 注意事项

1. **线程安全**：内部使用互斥锁保护 `info_cache_`，确保多线程访问安全
2. **超时处理**：异步参数获取采用 200ms 超时，避免阻塞主循环
3. **容错设计**：参数获取失败时不会中断上报流程，仅跳过该次获取
4. **连接恢复**：MQTT 连接断开时会自动重连
