import os
import uuid
from datetime import datetime
from flask import Blueprint, request, jsonify, render_template
from flask_login import login_required, current_user
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
    # 验证文件
    if 'file' not in request.files:
        return jsonify({'code': 400, 'message': '缺少文件参数'}), 400
    
    file = request.files['file']
    if file.filename == '':
        return jsonify({'code': 400, 'message': '未选择文件'}), 400
    
    # 验证必填字段
    camera_id = request.form.get('camera_id')
    location = request.form.get('location')
    violation_type = request.form.get('violation_type')
    
    if not camera_id:
        return jsonify({'code': 400, 'message': '缺少 camera_id 参数'}), 400
    if not location:
        return jsonify({'code': 400, 'message': '缺少 location 参数'}), 400
    if not violation_type:
        return jsonify({'code': 400, 'message': '缺少 violation_type 参数'}), 400
    
    # 验证文件类型
    if not allowed_file(file.filename):
        return jsonify({'code': 400, 'message': '不支持的文件格式'}), 400
    
    # 保存文件
    filename = secure_filename(f"{uuid.uuid4().hex}_{file.filename}")
    upload_path = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        'static', 'captures'
    )
    os.makedirs(upload_path, exist_ok=True)
    
    filepath = os.path.join(upload_path, filename)
    file.save(filepath)
    
    # 保存到数据库
    capture = Capture(
        camera_id=camera_id,
        location=location,
        image_path=f"captures/{filename}",
        violation_type=violation_type,
        capture_time=datetime.now()
    )
    db.session.add(capture)
    db.session.commit()
    
    return jsonify({'code': 200, 'message': '上传成功'}), 200

@capture_bp.route('/list', methods=['GET'])
@login_required
@admin_required
def list_captures():
    """获取抓拍记录列表"""
    captures = Capture.query.order_by(Capture.capture_time.desc()).all()
    return jsonify({
        'code': 200,
        'captures': [{
            'id': c.id,
            'camera_id': c.camera_id,
            'location': c.location,
            'image_path': c.image_path,
            'violation_type': c.violation_type,
            'capture_time': c.capture_time.strftime('%Y-%m-%d %H:%M:%S')
        } for c in captures]
    }), 200

@capture_bp.route('/delete/<int:capture_id>', methods=['POST'])
@login_required
@admin_required
def delete_capture(capture_id):
    """删除抓拍记录，需要管理员密码验证"""
    data = request.get_json()
    password = data.get('password')
    
    if not password:
        return jsonify({
            'code': 400,
            'error': '请输入管理员密码'
        }), 400
    
    from flask_bcrypt import Bcrypt
    from flask import current_app
    bcrypt = Bcrypt(current_app._get_current_object())
    
    # 验证当前登录用户的密码
    if not bcrypt.check_password_hash(current_user.password, password):
        return jsonify({
            'code': 403,
            'error': '密码错误，无权删除'
        }), 403

    capture = Capture.query.get(capture_id)
    if not capture:
        return jsonify({
            'code': 404,
            'error': '记录不存在'
        }), 404

    try:
        image_path = os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            'static', capture.image_path
        )
        if os.path.exists(image_path):
            os.remove(image_path)

        db.session.delete(capture)
        db.session.commit()

        return jsonify({
            'code': 200,
            'message': '记录删除成功'
        }), 200
    except Exception as e:
        db.session.rollback()
        return jsonify({
            'code': 500,
            'error': f'删除失败: {str(e)}'
        }), 500
