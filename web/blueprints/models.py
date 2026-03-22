from . import db
from flask_login import UserMixin
from datetime import datetime

class User(db.Model, UserMixin):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80), unique=True, nullable=False, index=True)
    password = db.Column(db.String(120), nullable=False)
    role = db.Column(db.String(20), default='user') # 'admin' or 'user'

    @property
    def is_super_admin(self):
        return self.role == 'admin'

    @property
    def is_assistant(self):
        return self.role == 'assistant'
    
    @property
    def is_admin(self):
        return self.role in ['admin', 'assistant']

class Capture(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    camera_id = db.Column(db.String(50), nullable=False, index=True)
    location = db.Column(db.String(100), nullable=False, index=True)
    image_path = db.Column(db.String(255), nullable=False)
    thumbnail_path = db.Column(db.String(255))
    violation_type = db.Column(db.String(100))
    capture_time = db.Column(db.DateTime, default=datetime.now, index=True)

class MqttConfig(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    broker = db.Column(db.String(255), nullable=False)
    port = db.Column(db.Integer, default=1883)
    username = db.Column(db.String(100))
    password = db.Column(db.String(100))
    topic_prefix = db.Column(db.String(100), default='jetson/camera')
    mediamtx_whip_port = db.Column(db.Integer, default=8889)
    mediamtx_rtsp_port = db.Column(db.Integer, default=8554)
    is_active = db.Column(db.Boolean, default=True)
    created_at = db.Column(db.DateTime, default=datetime.now)
