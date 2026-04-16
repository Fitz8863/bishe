# Half-Resolution Detector Stream Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a half-resolution image stream from `camera_node` to `detector_node` so detector throughput improves while streamer/monitor keep current full-resolution behavior.

**Architecture:** `camera_node` will publish both the existing full-resolution `camera/image_raw` and a new half-resolution detection-only topic. `detector_node` will subscribe to the detection topic while keeping the rest of the pipeline unchanged, so monitor/streamer continue consuming `detector/result` exactly as before.

**Tech Stack:** ROS 2 Humble, rclcpp, sensor_msgs/Image, cv_bridge, OpenCV, pytest/gtest, colcon.

---

### Task 1: Add failing coverage for the new detector image topic

**Files:**
- Modify: `src/bishe_launch/test/test_multicamera_config.py`

- [ ] **Step 1: Write the failing test**
- [ ] **Step 2: Run test to verify it fails**
- [ ] **Step 3: Implement the minimal launch/config changes to pass**
- [ ] **Step 4: Run test to verify it passes**

### Task 2: Publish half-resolution detector frames from camera

**Files:**
- Modify: `src/bishe_camera/src/camera_node.cpp`

- [ ] **Step 1: Add parameters for detector stream width/height or half-scale defaults**
- [ ] **Step 2: Publish a second topic for detector consumption with half-resolution frames**
- [ ] **Step 3: Keep the existing full-resolution topic unchanged**
- [ ] **Step 4: Verify build succeeds**

### Task 3: Rewire detector to consume the low-resolution topic

**Files:**
- Modify: `src/bishe_detector/src/detector_node.cpp`

- [ ] **Step 1: Add detector input topic parameter/default**
- [ ] **Step 2: Subscribe detector to the low-resolution topic**
- [ ] **Step 3: Keep downstream `detector/result` behavior unchanged**
- [ ] **Step 4: Run detector tests**

### Task 4: Verify runtime packages still build together

**Files:**
- Modify: `src/bishe_launch/launch/multicamera.launch.py`
- Modify: `src/bishe_launch/bishe_launch/multicamera_config.py`

- [ ] **Step 1: Thread the new detector topic/size parameters through launch**
- [ ] **Step 2: Run `py_compile` and launch pytest**
- [ ] **Step 3: Build `bishe_camera bishe_detector bishe_launch bishe_streamer bishe_monitor`**
