import os
import yaml

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch.actions import GroupAction

from bishe_launch.multicamera_config import build_camera_detector_actions


def _load_camera_config(config_path: str) -> dict:
    with open(config_path, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    cameras = data.get("cameras", [])
    for camera in cameras:
        if "id" in camera:
            return str(camera["id"]), camera
    raise RuntimeError(f"No cameras found in config: {config_path}")


def _to_bool(value: str) -> bool:
    return value.lower() in {"1", "true", "yes", "on"}


def _generate_nodes(context):
    config_path = LaunchConfiguration("config_path").perform(context)
    enable_monitor = _to_bool(LaunchConfiguration("enable_monitor").perform(context))
    compose_camera_detector = _to_bool(
        LaunchConfiguration("compose_camera_detector").perform(context)
    )
    device_name = LaunchConfiguration("device").perform(context)

    actions = []
    camera_id, camera = _load_camera_config(config_path)
    detector = camera.get("detector", {})
    detector.setdefault("input_topic", "camera/detector_frame_ref")
    streamer = camera.get("streamer", {})
    monitor = camera.get("monitor", {})
    namespace = f"camera_{camera_id}"

    group_actions = [
        *build_camera_detector_actions(
            namespace=namespace,
            camera=camera,
            detector=detector,
            device_name=device_name,
            compose_camera_detector=compose_camera_detector,
        ),
        Node(
            package="bishe_streamer",
            executable="streamer_node",
            name="streamer_node",
            namespace=namespace,
            parameters=[
                {
                    "device": device_name,
                    "rtsp_url": streamer.get(
                        "rtsp_url", f"rtsp://localhost:8554/stream_{camera_id}"
                    ),
                    "scale": streamer.get("scale", 1.0),
                    "audio_device": streamer.get("audio_device", "hw:0,0"),
                    "framerate": streamer.get(
                        "framerate", camera.get("framerate", 60)
                    ),
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
                namespace=namespace,
                parameters=[
                    {
                        "device": device_name,
                        "window_seconds": monitor.get("window_seconds", 5),
                        "trigger_frame_threshold": monitor.get(
                            "trigger_frame_threshold", 3
                        ),
                        "trigger_cooldown_seconds": monitor.get(
                            "trigger_cooldown_seconds", 15
                        ),
                        "upload_after_alarm_count": monitor.get(
                            "upload_after_alarm_count", 3
                        ),
                        "reset_alarm_count_after_upload": monitor.get(
                            "reset_alarm_count_after_upload", True
                        ),
                        "alarm_count_reset_timeout_seconds": monitor.get(
                            "alarm_count_reset_timeout_seconds", 30
                        ),
                        "violation_ratio_threshold": monitor.get(
                            "violation_ratio_threshold", 0.4
                        ),
                        "location": camera.get("location", f"camera_{camera_id}"),
                        "camera_id": camera_id,
                        "upload.server_url": monitor.get(
                            "upload_server_url",
                            camera.get(
                                "upload_server_url",
                                "http://localhost:5000/capture/upload",
                            ),
                        ),
                        "upload.timeout_seconds": monitor.get(
                            "upload_timeout_seconds", 10
                        ),
                        "alarm.audio_file": monitor.get(
                            "alarm_audio_file", camera.get("alarm_audio_file", "")
                        ),
                        "alarm.fire_audio_file": monitor.get(
                            "fire_alarm_audio_file",
                            camera.get("fire_alarm_audio_file", ""),
                        ),
                        "alarm.smoking_audio_file": monitor.get(
                            "smoking_alarm_audio_file",
                            camera.get("smoking_alarm_audio_file", ""),
                        ),
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
                    "broker": "100.127.154.73",
                    "port": 1883,
                    "client_id": "jetson",
                    "device": "jetson-orin-nano",
                    "subscribe_topic": "jetson/camera/command",
                    "publish_topic": "jetson/camera/command",
                    "info_topic": "jetson/info",
                    "alarm_topic": "jetson/alarm",
                    "report_interval_sec": 1.0,
                    "camera_id": str(camera_id),
                    "camera_location": camera.get("location", f"camera_{camera_id}"),
                    "camera_http_url": streamer.get("http_url", ""),
                }
            ],
            output="screen",
        )
    )

    actions.append(
        Node(
            package="bishe_streamer",
            executable="intercom_node",
            name="intercom_node",
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
            DeclareLaunchArgument("enable_monitor", default_value="true"),
            DeclareLaunchArgument("compose_camera_detector", default_value="false"),
            DeclareLaunchArgument("device", default_value="jetson-orin-nano"),
            OpaqueFunction(function=_generate_nodes),
        ]
    )
