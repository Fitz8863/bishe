# 舵机控制 (Servo Control) 实施计划

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在实时监控页面的摄像头卡片下增加 D-Pad 式舵机控制面板，通过 MQTT 发送 `{"id": "xxx", "col": N, "row": M}` 指令到 `jetson/esp8266/cmd` 主题。

**Architecture:** 后端 MQTTManager 新增 `jetson/esp8266/info` 订阅与心跳缓存；video_stream 返回摄像头数据时附加 `has_servo` 字段；settings 新增 `/api/servo/control` POST 路由；前端在 monitor.html 的 `createCameraCard` 函数中生成 D-Pad 面板 HTML + CSS + JS。

**Tech Stack:** Flask 3.1+, paho-mqtt 1.6.1, Bootstrap 5, Vanilla JS

---

## Chunk 1: 后端 MQTT 层

### Task 1: MQTTManager 新增 esp8266 心跳处理

**Files:**
- Modify: `blueprints/mqtt_manager.py` (全文)

#### 修改 1.1: 新增 `servo_devices` 属性

在 `__init__` 方法中，在 `self.devices = {}` 后新增:

```python
self.servo_devices = {}  # { servo_id: {"info": {...}, "last_seen": timestamp} }
```

#### 修改 1.2: `_on_connect` 新增订阅

在 `client.subscribe("jetson/info")` 后新增:

```python
client.subscribe("jetson/esp8266/info", qos=1)
```

#### 修改 1.3: `_on_message` 新增分支处理

在现有的 `if msg.topic == "jetson/info":` 块之后，新增 `elif` 分支:

```python
elif msg.topic == "jetson/esp8266/info":
    try:
        payload = json.loads(msg.payload.decode('utf-8'))
        servo_id = payload.get('id')
        if servo_id:
            self.servo_devices[servo_id] = {
                'info': payload,
                'last_seen': time.time()
            }
            self.connected = True
    except Exception as e:
        print(f"解析 jetson/esp8266/info 消息失败: {e}")
```

#### 修改 1.4: 新增 `send_servo_command` 方法

在 `send_intercom_command` 方法之后新增:

```python
def send_servo_command(self, camera_id, col, row):
    """发送舵机控制指令到 jetson/esp8266/cmd 主题"""
    payload = {"id": camera_id, "col": col, "row": row}
    return self.publish_raw("jetson/esp8266/cmd", payload)
```

#### 修改 1.5: 新增 `get_servo_status` 方法

在 `get_active_data` 方法之后新增:

```python
def get_servo_status(self):
    """返回在线舵机设备字典（10秒内有心跳视为在线）"""
    now = time.time()
    return {
        sid: info for sid, info in self.servo_devices.items()
        if now - info['last_seen'] < 10
    }
```

- [ ] **Step 1: 阅读 mqtt_manager.py 全文，确认修改位置**
- [ ] **Step 2: 依次应用上述 5 个修改**
- [ ] **Step 3: flake8 检查**
  ```bash
  flake8 blueprints/mqtt_manager.py --max-line-length=120
  ```
- [ ] **Step 4: 手动测试（启动应用后连接 MQTT，发送模拟消息）**

---

## Chunk 2: 后端 API 层

### Task 2: video_stream.py 增加 has_servo 字段

**Files:**
- Modify: `blueprints/video_stream.py:82-108` (`list_cameras` 函数)

修改 `list_cameras()` 函数，在构建摄像头列表时为每个摄像头增加 `has_servo` 字段:

```python
def list_cameras():
    """列出所有摄像头 (仅使用动态MQTT发现)"""
    from blueprints.mqtt_manager import mqtt_manager
    
    if not mqtt_manager or not mqtt_manager.connected:
        return jsonify({
            'cameras': [],
            'mqtt_connected': False,
            'error': '请先在系统设置中连接远程服务器'
        }), 200

    dynamic_cameras = []
    active_info = mqtt_manager.get_active_cameras()
    # 获取在线舵机设备列表
    online_servos = mqtt_manager.get_servo_status()
    
    for cam in active_info:
        cam_data = {
            'id': cam['id'],
            'name': cam.get('location', f"摄像头 {cam['id']}"),
            'webrtc_url': cam.get('http_url'),
            'is_dynamic': True,
            'has_servo': cam['id'] in online_servos  # 新增字段
        }
        dynamic_cameras.append(cam_data)
    
    return jsonify({
        'cameras': dynamic_cameras,
        'mqtt_connected': True
    }), 200
```

