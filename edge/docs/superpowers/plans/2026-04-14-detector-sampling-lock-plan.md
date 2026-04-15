# Detector Sampling Lock Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add sampling-then-lock detection gating in `bishe_detector` so YOLO runs once per second by default, switches to full-frame detection for 3 seconds after any detection, refreshes the window on repeat detections, and still publishes `detector/result` for every input frame.

**Architecture:** Keep the existing camera → detector → streamer/monitor topology. Implement a small gate controller inside `detector_node`: `imageCallback()` decides whether a frame goes through YOLO or is pass-through published, while `workerLoop()` refreshes the lock window when inference returns any detection. Add runtime parameters for sampling interval and lock duration.

**Tech Stack:** ROS 2 Humble, rclcpp, image_transport, cv_bridge, OpenCV, TensorRT, GoogleTest.

---

## File Structure

- Modify: `src/bishe_detector/src/detector_node.cpp`
  - Add gate state, parameter parsing, pass-through publishing, and lock refresh logic.
- Modify: `src/bishe_detector/CMakeLists.txt`
  - Add test target if missing.
- Modify: `src/bishe_detector/package.xml`
  - Add gtest dependency if missing.
- Create: `src/bishe_detector/include/bishe_detector/detection_gate.hpp`
  - Encapsulate gate decision logic in a testable helper.
- Create: `src/bishe_detector/test/test_detection_gate.cpp`
  - Unit tests for sampling/lock transitions.
- Modify: `src/bishe_launch/config/cameras.yaml`
  - Add initial detector parameters for sampling interval and lock duration.

## Chunk 1: Build a Testable Gate Helper

### Task 1: Add gate helper tests first

**Files:**
- Create: `src/bishe_detector/test/test_detection_gate.cpp`
- Create: `src/bishe_detector/include/bishe_detector/detection_gate.hpp`
- Modify: `src/bishe_detector/CMakeLists.txt`
- Modify: `src/bishe_detector/package.xml`

- [ ] **Step 1: Write the failing test**

Add tests that verify:
- sampling mode allows the first frame immediately
- subsequent frames before `sampling_interval_ms` are skipped
- any detection result enters locked mode
- locked mode allows all frames before `locked_until`
- a new detection during locked mode refreshes `locked_until`
- no detection until timeout returns to sampling mode

- [ ] **Step 2: Run test to verify it fails**

Run: `colcon build --packages-select bishe_detector --cmake-args -DBUILD_TESTING=ON`

Expected: FAIL because `detection_gate.hpp` and/or test target do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Implement a small helper class, for example:

```cpp
enum class DetectionMode { Sampling, Locked };

class DetectionGate {
public:
  DetectionGate(std::chrono::milliseconds sampling_interval,
                std::chrono::milliseconds lock_duration);

  bool should_process(std::chrono::steady_clock::time_point now);
  void on_detection(std::chrono::steady_clock::time_point now);
  void update_config(std::chrono::milliseconds sampling_interval,
                     std::chrono::milliseconds lock_duration);
};
```

Keep all timing state inside this helper so `detector_node.cpp` stays readable.

- [ ] **Step 4: Run test to verify it passes**

Run: `colcon test --packages-select bishe_detector --event-handlers console_direct+`

Expected: PASS for `test_detection_gate`.

## Chunk 2: Integrate Gate Into detector_node

### Task 2: Add detector parameters and pass-through publishing

**Files:**
- Modify: `src/bishe_detector/src/detector_node.cpp`
- Modify: `src/bishe_launch/config/cameras.yaml`
- Reference: `src/bishe_msgs/msg/DetectorResult.msg`

- [ ] **Step 1: Write the failing test / verification target**

Add at least one unit-level test or deterministic helper test to prove pass-through result construction preserves image header and clears violation fields when YOLO is skipped. If direct node testing is too heavy, extract a tiny helper to create pass-through `DetectorResult` and test that helper.

- [ ] **Step 2: Run test to verify it fails**

Run the same package test command and confirm the new expectation fails before implementation.

- [ ] **Step 3: Write minimal implementation**

In `detector_node.cpp`:
- declare/get new parameters:
  - `sampling_interval_ms`
  - `lock_duration_ms`
- extend parameter callback validation
- instantiate `DetectionGate`
- in `imageCallback()`:
  - increment input stats
  - call `gate.should_process(now)`
  - if false: publish pass-through `DetectorResult` immediately
  - if true: enqueue the frame for existing worker processing

Pass-through result requirements:

```cpp
result.has_violation = false;
result.confidence = 0.0f;
result.violation_type = "";
result.nms_threshold = nms_threshold_.load();
result.annotated_image = *msg;
```

Do not create extra image copies for pass-through frames.

- [ ] **Step 4: Run test to verify it passes**

Run: `colcon test --packages-select bishe_detector --event-handlers console_direct+`

Expected: PASS for helper tests.

### Task 3: Refresh lock window from worker results

**Files:**
- Modify: `src/bishe_detector/src/detector_node.cpp`

- [ ] **Step 1: Write the failing test**

Extend `test_detection_gate.cpp` so a second detection during locked mode refreshes the deadline rather than letting it expire at the old time.

- [ ] **Step 2: Run test to verify it fails**

Run: `colcon test --packages-select bishe_detector --event-handlers console_direct+`

Expected: FAIL on refresh behavior before the integration is complete.

- [ ] **Step 3: Write minimal implementation**

In `workerLoop()`:
- after `DetectionResult detection_result = worker->yolo->Detect(frame);`
- if `!detection_result.detections.empty()` then call `gate.on_detection(now)`

The lock refresh should be based on worker completion time or a captured frame decision time, but choose one consistently and document it inline if non-obvious.

- [ ] **Step 4: Run test to verify it passes**

Run: `colcon test --packages-select bishe_detector --event-handlers console_direct+`

Expected: PASS.

## Chunk 3: Validate Runtime Behavior and Config

### Task 4: Wire config defaults and build verification

**Files:**
- Modify: `src/bishe_launch/config/cameras.yaml`
- Modify: `src/bishe_detector/src/detector_node.cpp`

- [ ] **Step 1: Set YAML defaults**

Add:

```yaml
detector:
  sampling_interval_ms: 1000
  lock_duration_ms: 3000
```

- [ ] **Step 2: Build the package**

Run: `colcon build --packages-select bishe_detector --cmake-args -DBUILD_TESTING=ON`

Expected: build succeeds.

- [ ] **Step 3: Run tests**

Run: `colcon test --packages-select bishe_detector --event-handlers console_direct+`

Expected: all detector tests pass.

- [ ] **Step 4: Run focused runtime smoke verification**

Run the application in the workspace and verify:
- default state only sends ~1 processed YOLO frame per second
- output `detector/result` frame rate remains continuous for streamer input
- a detected object causes full-frame YOLO for ~3 seconds
- repeated detections extend the lock window
- after inactivity, detector returns to sampling mode

Use logs/counters inside `detector_node` if necessary, but keep them throttled.

## Verification Checklist

- [ ] `bishe_detector` builds successfully with `BUILD_TESTING=ON`
- [ ] `test_detection_gate` and any helper tests pass
- [ ] pass-through frames preserve video continuity for `streamer_node`
- [ ] monitor still ignores non-violation pass-through frames without regression
- [ ] sampling and lock durations are configurable and validated

## Notes

- Keep the topology unchanged: do not refactor `streamer_node` to subscribe to `camera/image_raw` in this change.
- Keep the message contract unchanged: no new ROS message type.
- Prefer a testable helper over embedding all timing logic directly into `detector_node.cpp`.
