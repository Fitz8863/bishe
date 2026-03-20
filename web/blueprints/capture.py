import os
import uuid
from datetime import datetime
from flask import Blueprint, request, jsonify, render_template
from flask_login import login_required, current_user
from werkzeug.utils import secure_filename
from PIL import Image
from . import db
from .models import Capture
from .auth import admin_required
from exts import socketio

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

    # 生成缩略图
    thumbnail_filename = f"thumb_{filename}"
    thumbnail_dir = os.path.join(upload_path, 'thumbnails')
    os.makedirs(thumbnail_dir, exist_ok=True)
    thumbnail_path = os.path.join(thumbnail_dir, thumbnail_filename)

    try:
        with Image.open(filepath) as img:
            # 计算高度以保持宽高比，最大宽度300px
            width, height = img.size
            if width > 300:
                ratio = 300 / width
                new_height = int(height * ratio)
                img = img.resize((300, new_height), Image.Resampling.LANCZOS)
            img.save(thumbnail_path)
    except Exception as e:
        # 如果缩略图生成失败，记录错误但不中断上传流程
        print(f"缩略图生成失败: {str(e)}")
        thumbnail_filename = None

    # 保存到数据库
    capture = Capture(
        camera_id=camera_id,
        location=location,
        image_path=f"captures/{filename}",
        thumbnail_path=f"captures/thumbnails/{thumbnail_filename}" if thumbnail_filename else None,
        violation_type=violation_type,
        capture_time=datetime.now()
    )
    db.session.add(capture)
    db.session.commit()

    # 发送Socket.IO事件
    socketio.emit('new_capture', {
        'camera_id': camera_id,
        'location': location,
        'violation_type': violation_type,
        'capture_time': capture.capture_time.strftime('%Y-%m-%d %H:%M:%S'),
        'thumbnail': f"/static/captures/thumbnails/{thumbnail_filename}" if thumbnail_filename else None
    }, namespace='/')

    return jsonify({'code': 200, 'message': '上传成功'}), 200

from sqlalchemy import func

@capture_bp.route('/api/stats', methods=['GET'])
@login_required
@admin_required
def get_stats():
    """获取统计数据用于图表展示"""
    # 1. 违规类型分布
    type_stats = db.session.query(
        Capture.violation_type, 
        func.count(Capture.id)
    ).group_by(Capture.violation_type).all()
    
    # 2. 拍摄地点分布 (取前5名)
    location_stats = db.session.query(
        Capture.location, 
        func.count(Capture.id)
    ).group_by(Capture.location).order_by(func.count(Capture.id).desc()).limit(5).all()
    
    # 3. 最近7天的趋势
    from datetime import timedelta
    seven_days_ago = datetime.now() - timedelta(days=7)
    daily_stats = db.session.query(
        func.date(Capture.capture_time).label('date'),
        func.count(Capture.id)
    ).filter(Capture.capture_time >= seven_days_ago).group_by(func.date(Capture.capture_time)).all()
    
    return jsonify({
        'code': 200,
        'types': {t[0] if t[0] else '其他': t[1] for t in type_stats},
        'locations': {l[0]: l[1] for l in location_stats},
        'trends': {str(d[0]): d[1] for d in daily_stats}
    }), 200

@capture_bp.route('/list', methods=['GET'])
@login_required
@admin_required
def list_captures():
    """获取抓拍记录列表，支持筛选和分页"""
    page = request.args.get('page', 1, type=int)
    per_page = request.args.get('per_page', 30, type=int)
    
    # 获取筛选参数
    camera_id = request.args.get('camera_id')
    location = request.args.get('location')
    start_time = request.args.get('start_time')
    end_time = request.args.get('end_time')
    
    query = Capture.query
    
    # 应用筛选条件
    if camera_id:
        query = query.filter(Capture.camera_id.like(f"%{camera_id}%"))
    if location:
        query = query.filter(Capture.location.like(f"%{location}%"))
    if start_time:
        try:
            start_dt = datetime.strptime(start_time, '%Y-%m-%dT%H:%M')
            query = query.filter(Capture.capture_time >= start_dt)
        except ValueError:
            pass
    if end_time:
        try:
            end_dt = datetime.strptime(end_time, '%Y-%m-%dT%H:%M')
            query = query.filter(Capture.capture_time <= end_dt)
        except ValueError:
            pass
            
    # 按时间降序排列
    query = query.order_by(Capture.capture_time.desc())
    
    # 执行分页
    pagination = query.paginate(page=page, per_page=per_page, error_out=False)
    captures = pagination.items
    
    return jsonify({
        'code': 200,
        'captures': [{
            'id': c.id,
            'camera_id': c.camera_id,
            'location': c.location,
            'image_path': c.image_path,
            'thumbnail_path': c.thumbnail_path,
            'violation_type': c.violation_type,
            'capture_time': c.capture_time.strftime('%Y-%m-%d %H:%M:%S')
        } for c in captures],
        'pagination': {
            'total': pagination.total,
            'pages': pagination.pages,
            'current_page': pagination.page,
            'per_page': pagination.per_page,
            'has_next': pagination.has_next,
            'has_prev': pagination.has_prev
        }
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