- [ ] **Step 1: 应用修改到 `list_cameras()` 函数**
- [ ] **Step 2: flake8 检查**
  ```bash
  flake8 blueprints/video_stream.py --max-line-length=120
  ```

### Task 3: settings.py 新增舵机控制 API 路由

**Files:**
- Modify: `blueprints/settings.py` (在文件末尾 `get_jetson_info` 路由之后新增)

```python
@settings_bp.route('/api/servo/control', methods=['POST'])
def servo_control():
    """前端调用的舵机控制 API 入口"""
    data = request.json
    if not data:
        return jsonify({'error': '请求体必须为JSON格式'}), 400
    
    camera_id = data.get('camera_id')
    col = data.get('col')
    row = data.get('row')
    
    if not camera_id:
        return jsonify({'error': '缺少 camera_id'}), 400
    if col is None or row is None:
        return jsonify({'error': '缺少 col 或 row 参数'}), 400
    
    try:
        col = int(col)
        row = int(row)
    except (TypeError, ValueError):
        return jsonify({'error': 'col 和 row 必须为整数'}), 400
    
    if not (-10 <= col <= 10):
        return jsonify({'error': f'col 必须在 -10~10 范围内，当前值: {col}'}), 400
    if not (-8 <= row <= 8):
        return jsonify({'error': f'row 必须在 -8~8 范围内，当前值: {row}'}), 400
    
    try:
        from blueprints.mqtt_manager import mqtt_manager
        if not mqtt_manager or not mqtt_manager.connected:
            return jsonify({'error': 'MQTT未连接'}), 400
        
        success, message = mqtt_manager.send_servo_command(camera_id, col, row)
        if success:
            return jsonify({
                'message': '舵机控制指令已发送',
                'col': col,
                'row': row
            }), 200
        return jsonify({'error': message}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500
```

- [ ] **Step 1: 将上述路由添加到 settings.py 末尾**
- [ ] **Step 2: flake8 检查**
  ```bash
  flake8 blueprints/settings.py --max-line-length=120
  ```

### Task 4: 编写后端单元测试

**Files:**
- Create: `tests/test_servo.py`

```python
import pytest
from blueprints.mqtt_manager import MQTTManager
import time


class TestServoStatus:
    """测试 get_servo_status 方法"""

    def test_empty_when_no_devices(self):
        manager = MQTTManager(broker='127.0.0.1')
        result = manager.get_servo_status()
        assert result == {}

    def test_returns_recent_device(self):
        manager = MQTTManager(broker='127.0.0.1')
        # 模拟收到心跳
        manager.servo_devices['001'] = {
            'info': {'id': '001'},
            'last_seen': time.time()
        }
        result = manager.get_servo_status()
        assert '001' in result

    def test_filters_expired_device(self):
        manager = MQTTManager(broker='127.0.0.1')
        # 模拟 15 秒前的心跳（已超过 10 秒超时）
        manager.servo_devices['001'] = {
            'info': {'id': '001'},
            'last_seen': time.time() - 15
        }
        result = manager.get_servo_status()
        assert '001' not in result


class TestServoCommand:
    """测试 send_servo_command 方法"""

    def test_builds_correct_payload(self, monkeypatch):
        """验证 send_servo_command 构建正确的 payload 并调用 publish_raw"""
        manager = MQTTManager(broker='127.0.0.1')
        manager.connected = True
        
        captured = {}
        def mock_publish_raw(topic, payload):
            captured['topic'] = topic
            captured['payload'] = payload
            return True, "OK"
        
        monkeypatch.setattr(manager, 'publish_raw', mock_publish_raw)
        manager.send_servo_command('001', 5, -3)
        
        assert captured['topic'] == 'jetson/esp8266/cmd'
        assert captured['payload'] == {'id': '001', 'col': 5, 'row': -3}

    def test_fails_when_not_connected(self):
        manager = MQTTManager(broker='127.0.0.1')
        manager.connected = False
        manager.client = None
        success, msg = manager.send_servo_command('001', 0, 0)
        assert not success
        assert '未连接' in msg
```

