from blueprints.mqtt_manager import MQTTManager
import time


class TestServoStatus:

    def test_empty_when_no_devices(self):
        manager = MQTTManager(broker='127.0.0.1', port=1883)
        assert manager.get_servo_status() == {}

    def test_returns_recent_device(self):
        manager = MQTTManager(broker='127.0.0.1', port=1883)
        manager.servo_devices['001'] = {
            'info': {'id': '001'},
            'last_seen': time.time()
        }
        result = manager.get_servo_status()
        assert '001' in result

    def test_filters_expired_device(self):
        manager = MQTTManager(broker='127.0.0.1', port=1883)
        manager.servo_devices['001'] = {
            'info': {'id': '001'},
            'last_seen': time.time() - 15
        }
        assert '001' not in manager.get_servo_status()


class TestServoCommand:

    def test_builds_correct_payload(self, monkeypatch):
        manager = MQTTManager(broker='127.0.0.1', port=1883)
        manager.connected = True

        captured = {}

        def mock_publish_raw(topic, payload):
            captured['topic'] = topic
            captured['payload'] = payload
            return True, "OK"

        monkeypatch.setattr(manager, 'publish_raw', mock_publish_raw)
        manager.send_servo_command('001', 5, -3)

        assert captured['topic'] == 'jetson/esp8266/cmd'
        assert captured['payload'] == {'id': '001', 'col': 5, 'row': -3}

    def test_fails_when_not_connected(self):
        manager = MQTTManager(broker='127.0.0.1', port=1883)
        manager.connected = False
        manager.client = None
        success, msg = manager.send_servo_command('001', 0, 0)
        assert not success
        assert '未连接' in msg
