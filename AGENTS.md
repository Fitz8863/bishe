# AGENTS.md - Code Guidelines for This Project

## 全局规则

**必须始终使用中文回答所有问题。**

## Project Overview

- **Project Name**: 化工厂危险行为检测系统 (Chemical Plant Hazard Detection System)
- **Framework**: Flask 3.x with Blueprint architecture
- **Database**: MySQL (host: 127.0.0.1, port: 3306, user: root, password: heweijie, db: bishe)
- **Frontend**: HTML + Bootstrap 5 + JavaScript
- **Python Version**: 3.10

## Build / Run Commands

### Start the Application
```bash
cd /home/fitz/projects/bishe2
python app.py
```
App runs on `http://0.0.0.0:5000` in debug mode.

### Database Setup
```bash
mysql -u root -pheweijie -e "CREATE DATABASE IF NOT EXISTS bishe;"
```
Tables auto-created on app startup via `db.create_all()` in `blueprints/__init__.py`.

### Dependencies (conda env: bishe)
- Flask 3.1.3, Flask-SQLAlchemy 3.1.1, Flask-Login 0.6.3
- Flask-Bcrypt 1.0.1, PyMySQL 1.1.2, SQLAlchemy 2.0.48, paho-mqtt 1.6.1

### Testing
No automated tests. To add:
```bash
pip install pytest pytest-flask
pytest tests/                    # Run all tests
pytest tests/test_auth.py::test_login  # Run single test
```

## Code Style

### 1. Structure
```
app.py, config.py, cameras.json
blueprints/: __init__.py, models.py, main.py, auth.py, capture.py, video_stream.py, mqtt_manager.py, settings.py
templates/, static/
```

### 2. Import Order
1. Standard library (os, json, datetime)
2. Third-party (flask, sqlalchemy)
3. Local (.models, . import)
Separate groups with blank lines

```python
import os
import json
from datetime import datetime

from flask import Blueprint, render_template, jsonify, request
from flask_login import login_required, current_user

from .models import User
from . import db
```

### 3. Naming Conventions
- **Files**: snake_case (`video_stream.py`, `auth.py`)
- **Classes**: PascalCase (`User`, `Capture`, `VideoCamera`)
- **Functions/Variables**: snake_case (`init_cameras`, `capture_time`)
- **Constants**: UPPER_SNAKE_CASE (`MAX_FRAME_RATE`)
- **Blueprints**: snake_case with `_bp` suffix (`auth_bp`, `capture_bp`)

### 4. Blueprint Structure
```python
from flask import Blueprint, render_template, jsonify, request
from . import db
from .models import SomeModel

example_bp = Blueprint('example', __name__, url_prefix='/example')

@example_bp.route('/')
def index():
    return render_template('example.html')

@example_bp.route('/api/data', methods=['GET', 'POST'])
def get_data():
    return jsonify({'data': 'value'})
```

### 5. Database Models
```python
from . import db
from flask_login import UserMixin
from datetime import datetime

class User(db.Model, UserMixin):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80), unique=True, nullable=False)
    password = db.Column(db.String(120), nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.now)
```

### 6. Error Handling
Wrap DB operations in try-except with rollback. Use flash messages for errors.
```python
try:
    user = User.query.get(user_id)
    db.session.delete(user)
    db.session.commit()
except Exception as e:
    db.session.rollback()
    flash(f'操作失败: {str(e)}', 'danger')
```

### 7. Type Hints
Use when beneficial: `def get_user(user_id: int) -> User | None:`

### 8. Formatting
- 4 spaces indentation (no tabs), max 120 chars per line
- Trailing commas in multi-line imports, use f-strings

### 9. HTML Templates
- Use Jinja2 inheritance (`{% extends "base.html" %}`)
- Page CSS in `{% block extra_css %}`, JS in `{% block extra_js %}`
- Use Bootstrap classes

### 10. API Design & Routes
- RESTful: GET=retrieve, POST=create, DELETE=remove
- Return JSON with consistent structure
- Use `url_for('blueprint.function')`: `url_for('auth.login')`, `url_for('main.index')`
```python
return jsonify({'items': [{'id': i.id, 'name': i.name} for i in items]}), 200
```

### 11. Configuration
All config in `config.py`. Use environment variables in production. Never commit secrets.

## Common Tasks

### Add Blueprint
1. Create `blueprints/new_module.py`
2. Register in `app.py`:
```python
from blueprints.new_module import new_bp
app.register_blueprint(new_bp)
```

### Add Model
1. Add class to `blueprints/models.py`
2. Run app once - tables auto-created

### Add Template
1. Create in `templates/`
2. Extend from `base.html`
3. Add route in appropriate Blueprint