- [ ] **Step 1: 创建 test_servo.py**
- [ ] **Step 2: 运行测试确保通过**
  ```bash
  pytest tests/test_servo.py -v
  ```
- [ ] **Step 3: 如有失败，调试修复**
- [ ] **Step 4: 提交**
  ```bash
  git add blueprints/mqtt_manager.py blueprints/video_stream.py blueprints/settings.py tests/test_servo.py
  git commit -m "feat: add servo control backend - MQTT subscription, API route, tests"
  ```

---

## Chunk 3: 前端 D-Pad UI

### Task 5: monitor.html 新增舵机控制面板 CSS

**Files:**
- Modify: `templates/monitor.html` (在 `{% block extra_css %}` 的 `<style>` 标签内，`.status-dot-intercom` 之前新增)

```css
    /* Servo Control D-Pad */
    .servo-panel {
        padding: 1rem;
        background: #0f172a;
        border-top: 1px solid var(--border-color);
    }
    .servo-toggle {
        display: flex;
        align-items: center;
        gap: 0.5rem;
        cursor: pointer;
        user-select: none;
        padding: 0.5rem 0;
        color: var(--text-secondary);
        font-size: 0.8rem;
        font-weight: 600;
        text-transform: uppercase;
        letter-spacing: 0.025em;
    }
    .servo-toggle i {
        transition: transform 0.2s;
    }
    .servo-toggle.collapsed i {
        transform: rotate(-90deg);
    }
    .dpad-container {
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 0.25rem;
        padding: 0.75rem 0;
    }
    .dpad-row {
        display: flex;
        justify-content: center;
        align-items: center;
        gap: 0.25rem;
    }
    .dpad-btn {
        width: 48px;
        height: 48px;
        border-radius: 50%;
        background: #1e293b;
        border: 1px solid var(--border-color);
        color: var(--text-primary);
        display: flex;
        align-items: center;
        justify-content: center;
        font-size: 0.85rem;
        cursor: pointer;
        transition: all 0.15s;
    }
    .dpad-btn:hover:not(:disabled) {
        background: #334155;
        border-color: var(--primary-color);
        color: var(--primary-color);
    }
    .dpad-btn:active:not(:disabled) {
        transform: scale(0.95);
    }
    .dpad-btn:disabled {
        opacity: 0.3;
        cursor: not-allowed;
    }
    .dpad-btn.center-btn {
        width: 40px;
        height: 40px;
        background: var(--primary-color);
        border-color: var(--primary-color);
        color: #fff;
    }
    .dpad-btn.center-btn:hover {
        background: #4f46e5;
    }
    .dpad-coords {
        font-family: monospace;
        font-size: 0.75rem;
        color: var(--text-secondary);
        margin-top: 0.5rem;
        text-align: center;
    }
    .dpad-coords .coord-value {
        color: var(--primary-light);
        font-weight: 700;
    }
```

- [ ] **Step 1: 在 monitor.html 的 `<style>` 标签内追加上述 CSS**
- [ ] **Step 2: 保存后检查 HTML 语法**

### Task 6: monitor.html 修改 createCameraCard 增加舵机面板

**Files:**
- Modify: `templates/monitor.html` (`createCameraCard` 函数, ~line 513)

将 `createCameraCard` 函数的 `card.innerHTML` 替换为以下版本:

