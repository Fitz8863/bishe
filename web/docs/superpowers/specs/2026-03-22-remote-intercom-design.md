# 远程语音对讲功能设计规范 (Remote Intercom Design Spec)

- **日期**: 2026-03-22
- **状态**: 草案 (Draft)
- **负责人**: Antigravity

## 1. 业务目标
为监控系统增加实时语音对讲功能，允许管理员从浏览器向指定的边缘设备（Jetson）发送实时语音流，用于远程指挥和应急处理。

## 2. 技术架构 (Architecture)
采用 **方案 A: WebRTC WHIP** 以获得极致的低延迟体验。

- **Frontend (Browser)**: 使用 `navigator.mediaDevices.getUserMedia` 采集音频。使用 `RTCPeerConnection` 建立 WebRTC 连接，并通过 **WHIP (WebRTC-HTTP Ingestion Protocol)** 将 SDP 推送到 MediaMTX。
- **Streaming Server (MediaMTX)**: 接收 WebRTC WHIP 推流，并将其转换为 RTSP 流供设备拉取。
- **Edge Device (Jetson)**: 接收来自 Web 端的 MQTT 指令，从 MediaMTX 拉取对应的 RTSP 音频流进行播放。
- **Backend (Flask)**: 提供在线设备列表 API，并负责下发 MQTT 对讲控制指令。

## 3. 详细组件设计

### 3.1 用户界面 (UI)
- **位置**: `monitor.html` 顶部监控网格上方。
- **控件**:
    - **设备下拉框**: 动态加载当前在线的 MQTT 设备列表。
    - **开始通话按钮**: 触发麦克风授权 -> 建立 WebRTC 连接 -> 发送 MQTT `intercom_start`。
    - **结束通话按钮**: 关闭 WebRTC 连接 -> 发送 MQTT `intercom_stop`。
    - **状态指示灯**: 灰色(等待)、黄色(连接中)、红色(对讲中)、深红(错误)。

### 3.2 数据流逻辑 (Data Flow)
1.  **设备发现**: 前端调用 `/api/mqtt/devices` 获取在线设备（从 `mqtt_manager.devices` 获取）。
2.  **建立通话**:
    - 前端生成本地 SDP，发送 `POST` 请求到 MediaMTX WHIP 接口：`http://<MEDIAMTX_IP>:8889/intercom_<device_id>/whip`。
    - 同时，前端调用后端 API 下发 MQTT 指令：`{ "type": "intercom_start", "url": "rtsp://<MEDIAMTX_IP>:8554/intercom_<device_id>" }`。
3.  **音频播放**: Jetson 收到指令后，启动拉流播放器（如 GStreamer 或 ffplay）。
4.  **断开通话**: 前端停止推流，下发 MQTT 停止指令。

### 3.3 系统配置
- **新增配置项**: `MEDIAMTX_IP` (NAS 的局域网 IP)，可在“系统设置”页面配置，默认为空。

## 4. 异常处理
- **麦克风权限**: 若浏览器被拒绝权限，显示明确提示并引导用户开启“白名单”。
- **MediaMTX 离线**: 3 秒内未建立 WebRTC 连接则触发超时提示。
- **MQTT 延迟**: 若指令下发失败，前端将自动关闭推流。

## 5. 验收标准
1.  下拉框能准确显示在线的 Jetson 设备名称。
2.  点击“开始通话”后，MediaMTX 控制台能看到新增的 WebRTC 路径。
3.  延迟控制在 500ms 以内。
4.  点击“结束通话”能立即停止推流并重置 UI。
