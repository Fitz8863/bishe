from flask import Blueprint, render_template, jsonify
from flask_login import login_required, current_user
from . import video_stream
from .auth import admin_required

main_bp = Blueprint('main', __name__)

@main_bp.route('/')
def index():
    if current_user.is_authenticated and not current_user.is_admin:
        return render_template('user_index.html')
    return render_template('index.html')

@main_bp.route('/monitor')
@login_required
@admin_required
def monitor():
    return render_template('monitor.html')

@main_bp.route('/alerts')
@login_required
@admin_required
def alerts():
    return render_template('alerts.html')

@main_bp.route('/api/cameras')
@login_required
@admin_required
def list_cameras():
    """获取摄像头列表"""
    return video_stream.list_cameras()

# @main_bp.route('/api/cameras/<camera_id>')
# @login_required
# @admin_required
# def get_camera(camera_id):
#     """获取单个摄像头信息"""
#     return video_stream.get_camera_info(camera_id)