```javascript
function createCameraCard(id, name, webrtcUrl, hasServo) {
    const card = document.createElement('div');
    card.className = 'monitor-card';
    card.id = `camera-card-${id}`;
    
    const servoPanel = hasServo ? `
        <div class="servo-panel">
            <div class="servo-toggle collapsed" data-bs-toggle="collapse" data-bs-target="#servo-${id}">
                <i class="fas fa-chevron-down me-1"></i>
                <i class="fas fa-gamepad me-1"></i>舵机控制
            </div>
            <div class="collapse" id="servo-${id}">
                <div class="dpad-container">
                    <div class="dpad-row">
                        <button class="dpad-btn" onclick="servoMove('${id}', 0, 1)" title="Row +1">
                            <i class="fas fa-chevron-up"></i>
                        </button>
                    </div>
                    <div class="dpad-row">
                        <button class="dpad-btn" onclick="servoMove('${id}', -1, 0)" title="Col -1">
                            <i class="fas fa-chevron-left"></i>
                        </button>
                        <button class="dpad-btn center-btn" onclick="servoCenter('${id}')" title="归中">
                            <i class="fas fa-circle"></i>
                        </button>
                        <button class="dpad-btn" onclick="servoMove('${id}', 1, 0)" title="Col +1">
                            <i class="fas fa-chevron-right"></i>
                        </button>
                    </div>
                    <div class="dpad-row">
                        <button class="dpad-btn" onclick="servoMove('${id}', 0, -1)" title="Row -1">
                            <i class="fas fa-chevron-down"></i>
                        </button>
                    </div>
                    <div class="dpad-coords">
                        col: <span class="coord-value" id="servo-col-${id}">0</span>
                        &nbsp;|&nbsp;
                        row: <span class="coord-value" id="servo-row-${id}">0</span>
                    </div>
                </div>
            </div>
        </div>
    ` : '';
    
    card.innerHTML = `
        <div class="monitor-header">
            <span class="fw-bold" style="font-size: 0.85rem; letter-spacing: 0.025em;">
                <i class="fas fa-video me-2 text-primary"></i>${name}
            </span>
            <span class="status-badge status-normal">
                <span class="status-dot"></span>Online
            </span>
        </div>
        <div class="monitor-body">
            <div class="video-overlay" id="overlay-${id}">
                <i class="fas fa-circle-notch fa-spin"></i>
                <div class="x-small text-secondary" style="font-family: monospace;">STREAMING_SYNC...</div>
            </div>
            <iframe src="${webrtcUrl}" allowfullscreen data-url="${webrtcUrl}" onload="hideOverlay('${id}')"></iframe>
        </div>
        <div class="monitor-stats">
            <div class="d-flex justify-content-between align-items-center">
                <span><i class="fas fa-microchip me-1"></i>Node: ${id}</span>
                <button class="btn-refresh-stream" onclick="refreshStream('${id}')">
                    <i class="fas fa-sync-alt me-1"></i>Reconnect
                </button>
            </div>
        </div>
        ${servoPanel}
    `;
    return card;
}
```

同时修改 `syncCameras` 函数中调用 `createCameraCard` 的地方 (~line 498):

```javascript
// 原: const card = createCameraCard(camera.id, camera.name, camera.webrtc_url);
// 改为:
const card = createCameraCard(camera.id, camera.name, camera.webrtc_url, camera.has_servo);
```

- [ ] **Step 1: 替换 createCameraCard 函数**
- [ ] **Step 2: 修改 syncCameras 中的调用**
- [ ] **Step 3: 保存文件**

### Task 7: monitor.html 新增舵机控制 JS 函数

**Files:**
- Modify: `templates/monitor.html` (在 `updateCameraCard` 函数之后, `window.onbeforeunload` 之前新增)

