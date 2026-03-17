from flask import Blueprint, render_template, request, jsonify, make_response
from flask_login import login_required
from . import db
from .models import MqttConfig
from .auth import admin_required

settings_bp = Blueprint('settings', __name__, url_prefix='/settings')

@settings_bp.before_request
@login_required
@admin_required
def before_request():
    pass

@settings_bp.route('/')
def index():
    """系统设置页面"""
    return render_template('settings.html')

@settings_bp.route('/api/mqtt/status', methods=['GET'])
def get_mqtt_status():
    """获取MQTT连接状态"""
    try:
        from blueprints.mqtt_manager import mqtt_manager
        if mqtt_manager and mqtt_manager.broker:
            return jsonify({
                'connected': mqtt_manager.connected,
                'broker': mqtt_manager.broker,
                'port': mqtt_manager.port
            }), 200
        return jsonify({'connected': False}), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@settings_bp.route('/api/mqtt/connect', methods=['POST'])
def mqtt_connect():
    """连接MQTT"""
    data = request.json
    broker = data.get('broker')
    port = data.get('port', 1883)
    username = data.get('username', '')
    password = data.get('password', '')
    save = data.get('save', False)
    
    if not broker:
        return jsonify({'error': '请输入服务器地址'}), 400
    
    try:
        from blueprints.mqtt_manager import MQTTManager
        import blueprints.mqtt_manager as mqtt_module
        
        # 如果已有连接，先断开
        if mqtt_module.mqtt_manager and mqtt_module.mqtt_manager.client:
            mqtt_module.mqtt_manager.disconnect()
        
        # 创建新的MQTT管理器
        mqtt_module.mqtt_manager = MQTTManager(
            broker=broker,
            port=port,
            username=username,
            password=password,
            topic_prefix='factory/camera'
        )
        
        success = mqtt_module.mqtt_manager.connect()
        
        if success:
            # 保存配置到数据库
            if save:
                # 禁用之前的配置
                MqttConfig.query.update({'is_active': False})
                
                new_config = MqttConfig(
                    broker=broker,
                    port=port,
                    username=username,
                    password=password,
                    is_active=True
                )
                db.session.add(new_config)
                db.session.commit()
            
            return jsonify({
                'connected': True,
                'broker': broker,
                'port': port
            }), 200
        else:
            return jsonify({'connected': False, 'error': '连接失败'}), 500
            
    except Exception as e:
        return jsonify({'connected': False, 'error': str(e)}), 500

@settings_bp.route('/api/mqtt/disconnect', methods=['POST'])
def mqtt_disconnect():
    """断开MQTT连接"""
    try:
        import blueprints.mqtt_manager as mqtt_module
        if mqtt_module.mqtt_manager and mqtt_module.mqtt_manager.client:
            mqtt_module.mqtt_manager.disconnect()
        
        # 设置cookie表示用户手动断开
        response = make_response(jsonify({'message': '已断开连接'}), 200)
        response.set_cookie('mqtt_auto_connect', 'false', max_age=30*24*60*60)
        return response
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@settings_bp.route('/api/mqtt/configs', methods=['GET'])
def get_mqtt_configs():
    """获取所有保存的MQTT配置（按broker去重）"""
    # 按broker分组，只取最新的记录
    configs = db.session.query(MqttConfig).order_by(MqttConfig.broker, MqttConfig.created_at.desc()).all()
    
    # 去重，保留每个broker最新的记录
    seen = set()
    unique_configs = []
    for c in configs:
        if c.broker not in seen:
            seen.add(c.broker)
            unique_configs.append(c)
    
    return jsonify({
        'configs': [{
            'id': c.id,
            'broker': c.broker,
            'port': c.port,
            'username': c.username or '',
            'is_active': c.is_active
        } for c in unique_configs]
    }), 200

@settings_bp.route('/api/camera/config', methods=['POST'])
def send_camera_config():
    """发送摄像头配置到MQTT"""
    data = request.json
    camera_id = data.get('camera_id')
    config_type = data.get('config_type')
    config_value = data.get('config_value')
    
    if not camera_id or not config_type:
        return jsonify({'error': '缺少必要参数'}), 400
    
    try:
        import blueprints.mqtt_manager as mqtt_module
        if not mqtt_module.mqtt_manager or not mqtt_module.mqtt_manager.connected:
            return jsonify({'error': 'MQTT未连接'}), 400
        
        payload = {
            'type': config_type,
            'value': config_value
        }
        
        success, message = mqtt_module.mqtt_manager.send_camera_command(camera_id, payload)
        
        if success:
            return jsonify({'message': '配置发送成功'}), 200
        else:
            return jsonify({'error': message}), 500
            
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@settings_bp.route('/apijetson/info', methods=['GET'])
def get_jetson_info():
    """获取最新的Jetson设备信息"""
    try:
        import time
        from blueprints.mqtt_manager import mqtt_manager
        if not mqtt_manager or not mqtt_manager.connected:
            return jsonify({'error': 'MQTT未连接'}), 400
            
        if not mqtt_manager.latest_jetson_info:
            return jsonify({'message': '等待数据中...', 'data': None}), 200
            
        # 检查是否超时 (10秒未收到心跳包视为断开)
        is_online = (time.time() - mqtt_manager.last_info_time) < 10
        
        return jsonify({
            'message': 'success',
            'data': mqtt_manager.latest_jetson_info,
            'is_online': is_online
        }), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500
