import json
import paho.mqtt.client as mqtt
from flask import jsonify

class MQTTManager:
    def __init__(self, broker, port, username='', password='', topic_prefix='factory/camera'):
        self.broker = broker
        self.port = port
        self.username = username
        self.password = password
        self.topic_prefix = topic_prefix
        self.client = None
        self.connected = False
    
    def connect(self):
        """连接MQTT服务器"""
        self.client = mqtt.Client()
        if self.username and self.password:
            self.client.username_pw_set(self.username, self.password)
        
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        
        try:
            self.client.connect(self.broker, self.port, 60)
            self.client.loop_start()
            return True
        except Exception as e:
            print(f"MQTT连接失败: {e}")
            return False
    
    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.connected = True
            print("MQTT连接成功")
        else:
            print(f"MQTT连接失败, 返回码: {rc}")
    
    def _on_disconnect(self, client, userdata, rc):
        self.connected = False
        print("MQTT断开连接")
    
    def publish(self, topic, payload):
        """发布消息"""
        if not self.connected or not self.client:
            return False, "MQTT未连接"
        
        try:
            full_topic = f"{self.topic_prefix}/{topic}"
            result = self.client.publish(full_topic, json.dumps(payload))
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                return True, "消息发送成功"
            else:
                return False, "消息发送失败"
        except Exception as e:
            return False, str(e)
    
    def send_camera_command(self, camera_id, command):
        """发送摄像头控制命令""" # {camera_id}/
        return self.publish(f"command", command)
    
    def disconnect(self):
        """断开连接"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()


mqtt_manager = None

def init_mqtt(app):
    """初始化MQTT（不自动连接，等待用户手动连接）"""
    global mqtt_manager
    mqtt_manager = MQTTManager(
        broker='',
        port=1883,
        username='',
        password='',
        topic_prefix=app.config.get('MQTT_TOPIC_PREFIX', 'factory/camera')
    )
    return mqtt_manager

def get_mqtt_status():
    """获取MQTT连接状态"""
    if mqtt_manager:
        return {
            'connected': mqtt_manager.connected,
            'broker': mqtt_manager.broker,
            'port': mqtt_manager.port
        }
    return {'connected': False}

def send_camera_command(camera_id, command):
    """发送摄像头命令"""
    if mqtt_manager:
        return mqtt_manager.send_camera_command(camera_id, command)
    return False, "MQTT未初始化"
