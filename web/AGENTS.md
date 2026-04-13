# AGENTS.md - 代码开发与 Agent 协作指南

## 1. 全局规则 (Global Rules)

- **语言限制**: **必须始终使用中文回答所有问题。** (Always answer all questions in Chinese.)
- **项目背景**: 化工厂危险行为检测系统 (Chemical Plant Hazard Detection System)。
- **核心架构**: 基于 Flask 3.1+ 的工厂模式与 Blueprint (蓝图) 模块化架构。

## 2. 环境与运行指令 (Environment & Commands)

### 2.1 开发环境
- **Python 版本**: 3.10+
- **虚拟环境**: Conda 环境 `bishe`
- **数据库**: MySQL 5.7+ (配置详见 `config.py`)

### 2.2 常用指令
- **启动应用**:
  ```bash
  conda activate bishe
  python app.py
  ```
- **数据库初始化**: 
  应用启动时会自动执行 `db.create_all()`。手动创建数据库命令：
  ```bash
  mysql -u root -pheweijie -e "CREATE DATABASE IF NOT EXISTS bishe DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;"
  ```

### 2.3 测试与代码质量 (Testing & Linting)
- **运行所有测试**: `pytest tests/`
- **运行单个测试文件**: `pytest tests/test_auth.py`
- **运行单个测试用例**: `pytest tests/test_auth.py::test_login`
- **代码格式化 (Black)**: `black .`
- **代码规范检查 (Flake8)**: `flake8 . --max-line-length=120`

## 3. 代码风格指南 (Code Style Guidelines)

### 3.1 目录结构与职责
- `app.py`: 应用工厂入口。
- `config.py`: 全局配置（数据库、MQTT、上传路径）。
- `exts.py`: 扩展实例（db, mail, mqtt），用于避免循环导入。
- `blueprints/`: 模块化逻辑。
  - `models.py`: 所有 SQLAlchemy 模型定义。
  - `auth.py`: 登录、注册、RBAC 装饰器。
  - `capture.py`: 告警图片上传 API。
  - `mqtt_manager.py`: MQTT 核心逻辑。

### 3.2 命名规范
- **文件与模块**: `snake_case.py` (如 `video_stream.py`)。
- **类名**: `PascalCase` (如 `MQTTManager`, `User`)。
- **函数与变量**: `snake_case` (如 `init_cameras`, `camera_id`)。
- **常量**: `UPPER_SNAKE_CASE` (如 `UPLOAD_FOLDER`)。
- **蓝图变量**: `snake_case` 且以 `_bp` 结尾 (如 `auth_bp`)。

### 3.3 导入规范 (Import Order)
1. 标准库 (os, json, datetime)。
2. 第三方库 (flask, sqlalchemy, paho.mqtt)。
3. 本地模块 (.models, .auth, exts)。
*组与组之间空一行，禁止循环引用。*

### 3.4 数据库操作
- **事务处理**: 必须包含在 `try-except` 块中，失败时务必执行 `db.session.rollback()`。
- **模型定义**: 统一在 `blueprints/models.py` 中定义，继承自 `db.Model`。

### 3.5 API 设计与 RBAC
- **API 路由**: JSON 接口建议以 `/api/` 为前缀，返回 `jsonify()`。
- **权限控制**: 
  - 使用 `@login_required` 保护需要登录的路由。
  - 使用 `@admin_required` (从 `blueprints.auth` 导入) 保护仅管理员可访问的路由。
  - 模板中使用 `current_user.is_admin` 判断 UI 显示。

### 3.6 前端开发 (Jinja2 & Bootstrap 5)
- **基础布局**: 所有模板必须 `{% extends "base.html" %}`。
- **UI 风格**: 遵循现有的深色主题，使用 Bootstrap 5 样式。
- **静态资源**: 抓拍图片存储在 `static/captures/`，缩略图在 `thumbnails/`。

## 4. 错误处理 (Error Handling)
- **后端报错**: 捕获异常并记录日志，API 返回 `{"error": "message"}` 及 4xx/5xx 状态码。
- **前端反馈**: 使用 Flask 的 `flash(message, category)` 弹出通知。

## 5. Agent 协作约束 (Agent Constraints)

- **严禁直接提交**: 除非用户明确要求，否则不要执行 `git commit`。
- **逻辑严密性**: 在修改模型后，需提醒用户可能需要手动更新数据库表。
- **任务拆解**: 复杂任务前必须使用 `todowrite` 创建任务列表。
- **代码质量**: 提交代码前应运行 `flake8` 检查，确保无语法与风格问题。
- **一致性**: 严格遵循现有的“工厂模式+蓝图”设计模式，不要在 `app.py` 中直接写路由。

---
*注：本文件由 Antigravity (Sisyphus) 根据当前代码库分析生成。*
