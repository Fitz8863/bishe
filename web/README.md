# 化工厂危险行为检测系统 (Chemical Plant Hazard Detection System)

基于 Flask 的化工厂危险行为实时检测系统，支持多路视频流监控、危险行为自动抓拍告警、MQTT 远程配置与消息推送、以及多摄像头的统一管理。

## 功能特性

### 核心功能
- **用户认证系统**：完整的用户注册、登录、会话管理，支持"记住我"功能。
- **实时视频监控**：多摄像头视频流并发监控，支持 WebRTC/RTSP 实时视频流低延迟播放。
- **危险行为检测与告警**：接收并记录远程摄像头/边缘计算节点自动检测到的危险行为（如未佩戴安全帽、抽烟、离岗等）。
- **告警记录管理**：查看和管理所有抓拍记录，支持按时间流展示、图片预览、以及违规数据大屏统计。
- **MQTT 物联网集成**：支持 MQTT 协议，实现系统与边缘摄像头之间的双向通信，推送告警消息。
- **远程设备控制**：支持通过 Web 界面动态配置 MQTT 参数，并下发配置指令（如：检测阈值、IOU阈值、缩放比例）给远程摄像头。
- **舵机远程控制**：支持通过 D-Pad 方向键和滑块控制摄像头下的舵机设备（COL: -8~8, ROW: -10~10）。
- **火灾警报联动**：监听 MQTT `jetson/alarm` 主题，收到 `alarm_type: "fire"` 时前端播放警报音频。
- **响应式设计**：基于 Bootstrap 5 构建的现代化深色主题 UI，兼容桌面与移动设备。

### 技术栈

#### 后端
- 核心框架: Flask 3.1+ (采用 Blueprint 模块化架构)
- 数据库: MySQL 5.7+
- ORM: SQLAlchemy 2.0 (Flask-SQLAlchemy 3.1.1)
- 身份认证: Flask-Login 0.6.3 + Flask-Bcrypt 1.0.1
- 消息队列 (IoT): MQTT (paho-mqtt 1.6.1)
- 实时通信: Flask-SocketIO 5.x

#### 前端
- 页面框架: HTML5 + Jinja2 Templates
- UI 库: Bootstrap 5.3
- 图标库: Font Awesome 6.0
- 交互: Vanilla JavaScript (ES6+), 原生 Fetch API, SocketIO Client

---

## 项目结构

```
bishe/
├── app.py                      # Flask 应用主入口
├── config.py                   # 核心配置文件
├── exts.py                    # Flask 扩展实例库
├── cameras.json               # 摄像头静态配置
├── requirements.txt           # Python 依赖
├── README.md                 # 项目文档
├── AGENTS.md                # Agent 开发规范
├── blueprints/              # Flask 蓝图模块
│   ├── __init__.py          # 数据库初始化
│   ├── models.py            # 数据模型 (User, Capture, MqttConfig)
│   ├── main.py            # 主页面路由
│   ├── auth.py           # 认证 (登录/注册/注销)
│   ├── capture.py        # 告警抓拍上传 API
│   ├── video_stream.py  # 视频流管理
│   ├── mqtt_manager.py # MQTT 客户端核心逻辑
│   ├── settings.py      # 系统设置
│   └── user_management.py # 用户管理
├── templates/               # Jinja2 模板
│   ├── base.html        # 全局布局 (含报警系统)
│   ├── index.html    # 首页
│   ├── login.html  # 登录
│   ├── register.html # 注册
│   ├── monitor.html # 实时监控 (含舵机控制)
│   ├── alerts.html # 告警记录
│   └── settings.html # 系统设置
├── static/                # 静态资源
│   ├── bootstrap/     # Bootstrap 本地
│   ├── css/style.css # 样式
│   ├── captures/    # 抓拍图片
│   ├── thumbnails/ # 缩略图
│   ├── alarm/     # 警报音频
│   └── image/    # UI 图片
└── tests/               # 测试 (可选)
```

---

## 快速开始

### 1. 环境准备

```bash
# 创建 Conda 环境
conda create -n bishe python=3.10
conda activate bishe

# 安装依赖
pip install -r requirements.txt
```

### 2. 数据库配置

```bash
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS bishe DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;"
```

修改 `config.py` 中的数据库连接参数。

### 3. 运行服务

```bash
python app.py
```

服务启动在 `http://0.0.0.0:5001`。

---

## MQTT 主题说明

| 主题 | 方向 | 说明 |
|-----|------|-----|
| `jetson/info` | 上行 | 边缘设备心跳 (多设备) |
| `jetson/esp8266/info` | 上行 | 舵机设备心跳 |
| `jetson/alarm` | 上行 | 远程告警 (含 alarm_type) |
| `jetson/camera/{id}/command` | 下行 | 配置指令下发 |
| `jetson/esp8266/cmd` | 下行 | 舵机控制指令 |

### 舵机控制

发送 JSON 到 `jetson/esp8266/cmd`:
```json
{"id": "001", "col": 5, "row": 3}
```

### 火灾警报

边缘设备发送 JSON 到 `jetson/alarm`:
```json
{"alarm_type": "fire", "camera_id": "001", "location": "生产车间A区"}
```

系统会通过 SocketIO 向前端广播 `fire_alarm` 事件，前端播放 `/static/alarm/alarm.mp3` 三次。

---

## 常见问题

### Q: 为什么看不到视频画面？
A: 检查 `cameras.json` 中的 `webrtc_url` 是否可访问。WebRTC 可能需要 HTTPS。

### Q: MQTT 连接成功但收不到消息？
A: 确保已卸载 eventlet (`pip uninstall eventlet greenlet`)，否则 SocketIO 事件可能无法从 MQTT 线程正确广播。

### Q: 数据库表未创建？
A: 检查 `config.py` 密码是否正确，并确保已执行 `CREATE DATABASE bishe;`。

---

## 开发规范

- 遵循 Flask 工厂模式 + Blueprint 蓝图分离
- 新增模块在 `blueprints/` 下创建
- 模型变更需手动更新数据库

---

## 许可证

MIT License