import json
import time
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
        self.latest_jetson_info = None
        self.last_info_time = 0
    
    def connect(self):
        """连接MQTT服务器"""
        self.client = mqtt.Client()
        if self.username and self.password:
            self.client.username_pw_set(self.username, self.password)
        
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
        
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
            # 订阅设备信息主题
            client.subscribe("jetson/info")
        else:
            self.connected = False
            print(f"MQTT连接失败, 返回码: {rc}")

    def _on_message(self, client, userdata, msg):
        if msg.topic == "jetson/info":
            try:
                payload = json.loads(msg.payload.decode('utf-8'))
                # 移除不需要的字段
                if 'timestamp_ns' in payload:
                    del payload['timestamp_ns']
                self.latest_jetson_info = payload
                self.last_info_time = time.time()
                self.connected = True # 收到消息说明连接肯定正常
            except Exception as e:
                print(f"解析 jetson/info 消息失败: {e}")
            
    def _on_disconnect(self, client, userdata, rc):
        self.connected = False
        print(f"MQTT断开连接, 返回码: {rc}")
        if rc != 0:
            print("尝试自动重连...")
            # loop_start 会处理自动重连，我们只需要更新状态
    
    def check_connection(self):
        """主动检查连接状态"""
        if self.client:
            self.connected = self.client.is_connected()
        return self.connected
    
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
        """发送摄像头控制命令 (固定主题，Payload包含ID)"""
        if isinstance(command, dict):
            command['camera_id'] = camera_id
        return self.publish("command", command)
    
    def disconnect(self):
        """断开连接"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()

    def get_active_cameras(self):
        """获取当前在线的摄像头列表"""
        if not self.connected or not self.latest_jetson_info:
            return []
        
        # 检查是否超时 (10秒未收到心跳包视为断开)
        if (time.time() - self.last_info_time) > 10:
            return []
            
        return self.latest_jetson_info.get('cameras', [])


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
