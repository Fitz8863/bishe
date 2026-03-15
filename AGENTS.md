# AGENTS.md - Code Guidelines for This Project

## 全局规则 (Global Rules)

**必须始终使用中文回答所有问题。** (Always answer all questions in Chinese.)

## 1. Project Overview

- **Project Name**: 化工厂危险行为检测系统 (Chemical Plant Hazard Detection System)
- **Framework**: Flask 3.1+ with Blueprint architecture
- **Database**: MySQL (host: 127.0.0.1, port: 3306, user: root, password: heweijie, db: bishe) via SQLAlchemy 2.0
- **Frontend**: HTML + Bootstrap 5 + JavaScript (Jinja2 templates)
- **Message Queue**: MQTT (paho-mqtt) for camera commands
- **Python Version**: 3.10+
- **Environment**: Conda env named `bishe`

## 2. Build / Run Commands

### Start the Application
```bash
cd /home/fitz/projects/bishe2
conda activate bishe
python app.py
```
App runs on `http://0.0.0.0:5000` in debug mode.

### Database Setup
Tables are auto-created on app startup via `db.create_all()` in `blueprints/__init__.py`.
To initialize manually:
```bash
mysql -u root -pheweijie -e "CREATE DATABASE IF NOT EXISTS bishe DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;"
```

## 3. Testing & Linting

Currently, there are no automated tests. When adding tests, follow these conventions:

### Testing Commands
```bash
# Install test dependencies
pip install pytest pytest-flask

# Run all tests
pytest tests/

# Run a specific test file
pytest tests/test_auth.py

# Run a single test case
pytest tests/test_auth.py::test_login
```

### Linting Commands
```bash
# Install linters
pip install flake8 black

# Run Black for formatting
black .

# Run Flake8 for linting
flake8 . --max-line-length=120
```

## 4. Code Style Guidelines

### 4.1 Structure
```
app.py                 # Application entry point
config.py              # Configuration variables
exts.py                # Extensions (db, mail, mqtt) to avoid circular imports
cameras.json           # Camera configurations
blueprints/            # Modular routes and logic
    ├── __init__.py    # DB & Login manager init
    ├── models.py      # SQLAlchemy models
    ├── main.py        # Core UI routes
    ├── auth.py        # Authentication
    ├── capture.py     # Image capture logic
    ├── mqtt_manager.py# MQTT connection and publishing
    ├── settings.py    # UI/API for configurations
    └── video_stream.py# WebRTC/RTSP handling
templates/             # Jinja2 HTML files
static/                # CSS, JS, Images, and capture uploads
```

### 4.2 Import Order
1. Standard library (`os`, `json`, `datetime`, `uuid`)
2. Third-party (`flask`, `sqlalchemy`, `paho.mqtt`)
3. Local application imports (`.models`, `.`, `exts`)

Separate each group with a blank line.

```python
import os
import json
from datetime import datetime

from flask import Blueprint, render_template, jsonify, request
from flask_login import login_required, current_user

from .models import User
from . import db
```

### 4.3 Naming Conventions
- **Files/Modules**: `snake_case.py` (e.g., `video_stream.py`)
- **Classes**: `PascalCase` (e.g., `MQTTManager`, `User`, `Capture`)
- **Functions/Variables**: `snake_case` (e.g., `init_cameras`, `camera_id`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `UPLOAD_FOLDER`, `ALLOWED_EXTENSIONS`)
- **Blueprints**: `snake_case` with `_bp` suffix (e.g., `capture_bp = Blueprint(...)`)

### 4.4 Blueprint Structure & API Design
- Blueprint prefix: Use `url_prefix` for API groups (e.g., `url_prefix='/settings'`).
- API Endpoints: Prefix JSON endpoints with `/api/` (e.g., `/api/mqtt/status`).
- Returns: UI routes return `render_template()`; API routes return `jsonify()` with appropriate HTTP status codes (200, 400, 404, 500).

```python
@capture_bp.route('/list', methods=['GET'])
def list_captures():
    captures = Capture.query.order_by(Capture.capture_time.desc()).all()
    return jsonify({
        'captures': [{'id': c.id, 'location': c.location} for c in captures]
    }), 200
```

### 4.5 Database Models
Define models in `blueprints/models.py`. Inherit from `db.Model`.
```python
from . import db
from flask_login import UserMixin
from datetime import datetime

class User(db.Model, UserMixin):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80), unique=True, nullable=False)
    password = db.Column(db.String(120), nullable=False)
    role = db.Column(db.String(20), default='user') # 'admin' or 'user'

    @property
    def is_admin(self):
        return self.role == 'admin'

class MqttConfig(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    broker = db.Column(db.String(255), nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.now)

### 4.6 Error Handling
- **Database Operations**: Wrap in `try-except` and always use `db.session.rollback()` on failure.
- **UI Errors**: Use Flask's `flash(message, category)` (categories: 'success', 'danger').
- **API Errors**: Return JSON with an `error` key and standard HTTP status code.

```python
try:
    db.session.add(new_record)
    db.session.commit()
    return jsonify({'message': 'Success'}), 200
except Exception as e:
    db.session.rollback()
    return jsonify({'error': str(e)}), 500
```

### 4.7 Formatting & Type Hints
- Use 4 spaces for indentation (no tabs).
- Maximum line length is 120 characters.
- Use type hints for function signatures where it adds clarity: `def get_camera_info(camera_id: str) -> dict:`
- Use f-strings for string formatting: `f"Error: {str(e)}"`

### 4.8 HTML Templates (Jinja2)
- Always inherit from `base.html`: `{% extends "base.html" %}`
- Override blocks: `{% block content %}`, `{% block extra_css %}`, `{% block extra_js %}`
- Use Bootstrap 5 classes for layout and styling.

### 4.9 Configuration
- Do not hardcode sensitive information.
- Store configurations in `config.py` (e.g. `DB_URI`, `MAIL_SERVER`).
- MQTT configuration is stored in the database (`MqttConfig` model) and managed via `/settings`.
- Camera streams are managed in `cameras.json` or dynamically loaded.

### 4.10 Role-Based Access Control (RBAC)

- **Roles**: Users are divided into `admin` and `user`.
- **Default Role**: All new registrations default to `user`.
- **Protection**: Use `@admin_required` decorator (imported from `blueprints.auth`) for routes that should only be accessible by administrators.
- **Admin-only Sections**: System Settings, Video Monitor, and Alert Records are restricted to admins.
- **Template Logic**: Use `current_user.is_admin` to conditionally show/hide UI elements.

```python
from .auth import admin_required

@main_bp.route('/admin_feature')
@login_required
@admin_required
def admin_feature():
    return render_template('admin.html')
```

## 6. Agent Instructions

- Avoid circular imports by leveraging `exts.py` or localizing imports inside functions if necessary.
- Before committing, verify your changes using `flake8` and `pytest` (if tests exist).
- If asked to implement a feature, always create a `TodoWrite` list first to break down the task.
- Follow the existing Flask factory/Blueprint pattern rigidly.
