# 化工厂危险行为检测系统 (Chemical Plant Hazard Detection System)

基于 Flask 的化工厂危险行为实时检测系统，支持多路视频流监控、危险行为自动抓拍告警、MQTT 远程配置与消息推送、以及多摄像头的统一管理。

## 🌟 功能特性

- **🔒 用户认证系统**：完整的用户注册、登录、会话管理，支持“记住我”功能。
- **📹 实时视频监控**：多摄像头视频流并发监控，支持 WebRTC/RTSP 实时视频流低延迟播放。
- **⚠️ 危险行为检测与告警**：接收并记录远程摄像头/边缘计算节点自动检测到的危险行为（如未佩戴安全帽、抽烟、离岗等）。
- **📊 告警记录管理**：查看和管理所有抓拍记录，支持按时间流展示、图片预览、以及违规数据大屏统计。
- **📡 MQTT 物联网集成**：支持 MQTT 协议，实现系统与边缘摄像头之间的双向通信，推送告警消息。
- **⚙️ 远程设备控制**：支持通过 Web 界面动态配置 MQTT 参数，并下发配置指令（如：检测阈值、IOU阈值、缩放比例）给远程摄像头。
- **📱 响应式设计**：基于 Bootstrap 5 构建的现代化深色主题 UI，兼容桌面与移动设备。

---

## 🛠️ 技术栈

### 后端 (Backend)
- **核心框架**: Flask 3.1+ (采用 Blueprint 模块化架构)
- **数据库**: MySQL 5.7+
- **ORM**: SQLAlchemy 2.0 (Flask-SQLAlchemy 3.1.1)
- **身份认证**: Flask-Login 0.6.3 + Flask-Bcrypt 1.0.1
- **消息队列 (IoT)**: MQTT (paho-mqtt 1.6.1)

### 前端 (Frontend)
- **页面框架**: HTML5 + Jinja2 Templates
- **UI 库**: Bootstrap 5.3
- **图标库**: Font Awesome 6.0
- **交互与图表**: Vanilla JavaScript (ES6+), 原生 Fetch API

---

## 📁 项目结构

```text
bishe2/
├── app.py                      # Flask 应用主入口
├── config.py                   # 核心配置文件（包含数据库、邮箱、MQTT全局配置）
├── cameras.json                # 摄像头默认静态配置文件
├── exts.py                     # Flask 扩展实例库（解决循环导入问题）
├── blueprints/                 # Flask 蓝图模块（MVC 控制器层）
│   ├── __init__.py             # 数据库初始化及 LoginManager 配置
│   ├── models.py               # SQLAlchemy 数据库模型定义 (User, Capture, MqttConfig)
│   ├── main.py                 # 主页面及仪表盘路由
│   ├── auth.py                 # 身份认证（登录/注册/注销）
│   ├── capture.py              # 告警抓拍数据上传及查询 API
│   ├── video_stream.py         # 视频流读取及摄像头状态管理
│   ├── mqtt_manager.py         # MQTT 客户端核心逻辑 (发布/订阅/连接维护)
│   └── settings.py             # 系统设置 (MQTT 配置与下发指令)
├── templates/                  # 视图模板 (Jinja2)
│   ├── base.html               # 全局基础布局 (导航栏, Flash 提示, 页脚)
│   ├── index.html              # 首页仪表盘
│   ├── login.html / register.html # 认证相关页面
│   ├── monitor.html            # 实时视频监控面板
│   ├── alerts.html             # 抓拍告警记录流
│   └── settings.html           # 统一设置中心
├── static/                     # 静态资源文件
│   ├── bootstrap/              # Bootstrap 框架本地缓存
│   ├── css/style.css           # 全局自定义 CSS 样式
│   ├── img/                    # UI 图片资源
│   └── captures/               # 【动态目录】存储远程上传的抓拍违规图片
└── AGENTS.md                   # Agent 开发与代码规范指南
```

---

## 🚀 快速开始

### 1. 环境准备

建议使用 Python 3.10 及以上版本，以及 MySQL 5.7/8.0。推荐使用 Conda 管理虚拟环境。

