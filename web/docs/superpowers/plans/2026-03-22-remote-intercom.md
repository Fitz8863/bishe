# 远程语音对讲功能实施计划 (Remote Intercom Implementation Plan)

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现一个基于 WebRTC WHIP 和 MediaMTX 的远程语音对讲功能，支持从监控页面选择设备并进行实时语音对讲。

**Architecture:** 浏览器通过 WebRTC WHIP 推流音频到 MediaMTX，MediaMTX 转为 RTSP 流，Jetson 通过 MQTT 指令触发拉流播放。

**Tech Stack:** Flask, WebRTC API (WHIP), paho-mqtt, MediaMTX.

---

## Chunk 1: 后端配置与设备列表 API

### Task 1: 扩展数据库模型以支持 MediaMTX 配置
**Files:**
- Modify: `blueprints/models.py`
- Modify: `blueprints/settings.py`

- [ ] **Step 1: 在 `MqttConfig` 模型中添加 MediaMTX 字段**
  添加 `mediamtx_ip`, `mediamtx_whip_port`, `mediamtx_rtsp_port`。
- [ ] **Step 2: 更新设置保存逻辑**
  在 `settings.py` 的 `save_mqtt_config` 中处理这些新字段。
- [ ] **Step 3: 验证模型变更**
  重启应用以触发 `db.create_all()`。

### Task 2: 实现设备列表与对讲控制 API
**Files:**
- Modify: `blueprints/settings.py`
- Modify: `blueprints/mqtt_manager.py`

- [ ] **Step 1: 在 `settings.py` 中增加 `/api/mqtt/devices` 接口**
  返回 `mqtt_manager.get_active_data()` 的结果。
- [ ] **Step 2: 在 `mqtt_manager.py` 中增加对讲指令方法**
  实现 `send_intercom_command(device_id, action, url)`。
- [ ] **Step 3: 增加 `/api/intercom/start` 和 `/api/intercom/stop` 接口**
  接收 `device_id`，检查状态锁，下发 MQTT 指令。

---

## Chunk 2: 前端 UI 与 WebRTC 逻辑

### Task 1: 实现监控页对讲控制栏 UI
**Files:**
- Modify: `templates/monitor.html`

- [ ] **Step 1: 在 `monitorGrid` 上方添加 HTML 结构**
  包含设备下拉框、开始/结束按钮、状态灯。
- [ ] **Step 2: 编写 JS 逻辑自动填充设备下拉框**
  轮询 `/api/mqtt/devices` 并动态更新 `<select>`。

### Task 2: 实现 WebRTC WHIP 推流逻辑
**Files:**
- Modify: `templates/monitor.html`

- [ ] **Step 1: 编写 `IntercomClient` 类**
  处理 `getUserMedia`、`RTCPeerConnection` 和 WHIP `fetch` 请求。
- [ ] **Step 2: 绑定按钮事件**
  “开始”：先建立 WebRTC 连接，状态变为 `connected` 后调用后端 `/api/intercom/start`。
  “结束”：停止推流，调用后端 `/api/intercom/stop`。

---

## Chunk 3: 系统设置页面更新与验证

### Task 1: 更新系统设置 UI
**Files:**
- Modify: `templates/settings.html`

- [ ] **Step 1: 在 MQTT 配置表单中增加 MediaMTX 设置项**
  让用户可以输入 NAS 的 IP 和端口。

### Task 2: 最终集成测试
- [ ] **Step 1: 配置 MediaMTX IP**
  在 Web 界面保存 NAS 的 IP。
- [ ] **Step 2: 浏览器权限验证**
  指导用户在 Chrome 中开启 `unsafely-treat-insecure-origin-as-secure`。
- [ ] **Step 3: 端到端对讲测试**
  选择设备 -> 开始通话 -> 观察 MediaMTX 日志 -> 验证 Jetson 是否收到指令。
