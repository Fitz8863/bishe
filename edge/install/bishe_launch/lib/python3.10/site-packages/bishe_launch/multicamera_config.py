from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode


def _camera_parameters(camera: dict, device_name: str) -> dict:
    detector_scale = camera.get("detector_scale", 0.5)
    detector_width = max(1, int(round(camera.get("width", 1280) * detector_scale)))
    detector_height = max(1, int(round(camera.get("height", 720) * detector_scale)))
    return {
        "video_device": camera["device"],
        "device": device_name,
        "width": camera.get("width", 1280),
        "height": camera.get("height", 720),
        "framerate": camera.get("framerate", 60),
        "detector_scale": detector_scale,
        "detector_width": detector_width,
        "detector_height": detector_height,
        "shared_memory_name": f"/camera_{camera['id']}_detector_shm",
        "shared_metadata_topic": "camera/detector_frame_ref",
    }


def _detector_parameters(camera: dict, detector: dict, device_name: str) -> dict:
    detector_scale = camera.get("detector_scale", 0.5)
    detector_width = max(1, int(round(camera.get("width", 1280) * detector_scale)))
    detector_height = max(1, int(round(camera.get("height", 720) * detector_scale)))
    return {
        "device": device_name,
        "confidence_threshold": detector.get("confidence_threshold", 0.5),
        "nms_threshold": detector.get("nms_threshold", 0.5),
        "sampling_interval_ms": detector.get("sampling_interval_ms", 1000),
        "lock_duration_ms": detector.get("lock_duration_ms", 3000),
        "engine_path": detector.get(
            "engine_path", "/home/jetson/projects/bishe/models/yolov8s.engine"
        ),
        "worker_threads": detector.get("worker_threads", 1),
        "max_queue_size": detector.get("max_queue_size", 4),
        "input_topic": detector.get("input_topic", "camera/detector_frame_ref"),
        "detector_width": detector_width,
        "detector_height": detector_height,
        "shared_memory_name": f"/camera_{camera['id']}_detector_shm",
    }


def build_camera_detector_actions(
    namespace: str,
    camera: dict,
    detector: dict,
    device_name: str,
    compose_camera_detector: bool = False,
):
    if compose_camera_detector:
        extra_arguments = [{"use_intra_process_comms": True}]
        return [
            ComposableNodeContainer(
                name=f"camera_detector_container_{camera['id']}",
                namespace=namespace,
                package="rclcpp_components",
                executable="component_container_mt",
                composable_node_descriptions=[
                    ComposableNode(
                        package="bishe_camera",
                        plugin="bishe_camera::CameraNode",
                        name="camera_node",
                        parameters=[_camera_parameters(camera, device_name)],
                        extra_arguments=extra_arguments,
                    ),
                    ComposableNode(
                        package="bishe_detector",
                        plugin="bishe_detector::DetectorNode",
                        name="detector_node",
                        parameters=[_detector_parameters(camera, detector, device_name)],
                        extra_arguments=extra_arguments,
                    ),
                ],
                output="screen",
            )
        ]

    return [
        Node(
            package="bishe_camera",
            executable="camera_node",
            name="camera_node",
            namespace=namespace,
            parameters=[_camera_parameters(camera, device_name)],
            output="screen",
        ),
        Node(
            package="bishe_detector",
            executable="detector_node",
            name="detector_node",
            namespace=namespace,
            parameters=[_detector_parameters(camera, detector, device_name)],
            output="screen",
        ),
    ]
