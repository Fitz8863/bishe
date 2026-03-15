import os
import uuid
from datetime import datetime
from flask import Blueprint, request, jsonify, render_template
from flask_login import login_required
from werkzeug.utils import secure_filename
from . import db
from .models import Capture
from .auth import admin_required

capture_bp = Blueprint('capture', __name__, url_prefix='/capture')

UPLOAD_FOLDER = 'static/captures'
ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'gif'}

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

@capture_bp.route('/upload', methods=['POST'])
def upload_capture():
    """接收远程摄像头上传的抓拍图片"""
    if 'file' not in request.files:
        return jsonify({'error': 'No file part'}), 400
    
    file = request.files['file']
    if file.filename == '':
        return jsonify({'error': 'No selected file'}), 400
    
    camera_id = request.form.get('camera_id', 'unknown')
    location = request.form.get('location', '未知区域')
    violation_type = request.form.get('violation_type', '违规行为')
    
    if file and allowed_file(file.filename):
        filename = secure_filename(f"{uuid.uuid4().hex}_{file.filename}")
        
        upload_path = os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 
            'static', 'captures'
        )
        os.makedirs(upload_path, exist_ok=True)
        
        filepath = os.path.join(upload_path, filename)
        file.save(filepath)
        
        capture = Capture(
            camera_id=camera_id,
            location=location,
            image_path=f"captures/{filename}",
            violation_type=violation_type,
            capture_time=datetime.now()
        )
        db.session.add(capture)
        db.session.commit()
        
        return jsonify({
            'message': 'Capture uploaded successfully',
            'capture_id': capture.id
        }), 200
    
    return jsonify({'error': 'Invalid file type'}), 400

@capture_bp.route('/list', methods=['GET'])
@login_required
@admin_required
def list_captures():
    """获取抓拍记录列表"""
    captures = Capture.query.order_by(Capture.capture_time.desc()).all()
    return jsonify({
        'captures': [{
            'id': c.id,
            'camera_id': c.camera_id,
            'location': c.location,
            'image_path': c.image_path,
            'violation_type': c.violation_type,
            'capture_time': c.capture_time.strftime('%Y-%m-%d %H:%M:%S')
        } for c in captures]
    }), 200

@capture_bp.route('/delete/<int:capture_id>', methods=['DELETE'])
@login_required
@admin_required
def delete_capture(capture_id):
    """删除抓拍记录"""
    capture = Capture.query.get(capture_id)
    if not capture:
        return jsonify({'error': 'Capture not found'}), 404
    
    image_path = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 
        'static', capture.image_path
    )
    if os.path.exists(image_path):
        os.remove(image_path)
    
    db.session.delete(capture)
    db.session.commit()
    
    return jsonify({'message': 'Capture deleted successfully'}), 200
