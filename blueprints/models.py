from . import db
from flask_login import UserMixin
from datetime import datetime

class User(db.Model, UserMixin):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80), unique=True, nullable=False)
    password = db.Column(db.String(120), nullable=False)

class Capture(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    camera_id = db.Column(db.String(50), nullable=False)
    location = db.Column(db.String(100), nullable=False)
    image_path = db.Column(db.String(255), nullable=False)
    violation_type = db.Column(db.String(100))
    capture_time = db.Column(db.DateTime, default=datetime.now)

class MqttConfig(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    broker = db.Column(db.String(255), nullable=False)
    port = db.Column(db.Integer, default=1883)
    username = db.Column(db.String(100))
    password = db.Column(db.String(100))
    topic_prefix = db.Column(db.String(100), default='factory/camera')
    is_active = db.Column(db.Boolean, default=True)
    created_at = db.Column(db.DateTime, default=datetime.now)
