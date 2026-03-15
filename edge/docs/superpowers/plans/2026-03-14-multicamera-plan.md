# 多摄像头配置驱动实现计划

**日期**: 2026-03-14
**前置**: design spec 已完成

## 阶段 1: 配置结构定义

### T1.1 创建摄像头配置文件
- **文件**: `src/bishe_launch/config/cameras.yaml`
- **内容**: 示例 2 摄像头配置 (id: 001, 002)
- **验证**: YAML 语法正确

### T1.2 定义配置加载模块
- **文件**: `src/bishe_launch/bishe_launch/config_loader.py`
- **函数**: `load_cameras_config(config_path: str) -> dict`
- **验证**: pytest 读取测试

---

## 阶段 2: 动态 Launch 生成

### T2.1 创建 multicamera.launch.py
- **文件**: `src/bishe_launch/launch/multicamera.launch.py`
- **逻辑**: 遍历配置数组，为每个摄像头创建 namespace 内的节点组
- **验证**: `ros2 launch --print-description` 输出正确

### T2.2 添加参数覆盖支持
- 支持 CLI 参数覆盖默认配置: `camera_id:=003 device:=/dev/video3`
- **验证**: 单独测试参数覆盖

---

## 阶段 3: 节点适配 (如需要)

### T3.1 验证 namespace 话题解析
- **测试**: 启动单摄像头，确认 `camera/image_raw` 话题在 namespace 下正确
- **问题**: 若不工作，调整订阅逻辑

### T3.2 Monitor 多实例支持
- 检查 monitor_node 是否需要 `camera_id` 参数动态化
- **验证**: 两个 monitor 实例同时运行

### T3.3 MQTT Topic 动态化
- **改动**: mqtt_node 接收 `publish_topic` 参数
- **验证**: 两条不同 topic 的消息发送

---

## 阶段 4: 测试与验证

### T4.1 单摄像头回归测试
- 使用新 launch 启动单摄像头 (001)
- **验证**: 检测、推流、MQTT 上报正常

### T4.2 双摄像头并行测试
- 同时启动 001 和 002
- **验证**:
  - `/camera_001/camera/image_raw` 和 `/camera_002/camera/image_raw` 均存在
  - 两路 RTSP 流正常
  - 独立告警触发

### T4.3 资源监控
- 监控 GPU 内存、CPU 使用率
- **验证**: 负载在可接受范围

---

## 任务清单

| ID | 任务 | 依赖 | 预计改动文件 |
|----|------|------|-------------|
| T1.1 | 创建 cameras.yaml | - | new: src/bishe_launch/config/cameras.yaml |
| T1.2 | 配置加载模块 | T1.1 | new: src/bishe_launch/bishe_launch/config_loader.py |
| T2.1 | multicamera.launch.py | T1.2 | new: src/bishe_launch/launch/multicamera.launch.py |
| T2.2 | CLI 参数覆盖 | T2.1 | modify: multicamera.launch.py |
| T3.1 | 验证 namespace | T2.1 | test only |
| T3.2 | Monitor 多实例 | T2.1 | test only (可能无需改动) |
| T3.3 | MQTT topic 动态化 | T2.1 | test only (可能无需改动) |
| T4.1 | 单摄像头回归 | - | integration test |
| T4.2 | 双摄像头并行 | T4.1 | integration test |
| T4.3 | 资源监控 | T4.2 | verification |

---

## 执行命令

```bash
# 构建
colcon build --packages-select bishe_launch --symlink-install
source install/setup.bash

# 单摄像头测试
ros2 launch bishe_launch multicamera.launch.py camera_ids:="['001']"

# 双摄像头测试
ros2 launch bishe_launch multicamera.launch.py camera_ids:="['001','002']"

# 查看话题
ros2 topic list | grep camera
```

---

**里程碑**: T4.2 通过 = 多摄像头功能完成
