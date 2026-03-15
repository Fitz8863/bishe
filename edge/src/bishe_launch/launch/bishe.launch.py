from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription(
        [
            # Camera node
            Node(
                package="bishe_camera",
                executable="camera_node",
                name="camera_node",
                parameters=[
                    {
                        "device": "/dev/video0",
                        "width": 1280,
                        "height": 720,
                        "framerate": 60,
                    }
                ],
                output="screen",
            ),
            # Detector node
            Node(
                package="bishe_detector",
                executable="detector_node",
                name="detector_node",
                parameters=[
                    {
                        "confidence_threshold": 0.5,
                        'nms_threshold': 0.5,
                        "engine_path": "/home/jetson/projects/bishe/models/yolov8s.engine",
                        "worker_threads": 1,
                        "max_queue_size": 12,
                    }
                ],
                output="screen",
            ),
            # Streamer node
            Node(
                package="bishe_streamer",
                executable="streamer_node",
                name="streamer_node",
                parameters=[
                    {
                        "rtsp_url": "rtsp://localhost:8554/stream",
                        "scale": 1.0,
                        "audio_device": "hw:1,0",
                        "framerate": 60,
                    }
                ],
                output="screen",
            ),
            # Monitor node
            Node(
                package="bishe_monitor",
                executable="monitor_node",
                name="monitor_node",
                parameters=[
                    {
                        "window_seconds": 5,
                        "violation_ratio_threshold": 0.4,
                        "location": "生产车间A区",
                        "camera_id": "001",
                        "upload.server_url": "http://YOUR_SERVER_IP:5000/capture/upload",
                        "alarm.audio_file": "/path/to/alarm.mp3",
                    }
                ],
                output="screen",
            ),
            # MQTT node
            Node(
                package="bishe_mqtt",
                executable="mqtt_node",
                name="mqtt_node",
                parameters=[
                    {
                        "broker": "YOUR_MQTT_BROKER_IP",
                        "port": 1883,
                        "client_id": "bishe_camera_001",
                        "subscribe_topic": "factory/camera/001/command",
                        "publish_topic": "factory/camera_001/status",
                    }
                ],
                output="screen",
            ),
        ]
    )
