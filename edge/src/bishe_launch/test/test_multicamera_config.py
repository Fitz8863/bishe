import sys
from pathlib import Path

from launch_ros.actions import Node

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from bishe_launch.multicamera_config import build_camera_detector_actions


def _normalize_launch_params(param_tuple):
    normalized = {}
    for key_parts, value in param_tuple[0].items():
        key = "".join(part.text for part in key_parts)
        if isinstance(value, tuple):
            value = "".join(part.text for part in value)
        normalized[key] = value
    return normalized


def test_build_camera_detector_actions_defaults_to_separate_process_nodes():
    actions = build_camera_detector_actions(
        namespace="camera_001",
        camera={
            "id": "001",
            "device": "/dev/video0",
            "width": 1280,
            "height": 720,
            "framerate": 60,
            "detector_scale": 0.5,
        },
        detector={
            "confidence_threshold": 0.25,
            "nms_threshold": 0.5,
            "sampling_interval_ms": 1000,
            "lock_duration_ms": 3000,
            "engine_path": "models/best.engine",
            "worker_threads": 1,
            "max_queue_size": 4,
        },
        device_name="jetson-orin-nano",
    )

    assert len(actions) == 2
    assert all(isinstance(action, Node) for action in actions)
    camera_params = _normalize_launch_params(actions[0]._Node__parameters)
    detector_params = _normalize_launch_params(actions[1]._Node__parameters)
    assert camera_params["detector_scale"] == 0.5
    assert camera_params["detector_width"] == 640
    assert camera_params["detector_height"] == 360
    assert str(detector_params["input_topic"]).startswith("camera/detector_frame_ref")
    assert detector_params["detector_width"] == 640
    assert detector_params["detector_height"] == 360
