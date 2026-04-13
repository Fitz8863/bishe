import os
import json
from flask import jsonify


class StreamManager:
    def __init__(self):
        self.cameras = {}
    
    def add_camera(self, camera_id, name, webrtc_url):
        self.cameras[camera_id] = {
            'id': camera_id,
            'name': name,
            'webrtc_url': webrtc_url
        }
    
    def remove_camera(self, camera_id):
        if camera_id in self.cameras:
            del self.cameras[camera_id]
    
    def get_camera(self, camera_id):
        return self.cameras.get(camera_id)
    
    def camera_exists(self, camera_id):
        return camera_id in self.cameras
    
    def get_all_cameras(self):
        return list(self.cameras.values())
    
    def load_from_config(self, config_path):
        """从配置文件加载摄像头"""
        if not os.path.exists(config_path):
            print(f"配置文件不存在: {config_path}")
            return
        
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                config = json.load(f)
            
            cameras = config.get('cameras', [])
            print(f"从配置文件加载到 {len(cameras)} 个摄像头")
            
            for camera in cameras:
                camera_id = camera.get('id')
                name = camera.get('name', camera_id)
                webrtc_url = camera.get('webrtc_url')
                
                if camera_id and webrtc_url:
                    self.add_camera(camera_id, name, webrtc_url)
                    print(f"成功加载摄像头: {camera_id} - {name}")
                        
        except Exception as e:
            print(f"读取配置文件失败: {str(e)}")


stream_manager = StreamManager()


# def init_cameras(config_path=None):
#     """初始化摄像头，从配置文件加载"""
#     if config_path is None:
#         config_path = os.path.join(
#             os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 
#             'cameras.json'
#         )
#     # print(f"[DEBUG] Loading cameras from: {config_path}")
#     stream_manager.load_from_config(config_path)


# def get_camera_info(camera_id):
#     """获取摄像头信息"""
#     camera = stream_manager.get_camera(camera_id)
#     if camera:
#         return jsonify({
#             'id': camera['id'],
#             'name': camera['name'],
#             'webrtc_url': camera['webrtc_url']
#         }), 200
#     return jsonify({'error': 'Camera not found'}), 404


def list_cameras():
    """列出所有摄像头 (仅使用动态MQTT发现)"""
    from blueprints.mqtt_manager import mqtt_manager
    
    # 检查是否连接了MQTT服务器
    if not mqtt_manager or not mqtt_manager.connected:
        return jsonify({
            'cameras': [],
            'mqtt_connected': False,
            'error': '请先在系统设置中连接远程服务器'
        }), 200

    # 获取动态MQTT发现的摄像头
    dynamic_cameras = []
    active_info = mqtt_manager.get_active_cameras()
    online_servos = mqtt_manager.get_servo_status()
    
    for cam in active_info:
        cam_data = {
            'id': cam['id'],
            'name': cam.get('location', f"摄像头 {cam['id']}"),
            'webrtc_url': cam.get('http_url'),
            'is_dynamic': True,
            'has_servo': cam['id'] in online_servos
        }
        dynamic_cameras.append(cam_data)
    
    return jsonify({
        'cameras': dynamic_cameras,
        'mqtt_connected': True
    }), 200
