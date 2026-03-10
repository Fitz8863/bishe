from flask import Blueprint, render_template, jsonify
from . import video_stream

main_bp = Blueprint('main', __name__)

@main_bp.route('/')
def index():
    return render_template('index.html')

@main_bp.route('/monitor')
def monitor():
    return render_template('monitor.html')

@main_bp.route('/alerts')
def alerts():
    return render_template('alerts.html')

@main_bp.route('/api/cameras')
def list_cameras():
    """获取摄像头列表"""
    return video_stream.list_cameras()

@main_bp.route('/api/cameras/<camera_id>')
def get_camera(camera_id):
    """获取单个摄像头信息"""
    return video_stream.get_camera_info(camera_id)
