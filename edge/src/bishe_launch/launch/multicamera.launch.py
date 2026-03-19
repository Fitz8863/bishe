import os
import yaml

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node, PushRosNamespace
from launch.actions import GroupAction


def _load_camera_config(config_path: str) -> dict:
    with open(config_path, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    cameras = data.get("cameras", [])
    return {str(camera["id"]): camera for camera in cameras if "id" in camera}


def _to_bool(value: str) -> bool:
    return value.lower() in {"1", "true", "yes", "on"}


def _generate_nodes(context):
    config_path = LaunchConfiguration("config_path").perform(context)
    selected_ids_value = LaunchConfiguration("camera_ids").perform(context).strip()
    enable_monitor = _to_bool(LaunchConfiguration("enable_monitor").perform(context))
    device_name = LaunchConfiguration("device").perform(context)

    camera_map = _load_camera_config(config_path)
    if not camera_map:
        raise RuntimeError(f"No cameras found in config: {config_path}")

    if selected_ids_value:
        selected_ids = [camera_id.strip() for camera_id in selected_ids_value.split(",") if camera_id.strip()]
    else:
        selected_ids = list(camera_map.keys())

    actions = []
    selected_camera_ids_for_mqtt = []
    selected_camera_locations_for_mqtt = []
    selected_camera_http_urls_for_mqtt = []
    for camera_id in selected_ids:
        if camera_id not in camera_map:
            raise RuntimeError(f"Camera id '{camera_id}' not found in config: {config_path}")

        camera = camera_map[camera_id]
        selected_camera_ids_for_mqtt.append(str(camera_id))
        selected_camera_locations_for_mqtt.append(camera.get("location", f"camera_{camera_id}"))
        detector = camera.get("detector", {})
        streamer = camera.get("streamer", {})
        selected_camera_http_urls_for_mqtt.append(streamer.get("http_url", ""))
        namespace = f"camera_{camera_id}"

        group_actions = [
            PushRosNamespace(namespace),
            Node(
                package="bishe_camera",
                executable="camera_node",
                name="camera_node",
                parameters=[
                    {
                        "device": camera["device"],
                        "width": camera.get("width", 1280),
                        "height": camera.get("height", 720),
                        "framerate": camera.get("framerate", 60),
                    }
                ],
                output="screen",
            ),
            Node(
                package="bishe_detector",
                executable="detector_node",
                name="detector_node",
                parameters=[
                    {
                        "confidence_threshold": detector.get("confidence_threshold", 0.5),
                        "nms_threshold": detector.get("nms_threshold", 0.5),
                        "engine_path": detector.get("engine_path", "/home/jetson/projects/bishe/models/yolov8s.engine"),
                        "worker_threads": detector.get("worker_threads", 1),
                        "max_queue_size": detector.get("max_queue_size", 8),
                    }
                ],
                output="screen",
            ),
            Node(
                package="bishe_streamer",
                executable="streamer_node",
                name="streamer_node",
                parameters=[
                    {
                        "rtsp_url": streamer.get("rtsp_url", f"rtsp://localhost:8554/stream_{camera_id}"),
                        "scale": streamer.get("scale", 1.0),
                        "audio_device": streamer.get("audio_device", "hw:1,0"),
                        "framerate": streamer.get("framerate", camera.get("framerate", 60)),
                    }
                ],
                output="screen",
            ),
        ]

        if enable_monitor:
            group_actions.append(
                Node(
                    package="bishe_monitor",
                    executable="monitor_node",
                    name="monitor_node",
                    parameters=[
                        {
                            "window_seconds": 5,
                            "violation_ratio_threshold": 0.4,
                            "location": camera.get("location", f"camera_{camera_id}"),
                            "camera_id": camera_id,
                            "upload.server_url": camera.get("upload_server_url", "http://localhost:5000/capture/upload"),
                            "alarm.audio_file": camera.get("alarm_audio_file", "/path/to/alarm.mp3"),
                        }
                    ],
                    output="screen",
                )
            )

        actions.append(GroupAction(group_actions))

    actions.append(
        Node(
            package="bishe_mqtt",
            executable="mqtt_node",
            name="mqtt_node",
            parameters=[
                {
                    "broker": "fnas",
                    "port": 1883,
                    "client_id": "jetson",
                    "device": "jetson-orin-nano",
                    "subscribe_topic": "factory/camera/command",
                    "publish_topic": "factory/camera/command",
                    "info_topic": "jetson/info",
                    "report_interval_sec": 1.5,
                    "camera_ids": selected_camera_ids_for_mqtt,
                    "camera_locations": selected_camera_locations_for_mqtt,
                    "camera_http_urls": selected_camera_http_urls_for_mqtt,
                }
            ],
            output="screen",
        )
    )

    return actions


def generate_launch_description():
    default_config_path = os.path.join(
        get_package_share_directory("bishe_launch"),
        "config",
        "cameras.yaml",
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("config_path", default_value=default_config_path),
            DeclareLaunchArgument("camera_ids", default_value=""),
            DeclareLaunchArgument("enable_monitor", default_value="false"),
            DeclareLaunchArgument("device", default_value="jetson-orin-nano"),
            OpaqueFunction(function=_generate_nodes),
        ]
    )