```javascript
    // Servo control state per camera
    const servoState = new Map(); // { camera_id: { col: 0, row: 0, debounce: false } }
    
    function servoMove(cameraId, deltaCol, deltaRow) {
        if (!servoState.has(cameraId)) {
            servoState.set(cameraId, { col: 0, row: 0, debounce: false });
        }
        const state = servoState.get(cameraId);
        if (state.debounce) return;  // 防抖
        
        const newCol = state.col + deltaCol;
        const newRow = state.row + deltaRow;
        
        // 边界检查
        if (newCol < -10 || newCol > 10 || newRow < -8 || newRow > 8) return;
        
        state.col = newCol;
        state.row = newRow;
        
        updateServoDisplay(cameraId);
        sendServoCommand(cameraId, state.col, state.row);
        
        // 200ms 防抖
        state.debounce = true;
        setTimeout(() => { state.debounce = false; }, 200);
    }
    
    function servoCenter(cameraId) {
        if (!servoState.has(cameraId)) {
            servoState.set(cameraId, { col: 0, row: 0, debounce: false });
        }
        const state = servoState.get(cameraId);
        state.col = 0;
        state.row = 0;
        
        updateServoDisplay(cameraId);
        sendServoCommand(cameraId, 0, 0);
    }
    
    function updateServoDisplay(cameraId) {
        const state = servoState.get(cameraId);
        if (!state) return;
        
        const colEl = document.getElementById(`servo-col-${cameraId}`);
        const rowEl = document.getElementById(`servo-row-${cameraId}`);
        if (colEl) colEl.textContent = state.col;
        if (rowEl) rowEl.textContent = state.row;
        
        // 更新按钮禁用状态
        const card = document.getElementById(`camera-card-${cameraId}`);
        if (!card) return;
        const buttons = card.querySelectorAll('.dpad-btn:not(.center-btn)');
        buttons.forEach(btn => {
            const onclick = btn.getAttribute('onclick') || '';
            const match = onclick.match(/servoMove\('[^']+',\s*(-?\d+),\s*(-?\d+)\)/);
            if (!match) return;
            const [, deltaCol, deltaRow] = match.map(Number);
            const futureCol = state.col + deltaCol;
            const futureRow = state.row + deltaRow;
            btn.disabled = futureCol < -10 || futureCol > 10 || futureRow < -8 || futureRow > 8;
        });
    }
    
    function sendServoCommand(cameraId, col, row) {
        fetch('/settings/api/servo/control', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                camera_id: cameraId,
                col: col,
                row: row
            })
        })
        .then(res => res.json())
        .then(data => {
            if (data.error) {
                console.error(`舵机控制失败: ${data.error}`);
                // 回滚本地状态
                if (servoState.has(cameraId)) {
                    const state = servoState.get(cameraId);
                    state.col -= col - (servoState.get(cameraId)._lastSentCol || 0);
                    state.row -= row - (servoState.get(cameraId)._lastSentRow || 0);
                    updateServoDisplay(cameraId);
                }
            }
        })
        .catch(err => console.error(`舵机请求异常: ${err}`));
    }
```

**同步修复**: 在 `sendServoCommand` 成功时记录 `_lastSentCol`/`_lastSentRow`:

```javascript
// 在 sendServoCommand 的 .then 成功回调中，data.error 不存在时:
if (servoState.has(cameraId)) {
    const state = servoState.get(cameraId);
    state._lastSentCol = col;
    state._lastSentRow = row;
}
```

- [ ] **Step 1: 在 monitor.html 的 JS 部分追加上述代码**
- [ ] **Step 2: 确认所有花括号/引号闭合正确**
- [ ] **Step 3: 保存文件**

---

## Chunk 4: 验证与提交

### Task 8: 全量验证

- [ ] **Step 1: 运行 flake8 检查所有修改文件**
  ```bash
  flake8 blueprints/mqtt_manager.py blueprints/video_stream.py blueprints/settings.py --max-line-length=120
  ```

- [ ] **Step 2: 运行全部测试**
  ```bash
  pytest tests/test_servo.py -v
  ```

- [ ] **Step 3: 启动应用手动验证**
  ```bash
  python app.py
  ```
  - 访问 `/monitor` 页面
  - 确认已上线且有舵机的摄像头卡片下显示"舵机控制"折叠栏
  - 展开后显示 D-Pad 面板
  - 点击方向按钮，坐标值变化，检查浏览器 Network 面板确认 POST 请求发送
  - 确认边界值按钮在到达 -10/10/-8/8 时禁用

- [ ] **Step 4: 提交前端变更**
  ```bash
  git add templates/monitor.html
  git commit -m "feat: add D-Pad servo control UI to monitor page"
  ```