```bash
# 克隆代码
git clone <repository-url>
cd bishe2

# 创建并激活 Conda 环境
conda create -n bishe python=3.10
conda activate bishe
```

### 2. 安装依赖

```bash
pip install Flask==3.1.3 Flask-SQLAlchemy==3.1.1 Flask-Login==0.6.3
pip install Flask-Bcrypt==1.0.1 PyMySQL==1.1.2 SQLAlchemy==2.0.48
pip install paho-mqtt==1.6.1
```

### 3. 配置数据库

启动 MySQL，并创建对应的数据库：
```bash
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS bishe DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;"
```

**修改 `config.py` 中的数据库连接参数：**
```python
HOSTNAME = '127.0.0.1'      
PORT = 3306                 
USERNAME = 'root'           
PASSWORD = 'your_password'  # 替换为你的 MySQL 密码
DATABASE = 'bishe'        
```

### 4. 运行服务

```bash
python app.py
```

服务将启动在 `http://0.0.0.0:5000`。浏览器访问 `http://localhost:5000` 即可进入系统（首次进入需先注册账号）。数据库表会在首次启动时自动创建。

---

## 🔌 API 文档与边缘端接入

本系统不仅是一个展示前端，同时也是一个轻量级的物联网管理后端，提供以下主要接口供边缘摄像头（如 Jetson Nano、树莓派等运行目标检测模型的设备）调用接入：

### 1. 抓拍图片上传接口
当边缘设备检测到危险行为时，调用此接口上传图片和违规信息。
- **接口**: `POST /capture/upload`
- **Content-Type**: `multipart/form-data`
- **参数**:
  - `file`: (File) 抓拍的图片文件
  - `camera_id`: (String) 摄像头编号
  - `location`: (String) 抓拍地点
  - `violation_type`: (String) 违规类型（如：未佩戴安全帽）

**调用示例 (Python)**:
```python
import requests

url = "http://192.168.1.100:5000/capture/upload"
files = {'file': open('alert.jpg', 'rb')}
data = {
    'camera_id': '001',
    'location': '生产车间A区',
    'violation_type': '未佩戴安全帽'
}
requests.post(url, files=files, data=data)
```

### 2. MQTT 配置指令下发
边缘设备需订阅特定主题，以接收来自本 Web 系统的配置更改指令。
- **下发主题规则**: `{MQTT_TOPIC_PREFIX}/{camera_id}/command` (默认: `jetson/camera/{camera_id}/command`)
- **Payload 示例**:
```json
{
  "type": "parameters",
  "value": {
    "confidence_threshold": 0.7,
    "iou_threshold": 0.5,
    "scale_ratio": 1.0
  }
}
```

---

## 🔧 常见问题 (FAQ)

**Q1: 为什么监控页面看不到视频画面？**
A: 请确保 `cameras.json` 中的 `webrtc_url` 是可访问的视频流服务地址，同时由于浏览器安全策略，某些非本地环境(localhost) 下的 WebRTC 视频流可能需要 HTTPS 支持。

**Q2: 数据库表没有创建？**
A: `app.py` 中通过 `init_db(app)` 和 `db.create_all()` 在应用启动时自动建表。如果未建表，请检查 `config.py` 的密码是否正确，并确保已手动执行 `CREATE DATABASE bishe;` 创建了 Database。

**Q3: 如何清除/修改已保存的 MQTT 连接？**
A: 系统设置页包含连接历史。你可以通过 "断开连接" 并重新添加来更新配置，MQTT 账号信息被持久化保存在 `mqtt_config` 数据库表中。

---

## 💻 开发指南

- 新增功能与代码规范：请务必阅读本项目的 [AGENTS.md](AGENTS.md)。
- 本项目遵循 **Flask 工厂模式和 Blueprint 蓝图分离** 的设计理念，所有新增模块需在 `blueprints/` 目录下创建。
- 模型变更需在 `models.py` 内实现。由于未引入 Alembic/Flask-Migrate，若修改模型字段，需手动更新数据库或清空数据库后重启应用。

---

## 📄 开源许可证

本项目基于 [MIT License](LICENSE) 开源。
