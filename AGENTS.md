# AGENTS.md - Code Guidelines for This Project

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
The app runs on `http://0.0.0.0:5000` in debug mode.

### Database Setup
```bash
mysql -u root -pheweijie -e "CREATE DATABASE IF NOT EXISTS bishe;"
```
Database tables are auto-created when the app starts (via `db.create_all()` in blueprints/__init__.py).

### Dependencies
Required packages (already installed in conda env `bishe`):
- Flask 3.1.3
- Flask-SQLAlchemy 3.1.1
- Flask-Login 0.6.3
- Flask-Bcrypt 1.0.1
- PyMySQL 1.1.2
- SQLAlchemy 2.0.48

## Code Style Guidelines

### 1. Project Structure
```
bishe2/
├── app.py                 # Main application entry point
├── config.py              # Configuration (database, mail, secret key)
├── cameras.json           # Camera configuration file
├── blueprints/
│   ├── __init__.py       # Database and login manager initialization
│   ├── models.py         # SQLAlchemy models (User, Capture)
│   ├── main.py           # Main routes (index, monitor, alerts)
│   ├── auth.py           # Authentication routes (login, register, logout)
│   ├── capture.py        # Capture upload/list routes
│   └── video_stream.py   # Video stream management
├── templates/             # Jinja2 templates
└── static/               # Static files (CSS, JS, images)
```

### 2. Import Conventions
- Standard library imports first
- Third-party imports second
- Local imports third
- Separate with blank lines

```python
# Correct
import os
import json
from datetime import datetime

from flask import Blueprint, render_template, jsonify
from flask_login import login_required, current_user

from .models import User
from . import db
```

### 3. Naming Conventions
- **Files**: snake_case (e.g., `video_stream.py`, `auth.py`)
- **Classes**: PascalCase (e.g., `User`, `Capture`, `VideoCamera`)
- **Functions/Variables**: snake_case (e.g., `init_cameras`, `capture_time`)
- **Constants**: UPPER_SNAKE_CASE (e.g., `MAX_FRAME_RATE`)
- **Blueprint names**: snake_case (e.g., `auth_bp`, `capture_bp`)

### 4. Flask Blueprint Structure
```python
# blueprints/example.py
from flask import Blueprint, render_template, jsonify, request
from . import db
from .models import SomeModel

example_bp = Blueprint('example', __name__, url_prefix='/example')

@example_bp.route('/')
def index():
    return render_template('example.html')

@example_bp.route('/api/data', methods=['GET', 'POST'])
def get_data():
    # Handle request
    return jsonify({'data': 'value'})
```

### 5. Database Models
```python
# In blueprints/models.py
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
- Always wrap database operations in try-except blocks
- Use flash messages for user-facing errors
- Return proper HTTP status codes for API endpoints

```python
try:
    db.session.add(user)
    db.session.commit()
    flash('Success message', 'success')
except Exception as e:
    db.session.rollback()
    flash(f'Error: {str(e)}', 'danger')
```

### 7. HTML Templates
- Use Jinja2 inheritance (`{% extends "base.html" %}`)
- Put page-specific CSS in `{% block extra_css %}`
- Put page-specific JS in `{% block extra_js %}`
- Use Bootstrap classes for styling

### 8. API Design
- RESTful conventions: GET for retrieve, POST for create, DELETE for remove
- Return JSON responses with proper structure
- Include error handling for all endpoints

```python
@api_bp.route('/api/items', methods=['GET'])
def list_items():
    items = Item.query.all()
    return jsonify({
        'items': [{'id': i.id, 'name': i.name} for i in items]
    }), 200
```

### 9. Configuration
- All configuration in `config.py`
- Use environment variables for sensitive data in production
- Database credentials should not be committed to version control

### 10. Routes and URL Building
- When using Blueprints, use `url_for('blueprint_name.function_name')`
- Example: `url_for('auth.login')`, `url_for('main.index')`

## Testing

Currently there are no automated tests in this project. To add tests:
```bash
pip install pytest pytest-flask
pytest tests/           # Run all tests
pytest tests/test_auth.py::test_login  # Run specific test
```

## Common Tasks

### Add a new Blueprint
1. Create `blueprints/new_module.py`
2. Import and register in `app.py`:
```python
from blueprints.new_module import new_bp
app.register_blueprint(new_bp)
```

### Add a new database model
1. Add model class to `blueprints/models.py`
2. Run the app once - tables will be auto-created

### Add a new template
1. Create template in `templates/`
2. Extend from `base.html`
3. Add route in appropriate Blueprint
