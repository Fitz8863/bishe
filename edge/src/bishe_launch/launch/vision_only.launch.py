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
                        "confidence_threshold": 0.45,
                        "nms_threshold": 0.5,
                        "engine_path": "/home/jetson/projects/bishe/models/yolov8s.engine",
                        "worker_threads": 2,
                        "max_queue_size": 8,
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
                        "scale": 0.4,
                        "audio_device": "hw:1,0",
                        "framerate": 60,
                    }
                ],
                output="screen",
            ),
        ]
    )
