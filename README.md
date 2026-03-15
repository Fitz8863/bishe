# 化工厂危险行为检测系统

本项目为**化工厂危险行为检测系统**的完整代码仓库，包含 Web 端管理和边缘端检测两部分。

## 📁 项目结构

```
bishe/
├── web/                      # Web 端 (Flask 后台管理系统)
│   ├── app.py                # Flask 应用入口
│   ├── config.py             # 配置文件
│   ├── blueprints/           # Flask 蓝图模块
│   ├── templates/            # HTML 模板
│   └── static/               # 静态资源
│
├── edge/                     # 边缘端 (Jetson Orin Nano)
│   └── (待添加 Jetson 代码)
│
└── README.md                 # 本文件
```

## 🏗️ 系统架构

```
┌─────────────────┐         ┌─────────────────┐
│   Web 管理系统   │  MQTT   │  Jetson 边缘端  │
│   (web/)        │◄───────►│    (edge/)      │
│                 │  HTTP   │                 │
│  - 用户管理      │◄───────►│  - 目标检测     │
│  - 视频监控      │         │  - 抓拍上传     │
│  - 告警记录      │         │  - MQTT 通信   │
└─────────────────┘         └─────────────────┘
```

## 📂 各模块说明

### web/ - Web 端
基于 Flask 开发的 Web 后台管理系统，提供以下功能：
- 用户认证与权限管理（超级管理员、辅助管理员、普通用户）
- 实时视频监控
- 告警记录管理
- MQTT 配置与远程控制

详细说明见 [web/README.md](web/README.md)

### edge/ - 边缘端
运行在 Jetson Orin Nano 上的目标检测程序，负责：
- 实时视频流分析
- 危险行为检测（未戴安全帽、抽烟等）
- 抓拍图片上传至 Web 端
- 接收 Web 端下发的配置指令

(代码待添加)

---

## 🚀 快速开始

### Web 端运行
```bash
cd web
pip install -r requirements.txt
python app.py
```

### Edge 端运行
(待补充)

---

## 📝 许可证

MIT License
