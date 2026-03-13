# 化工厂危险行为检测系统

基于 Flask 的化工厂危险行为实时检测系统，支持视频监控、危险行为告警、MQTT 消息推送和摄像头管理。

## 功能特性

- **用户认证**：注册、登录、会话管理
- **实时监控**：多摄像头视频流监控
- **危险行为检测**：自动检测化工厂内的危险行为并生成告警
- **告警管理**：查看和管理所有告警记录
- **MQTT 集成**：支持 MQTT 协议推送告警消息
- **摄像头管理**：配置和管理多个监控摄像头
- **设置管理**：系统参数配置

## 技术栈

- **后端**：Flask 3.x + SQLAlchemy 2.0
- **数据库**：MySQL
- **前端**：HTML + Bootstrap 5 + JavaScript
- **消息队列**：MQTT (paho-mqtt)
- **认证**：Flask-Login + Bcrypt

## 项目结构

```
.
├── app.py                 # 应用入口
├── config.py              # 配置文件
├── cameras.json           # 摄像头配置
├── blueprints/           # 蓝图模块
│   ├── __init__.py       # 数据库初始化
│   ├── models.py         # 数据模型
│   ├── main.py           # 主页面路由
│   ├── auth.py           # 认证路由
│   ├── capture.py        # 告警捕获
│   ├── video_stream.py   # 视频流处理
│   ├── mqtt_manager.py   # MQTT 管理
│   └── settings.py       # 设置路由
├── templates/            # HTML 模板
└── static/               # 静态资源
```

## 快速开始

### 环境要求

- Python 3.10+
- MySQL 5.7+
- Conda (推荐)

### 1. 克隆项目

```bash
git clone <repository-url>
cd bishe2
```

### 2. 创建虚拟环境

```bash
conda create -n bishe python=3.10
conda activate bishe
```

### 3. 安装依赖

```bash
pip install Flask==3.1.3 Flask-SQLAlchemy==3.1.1 Flask-Login==0.6.3
pip install Flask-Bcrypt==1.0.1 PyMySQL==1.1.2 SQLAlchemy==2.0.48
pip install paho-mqtt==1.6.1
```

### 4. 配置数据库

```bash
mysql -u root -p<your-password> -e "CREATE DATABASE IF NOT EXISTS bishe;"
```

### 5. 运行应用

```bash
python app.py
```

访问 http://localhost:5000

## 配置说明

### 数据库配置 (config.py)

```python
DB_HOST = '127.0.0.1'
DB_PORT = 3306
DB_USER = 'root'
DB_PASSWORD = 'heweijie'
DB_NAME = 'bishe'
```

### 摄像头配置 (cameras.json)

```json
[
    {
        "id": 1,
        "name": "摄像头1",
        "rtsp_url": "rtsp://example.com/stream1"
    }
]
```

### MQTT 配置

在设置页面配置 MQTT 服务器地址、端口、主题等信息。

## 主要页面

| 路由 | 描述 |
|------|------|
| `/` | 首页/监控面板 |
| `/login` | 用户登录 |
| `/register` | 用户注册 |
| `/monitor` | 实时视频监控 |
| `/alerts` | 告警记录 |
| `/settings` | 系统设置 |

## 开发指南

详细开发规范请参阅 [AGENTS.md](AGENTS.md)

### 添加新功能

1. 在 `blueprints/` 下创建新的蓝图模块
2. 在 `app.py` 中注册蓝图
3. 在 `templates/` 下创建对应的 HTML 模板

### 数据模型

所有数据模型定义在 `blueprints/models.py`，数据库表会在应用启动时自动创建。

## 许可证

MIT License
