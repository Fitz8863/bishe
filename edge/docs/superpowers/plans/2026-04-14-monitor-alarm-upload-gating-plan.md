# Monitor Alarm Upload Gating Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Change `bishe_monitor` so each violation trigger first plays alarm audio, and only after a configurable number of consecutive alarm triggers does it capture and upload evidence, with configurable reset-after-upload and quiet-time reset behavior.

**Architecture:** Keep the current detector → monitor → upload topology. Extend `monitor_node` with a small, testable alarm-upload gate helper that tracks per-violation-type alarm counts and reset timing, while preserving the existing hit window and cooldown logic. Wire the new parameters through launch and YAML config.

**Tech Stack:** ROS 2 Humble, rclcpp, OpenCV, cv_bridge, libcurl, GoogleTest.

---

## File Structure

- Modify: `src/bishe_monitor/src/monitor_node.cpp`
  - Split alarm playback from capture/upload and integrate alarm-upload gate logic.
- Create: `src/bishe_monitor/include/bishe_monitor/alarm_upload_gate.hpp`
  - Encapsulate per-type alarm count and reset logic in a testable helper.
- Modify: `src/bishe_monitor/test/test_async_task_worker.cpp` or create a new test file
  - Add unit tests for alarm-upload gating behavior.
- Modify: `src/bishe_monitor/CMakeLists.txt`
  - Add new test target if needed.
- Modify: `src/bishe_monitor/package.xml`
  - Add test dependency only if new test targets need more deps.
- Modify: `src/bishe_launch/config/cameras.yaml`
  - Add new monitor config defaults.
- Modify: `src/bishe_launch/launch/multicamera.launch.py`
  - Pass new monitor parameters into the node.

## Chunk 1: Build a Testable Alarm Upload Gate

### Task 1: Write failing tests for gate behavior

**Files:**
- Create: `src/bishe_monitor/include/bishe_monitor/alarm_upload_gate.hpp`
- Create or Modify: `src/bishe_monitor/test/test_alarm_upload_gate.cpp`
- Modify: `src/bishe_monitor/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Add tests that verify:
- first alarm increments count but does not request upload when threshold > 1
- reaching `upload_after_alarm_count` requests upload
- upload can clear count when `reset_alarm_count_after_upload=true`
- quiet timeout clears stale count before next alarm
- disabling quiet timeout keeps count across long gaps

- [ ] **Step 2: Run test to verify it fails**

Run: `colcon build --packages-select bishe_monitor --cmake-args -DBUILD_TESTING=ON`

Expected: FAIL because helper and/or test target do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Implement a helper along these lines:

```cpp
struct AlarmGateResult {
  bool should_upload;
  int new_count;
};

class AlarmUploadGate {
public:
  AlarmGateResult on_alarm_trigger(...);
  void on_upload_completed(...);
};
```

Keep reset rules inside the helper so `monitor_node.cpp` stays focused on orchestration.

- [ ] **Step 4: Run test to verify it passes**

Run: `colcon test --packages-select bishe_monitor --event-handlers console_direct+`

Expected: PASS for the new alarm gate tests.

## Chunk 2: Integrate Gate Into monitor_node

### Task 2: Separate alarm playback from capture/upload

**Files:**
- Modify: `src/bishe_monitor/src/monitor_node.cpp`

- [ ] **Step 1: Write the failing verification target**

Add a helper-level test if needed to prove that a trigger can request “play only” without upload.

- [ ] **Step 2: Run test to verify it fails**

Run: `colcon test --packages-select bishe_monitor --event-handlers console_direct+`

Expected: FAIL for the new expectation before integration.

- [ ] **Step 3: Write minimal implementation**

In `monitor_node.cpp`:
- keep existing `hit_times` + cooldown logic
- after a valid trigger:
  - clear hit window for the next round
  - play alarm first
  - update gate state for that violation type
  - only enqueue capture/upload if gate says upload is due

Prefer splitting current `enqueueCaptureAndAlarm()` into clearer pieces, such as:
- `enqueueAlarmPlayback(...)`
- `enqueueCaptureUpload(...)`

or keep one worker path but only call upload branch when needed.

- [ ] **Step 4: Run test to verify it passes**

Run: `colcon test --packages-select bishe_monitor --event-handlers console_direct+`

Expected: all bishe_monitor tests pass.

### Task 3: Add new parameters and per-type state wiring

**Files:**
- Modify: `src/bishe_monitor/src/monitor_node.cpp`
- Modify: `src/bishe_launch/config/cameras.yaml`
- Modify: `src/bishe_launch/launch/multicamera.launch.py`

- [ ] **Step 1: Add parameter declarations and validation**

Add:
- `upload_after_alarm_count`
- `reset_alarm_count_after_upload`
- `alarm_count_reset_timeout_seconds`

Validation rules:
- `upload_after_alarm_count >= 1`
- `alarm_count_reset_timeout_seconds >= 0`

- [ ] **Step 2: Wire YAML defaults**

Add to `cameras.yaml`, for example:

```yaml
monitor:
  upload_after_alarm_count: 2
  reset_alarm_count_after_upload: true
  alarm_count_reset_timeout_seconds: 30
```

- [ ] **Step 3: Pass parameters through launch**

Update `multicamera.launch.py` so monitor node receives the three new parameters.

- [ ] **Step 4: Run build verification**

Run:
- `colcon build --packages-select bishe_monitor`
- `colcon build --packages-select bishe_launch`

Expected: both packages build successfully.

## Chunk 3: Validate Runtime Semantics

### Task 4: Verify behavior end-to-end at package level

**Files:**
- Modify: `src/bishe_monitor/src/monitor_node.cpp` only if runtime logs or small adjustments are needed

- [ ] **Step 1: Run tests**

Run: `colcon test --packages-select bishe_monitor --event-handlers console_direct+`

Expected: all bishe_monitor tests pass.

- [ ] **Step 2: Run focused smoke checks**

Verify these behaviors during runtime or through controlled test hooks:
- first valid trigger plays alarm but does not upload when threshold > 1
- Nth valid trigger uploads
- upload-after-trigger resets count if configured
- long silence resets count if configured
- `fire` and `smoking` maintain independent counts

- [ ] **Step 3: Confirm no detector/streamer regressions**

Ensure this change does not modify detector-side or streamer-side logic and only changes monitor-side trigger behavior.

## Verification Checklist

- [ ] `bishe_monitor` builds successfully
- [ ] new alarm-upload gate tests pass
- [ ] `bishe_launch` builds or syntax-checks successfully
- [ ] upload gating parameters are configurable from `cameras.yaml`
- [ ] `fire` / `smoking` counts are tracked independently
- [ ] upload count reset rules behave as configured

## Notes

- Keep the existing violation hit window and cooldown logic intact.
- Do not move this logic into `detector_node`.
- Prefer a small helper for count/reset decisions over hard-coding all rules into `resultCallback()`.
