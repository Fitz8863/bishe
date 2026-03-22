from flask_login import login_required, current_user
from flask import Blueprint, render_template, request, jsonify, make_response, abort
from . import db, login_manager
from .models import MqttConfig
from .auth import admin_required

settings_bp = Blueprint('settings', __name__, url_prefix='/settings')

@settings_bp.before_request
def before_request():
    if request.endpoint == 'settings.intercom_webhook_stop':
        return
    
    if not current_user.is_authenticated:
        return login_manager.unauthorized()
    if not current_user.is_admin:
        abort(403)

@settings_bp.route('/api/intercom/webhook/stop', methods=['POST'])
def intercom_webhook_stop():
    device_id = None
    if request.is_json:
        device_id = request.json.get('device_id')
    else:
        device_id = request.form.get('device_id')
    
    if not device_id:
        return jsonify({'error': 'Missing device_id'}), 400
        
    try:
        from blueprints.mqtt_manager import mqtt_manager
        if mqtt_manager and mqtt_manager.connected:
            success, message = mqtt_manager.send_intercom_command(device_id, 'stop')
            if success:
                return jsonify({'message': f'Stopped intercom for {device_id}'}), 200
        return jsonify({'error': 'MQTT not connected'}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500

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
    data = request.json
    broker = data.get('broker')
    port = data.get('port', 1883)
    username = data.get('username', '')
    password = data.get('password', '')
    mediamtx_whip_port = data.get('mediamtx_whip_port', 8889)
    mediamtx_rtsp_port = data.get('mediamtx_rtsp_port', 8554)
    save = data.get('save', False)
    
    if not broker:
        return jsonify({'error': '请输入服务器地址'}), 400
    
    try:
        from blueprints.mqtt_manager import MQTTManager
        import blueprints.mqtt_manager as mqtt_module
        
        if mqtt_module.mqtt_manager and mqtt_module.mqtt_manager.client:
            mqtt_module.mqtt_manager.disconnect()
        
        mqtt_module.mqtt_manager = MQTTManager(
            broker=broker,
            port=port,
            username=username,
            password=password,
            topic_prefix='factory/camera'
        )
        
        success = mqtt_module.mqtt_manager.connect()
        
        if success:
            if save:
                MqttConfig.query.update({'is_active': False})
                
                new_config = MqttConfig(
                    broker=broker,
                    port=port,
                    username=username,
                    password=password,
                    mediamtx_whip_port=mediamtx_whip_port,
                    mediamtx_rtsp_port=mediamtx_rtsp_port,
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
    try:
        import blueprints.mqtt_manager as mqtt_module
        if mqtt_module.mqtt_manager and mqtt_module.mqtt_manager.client:
            mqtt_module.mqtt_manager.disconnect()
        
        response = make_response(jsonify({'message': '已断开连接'}), 200)
        response.set_cookie('mqtt_auto_connect', 'false', max_age=30*24*60*60)
        return response
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@settings_bp.route('/api/intercom/config', methods=['GET'])
def get_intercom_config():
    try:
        config = MqttConfig.query.filter_by(is_active=True).first()
        if config:
            return jsonify({
                'mediamtx_ip': config.broker,
                'mediamtx_whip_port': config.mediamtx_whip_port,
                'mediamtx_rtsp_port': config.mediamtx_rtsp_port
            }), 200
        return jsonify({'error': '未找到有效配置'}), 404
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
            'mediamtx_whip_port': c.mediamtx_whip_port or 8889,
            'mediamtx_rtsp_port': c.mediamtx_rtsp_port or 8554,
            'is_active': c.is_active
        } for c in unique_configs]
    }), 200

@settings_bp.route('/api/intercom/start', methods=['POST'])
def intercom_start():
    data = request.json
    device_id = data.get('device_id')
    
    if not device_id:
        return jsonify({'error': '缺少设备ID'}), 400
        
    try:
        from blueprints.mqtt_manager import mqtt_manager
        if not mqtt_manager or not mqtt_manager.connected:
            return jsonify({'error': 'MQTT未连接'}), 400
            
        config = MqttConfig.query.filter_by(is_active=True).first()
        if not config or not config.broker:
            return jsonify({'error': '请先在设置中配置服务器地址'}), 400
            
        rtsp_url = f"rtsp://{config.broker}:{config.mediamtx_rtsp_port}/{device_id}"
        success, message = mqtt_manager.send_intercom_command(device_id, 'start', rtsp_url)
        
        if success:
            return jsonify({'message': '对讲指令已发送'}), 200
        return jsonify({'error': message}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@settings_bp.route('/api/intercom/stop', methods=['POST'])
def intercom_stop():
    data = request.json
    device_id = data.get('device_id')
    
    if not device_id:
        return jsonify({'error': '缺少设备ID'}), 400
        
    try:
        from blueprints.mqtt_manager import mqtt_manager
        if not mqtt_manager or not mqtt_manager.connected:
            return jsonify({'error': 'MQTT未连接'}), 400
            
        success, message = mqtt_manager.send_intercom_command(device_id, 'stop')
        if success:
            return jsonify({'message': '对讲停止指令已发送'}), 200
        return jsonify({'error': message}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500

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

@settings_bp.route('/api/mqtt/realtime-stats', methods=['GET'])
def get_mqtt_realtime_stats():
    """获取MQTT实时设备统计（在线设备数、摄像头数）"""
    try:
        import time
        from blueprints.mqtt_manager import mqtt_manager
        if not mqtt_manager:
            return jsonify({
                'connected': False,
                'device_count': 0,
                'camera_count': 0,
                'devices': []
            }), 200
        
        # 获取活跃设备数据
        data = mqtt_manager.get_active_data()
        
        return jsonify({
            'connected': mqtt_manager.connected,
            'device_count': data.get('device_count', 0),
            'camera_count': data.get('camera_count', 0),
            'devices': data.get('devices', [])
        }), 200
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
