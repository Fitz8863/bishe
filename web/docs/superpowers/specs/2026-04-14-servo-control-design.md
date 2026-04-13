# 舵机控制功能设计文档

**日期**: 2026-04-14  
**作者**: Sisyphus  
**状态**: 待用户审查

---

## 1. 概述

在实时视频监控页面 (`/monitor`) 的每个摄像头卡片下方增加舵机远程控制功能。通过接收 `jetson/esp8266/info` 心跳包中的 `id` 字段与 `jetson/info` 中 `cameras[].id` 字段匹配，在匹配成功的摄像头卡片下渲染控制面板，用户通过 9x9 网格点击选择位置，后端通过 MQTT 发送控制指令到 `jetson/esp8266/cmd` 主题。

---

## 2. 数据流架构

```
ESP8266 设备                              Web 后端                              前端 (monitor.html)
   │                                        │                                       │
   │─── jetson/esp8266/info ──────────────▶│                                       │
   │    {"id": "001", "status": "ok"}      │                                       │
   │                                        │──── 匹配 cameras[].id == "001" ─────▶│
   │                                        │    GET /api/cameras 返回 has_servo=true│
   │                                        │                                       │
   │                                        │◀──── POST /settings/api/servo/control ─┤
   │                                        │         {"camera_id": "001",           │
   │─── jetson/esp8266/cmd ◀───────────────│          "col": 5, "row": 7}            │
   │    {"id": "001",                       │                                       │
   │     "col": 5,                          │                                       │
   │     "row": 7}                          │                                       │
```

### 2.1 MQTT 主题

| 主题 | 方向 | QoS | Payload 示例 |
|------|------|-----|-------------|
| `jetson/esp8266/info` | 订阅 | 1 | `{"id": "001", "status": "online"}` |
| `jetson/esp8266/cmd` | 发布 | 2 | `{"id": "001", "col": 5, "row": 7}` |

---

## 3. 前端设计

### 3.1 UI 布局

每个摄像头卡片新增折叠式舵机控制面板：

```
┌───────────────────────────────────────┐
│ [📷 摄像头 001]          [Online]     │
├───────────────────────────────────────┤
│                                       │
│        ┌─ 视频流播放区域 ─┐            │
│        │                 │            │
│        │   WebRTC iframe │            │
│        │                 │            │
│        └─────────────────┘            │
│                                       │
├───────────────────────────────────────┤
│ [🎯 舵机控制 ▼]                       │  ← Bootstrap accordion, 可展开/折叠
├───────────────────────────────────────┤
│ ┌───────────────────────────────┐     │
│ │   9×9 网格 (点阵式)           │     │
│ │   · · · · · · · · ·          │     │
│ │   · · · · ● · · · ·  ← 已选  │     │
│ │   · · · · · · · · ·          │     │
│ │   ... (省略 6 行) ...         │     │
│ │                               │     │
│ │   当前坐标: col=5, row=2      │     │
│ │   [📤 发送指令]  [🔄 重置]    │     │
│ └───────────────────────────────┘     │
└───────────────────────────────────────┘
```

### 3.2 视觉规范

- **网格单元格**: 12px × 12px 圆形点，间距 4px，默认 `rgba(100,116,139,0.4)`
- **已选单元格**: 直径放大至 16px，背景 `var(--primary-color) #6366f1`，带发光效果
- **悬停效果**: 单元格边框变为 `var(--primary-light)`
- **折叠/展开**: 使用 Bootstrap 5 `collapse` 组件 + Font Awesome 旋转箭头图标
- **发送按钮**: `btn-sm btn-outline-primary`
- **重置按钮**: `btn-sm btn-outline-secondary`

### 3.3 交互逻辑

1. 前端每 10 秒通过 `GET /api/cameras` 获取摄像头列表
2. 若某摄像头 `has_servo: true`，渲染舵机控制面板
3. 点击网格中的点，记录当前 `col` 和 `row`，高亮该点
4. 点击"发送指令"按钮，POST 到后端，成功后显示绿色提示
5. 点击"重置"按钮，清除当前选择

### 3.4 新增前端文件

无需新增文件，所有变更在 `templates/monitor.html` 的 CSS 和 JS 部分完成。

---

## 4. 后端设计

### 4.1 `blueprints/mqtt_manager.py` 变更

#### 4.1.1 新增属性
```python
self.servo_devices = {}  # { camera_id: {"id": "001", "last_seen": timestamp} }
```

#### 4.1.2 修改 `_on_connect`
新增订阅: `client.subscribe("jetson/esp8266/info")`

