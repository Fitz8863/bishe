# 多摄像头配置驱动架构设计

**日期**: 2026-03-14
**状态**: 设计中

## 1. 目标

在现有单摄像头 pipeline (bishe_camera → bishe_detector → bishe_streamer) 基础上，实现配置驱动的多摄像头支持。每个摄像头拥有独立的参数，通过 YAML 配置文件和动态 launch 生成实现。

## 2. 现有架构分析

### 当前结构
```
bishe.launch.py (硬编码单实例)
├── camera_node (/dev/video0)
├── detector_node (订阅 camera/image_raw)
├── streamer_node
├── monitor_node
└── mqtt_node
```

### 现有参数声明方式 (C++)
```cpp
this->declare_parameter<std::string>("device", "/dev/video0");
this->declare_parameter<int>("width", 1280);
// ... 节点名称固定为 camera_node, detector_node 等
```

## 3. 架构设计

### 3.1 命名空间隔离

每个摄像头实例运行在独立 ROS 2 命名空间中，实现话题和节点隔离：

```
/camera_001/
├── camera_node
├── detector_node  
└── streamer_node

/camera_002/
├── camera_node
├── detector_node
└── streamer_node
```

**话题映射**：
- `/camera_001/camera/image_raw` → detector 订阅
- `/camera_001/detector/result` → monitor 订阅

### 3.2 配置文件结构

```yaml
# config/cameras.yaml
cameras:
  - id: "001"
    name: "生产线A"
    device: "/dev/video0"
    location: "生产车间A区"
    width: 1280
    height: 720
    framerate: 60
    detector:
      confidence_threshold: 0.5
      nms_threshold: 0.5
      worker_threads: 1
    streamer:
      rtsp_url: "rtsp://localhost:8554/stream_001"
      scale: 1.0
    mqtt:
      publish_topic: "factory/camera_001/status"

  - id: "002"
    name: "生产线B"
    device: "/dev/video2"
    # ... 独立参数
```

### 3.3 动态 Launch 生成

```python
# launch/multicamera.launch.py
def generate_launch_description():
    cameras_config = load_cameras_config()  # 加载 YAML
    
    nodes = []
    for camera in cameras_config['cameras']:
        ns = f"/camera_{camera['id']}"
        
        # Camera Node
        nodes.append(Node(
            package="bishe_camera",
            executable="camera_node",
            name="camera_node",
            namespace=ns,
            parameters=[camera],  # 透传整个 camera dict
        ))
        
        # Detector Node (自动订阅 namespace 下的 image_raw)
        nodes.append(Node(
            package="bishe_detector",
            executable="detector_node",
            name="detector_node", 
            namespace=ns,
            parameters=[camera.get('detector', {})],
        ))
        
        # ... streamer, monitor, mqtt
    
    return LaunchDescription(nodes)
```

### 3.4 关键设计决策

| 决策项 | 方案 | 理由 |
|--------|------|------|
| 命名空间隔离 | `/camera_{id}` | 话题隔离，避免冲突 |
| 参数传递 | 透传 YAML dict | 最小化代码改动 |
| 节点命名 | 固定名称 (camera_node 等) | 保持现有逻辑不变 |
| Monitor 聚合 | 新增 aggregator 节点 | 收集所有摄像头告警 |

## 4. 改动范围

### 4.1 需要修改的包

| 包 | 改动 |
|----|------|
| bishe_launch | 新增 multicamera.launch.py，删除硬编码 |
| bishe_monitor | 新增 aggregator 节点（可选） |
| bishe_mqtt | 支持 namespace 动态 topic |

### 4.2 无需修改的包

- **bishe_camera**: 参数已通过 declare_parameter 外部化
- **bishe_detector**: 订阅相对话题 `camera/image_raw`，namespace 自动生效
- **bishe_streamer**: 参数外部化

## 5. 待验证假设

1. detector_node 订阅相对话题 `camera/image_raw` 能在 namespace 下正确解析
2. YAML 字典能直接作为 parameters[] 传递
3. 多实例运行时无资源冲突 (GPU 线程池、视频设备)

## 6. 风险与约束

- **GPU 资源**: 多 detector 共享同一 GPU，需通过 worker_threads 调配
- **网络带宽**: 多路 RTSP 推流需评估带宽
- **配置验证**: 启动前需校验 device 存在、topic 不冲突

---

**下一步**: 实现 plan 阶段，拆分具体任务