#### 4.1.3 修改 `_on_message`
新增处理逻辑：
```python
elif msg.topic == "jetson/esp8266/info":
    payload = json.loads(msg.payload.decode('utf-8'))
    servo_id = payload.get('id')
    if servo_id:
        self.servo_devices[servo_id] = {
            'info': payload,
            'last_seen': time.time()
        }
```

#### 4.1.4 新增 `send_servo_command` 方法
```python
def send_servo_command(self, camera_id, col, row):
    """发送舵机控制指令到 jetson/esp8266/cmd"""
    payload = {"id": camera_id, "col": col, "row": row}
    return self.publish_raw("jetson/esp8266/cmd", payload)
```

#### 4.1.5 新增 `get_servo_status` 方法
```python
def get_servo_status(self):
    """返回在线舵机设备列表（10秒内有心跳视为在线）"""
    now = time.time()
    return {
        sid: info for sid, info in self.servo_devices.items()
        if now - info['last_seen'] < 10
    }
```

### 4.2 `blueprints/video_stream.py` 变更

修改 `list_cameras()` 函数：
- 新增从 `mqtt_manager.servo_devices` 获取在线舵机列表
- 为每个摄像头增加 `has_servo: True` 如果其 `id` 在 `servo_devices` 中

```python
online_servos = mqtt_manager.get_servo_status()
for cam in active_info:
    cam_data = {...}
    cam_data['has_servo'] = cam['id'] in online_servos
```

### 4.3 `blueprints/settings.py` 变更

新增路由：
```python
@settings_bp.route('/api/servo/control', methods=['POST'])
def servo_control():
    """前端调用的舵机控制 API 入口"""
    # 1. 解析 camera_id, col, row
    # 2. 校验 col (1-9), row (1-9)
    # 3. 调用 mqtt_manager.send_servo_command
    # 4. 返回成功/失败响应
```

**参数校验**:
- `camera_id`: 必填，字符串
- `col`: 必填，整数，范围 1-9
- `row`: 必填，整数，范围 1-9

**响应格式**:
- 成功: `{"message": "舵机控制指令已发送", "col": 5, "row": 7}`, 200
- 失败: `{"error": "参数错误: col 必须在 1-9 范围内"}`, 400
- MQTT未连接: `{"error": "MQTT未连接"}`, 400

---

## 5. 摄像头-Servo 匹配规则

1. `jetson/esp8266/info` 心跳包包含 `id` 字段（如 `"001"`）
2. 该 `id` 与 `jetson/info` 心跳中 `cameras[].id` 字段匹配时，表示舵机属于该摄像头
3. 匹配成功的摄像头在前端 `GET /api/cameras` 响应中 `has_servo: true`
4. 心跳超时（10 秒未收到）后，舵机从在线列表移除，前端自动隐藏控制面板

---

## 6. 错误处理

| 场景 | 前端表现 | 后端返回 |
|------|----------|----------|
| MQTT 未连接 | 不显示舵机控制面板 | N/A |
| 舵机离线（心跳超时） | 折叠面板自动隐藏 | N/A |
| 参数越界 (col/row 不在 1-9) | 红色错误提示 toast | 400 + error 消息 |
| MQTT 发送失败 | 红色错误提示 toast | 500 + error 消息 |
| 指令发送成功 | 绿色成功提示 toast | 200 + message |

---

## 7. 文件变更清单

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `blueprints/mqtt_manager.py` | 修改 | 新增 esp8266 主题订阅/解析，新增 servo 发布方法 |
| `blueprints/video_stream.py` | 修改 | `list_cameras` 增加 `has_servo` 字段 |
| `blueprints/settings.py` | 修改 | 新增 `/api/servo/control` 路由 |
| `templates/monitor.html` | 修改 | 新增舵机控制面板 CSS + JS |
| `tests/test_servo.py` | 新增 | 舵机控制 API 单元测试（可选） |

---

## 8. 测试计划

### 8.1 单元测试
- [ ] `send_servo_command` 发布正确的 JSON 到正确主题
- [ ] `get_servo_status` 只返回 10 秒内在线设备
- [ ] `/api/servo/control` 参数校验：col/row 范围检查
- [ ] MQTT 未连接时返回 400

### 8.2 集成测试
- [ ] 模拟 `jetson/esp8266/info` 心跳，验证 `has_servo` 字段
- [ ] 前端点击网格后发送正确的 POST 请求
- [ ] 心跳超时后面板消失
