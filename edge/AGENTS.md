# 🤖 Agent Guidelines for `bishe` Workspace (ROS 2 Humble)

This repository contains a ROS 2 Humble workspace for a camera monitoring and detection system (likely a graduation project / "bishe"). It includes nodes for camera capture, AI detection, stream handling, rule monitoring, and MQTT communication.

## 🏗️ Environment & Architecture
- **Framework**: ROS 2 Humble
- **OS**: Ubuntu Linux (usually 22.04 for Humble)
- **Build System**: Colcon / ament_cmake / ament_python
- **Primary Languages**: C++17 and Python 3.10
- **Domain**: Factory camera monitoring (RTSP streaming, TensorRT/YOLO detection, MQTT telemetry)

## 🛠️ Build & Test Commands

### Building
Always build from the workspace root (where `src` is located).

- **Full Build**:
  ```bash
  colcon build --symlink-install
  ```
- **Build a Single Package**:
  ```bash
  colcon build --packages-select <package_name> --symlink-install
  ```
- **Build with Compile Commands (for LSP/IntelliSense)**:
  ```bash
  colcon build --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  ```
- **Clean Workspace**:
  ```bash
  rm -rf build/ install/ log/
  ```

### Sourcing
After building, you must source the workspace before running nodes or tests:
```bash
source install/setup.bash
```

### Testing
- **Run All Tests**:
  ```bash
  colcon test
  ```
- **Check Test Results**:
  ```bash
  colcon test-result --all
  ```
- **Run Tests for a Single Package**:
  ```bash
  colcon test --packages-select <package_name>
  ```
- **Run a Specific Test (GTest / PyTest)**:
  ```bash
  # For Python packages
  colcon test --packages-select <package_name> --pytest-args -k "test_name"
  # For C++ packages with CTest
  colcon test --packages-select <package_name> --ctest-args -R "test_name"
  ```

## 📝 Code Style & Conventions

### 1. General Rules
- **Language**: Code logic, variables, and structural names MUST be in English.
- **Comments**: Chinese comments and strings are acceptable and already present in the codebase (e.g., `// TODO: 在此处初始化...`). Keep Chinese for documentation/comments if it helps clarify logic for the human developer.
- **Dependencies**: Keep `package.xml` and `CMakeLists.txt` (or `setup.py`) synchronized when adding new dependencies.

### 2. C++ Guidelines (ROS 2 Style)
- **Standard**: C++17.
- **Naming Conventions**:
  - `CamelCase` for Classes and Structs (e.g., `DetectorNode`).
  - `snake_case` for variables, functions, and namespaces (e.g., `image_sub_`, `imageCallback`).
  - `ALL_CAPS` for macros and constants.
  - **Class member variables** MUST end with a trailing underscore (e.g., `confidence_threshold_`).
- **Pointers**: Prefer smart pointers. Use ROS 2 typedefs like `bishe_msgs::msg::DetectorResult::SharedPtr`.
- **ROS 2 Paradigms**:
  - Encapsulate logic within classes inheriting from `rclcpp::Node`. Avoid global state.
  - Declare and get ROS parameters inside the node's constructor (`this->declare_parameter`, `this->get_parameter`).
  - Use `ament_target_dependencies` in `CMakeLists.txt` instead of `target_link_libraries` for ROS 2 packages whenever possible.
- **Logging**:
  - NEVER use `std::cout` or `printf`.
  - ALWAYS use ROS 2 logging macros: `RCLCPP_INFO(this->get_logger(), ...)`, `RCLCPP_ERROR`, `RCLCPP_DEBUG`.
- **Error Handling**:
  - Avoid `exit(1)` or terminating the node process manually.
  - Log errors appropriately and return early or throw standard C++ exceptions caught at a higher level.

### 3. Python Guidelines
- **Standard**: PEP 8 compliance.
- **Naming Conventions**:
  - `CamelCase` for Classes.
  - `snake_case` for functions, variables, and module names.
- **ROS 2 Launch Files**:
  - Always use modern Python launch files (`.launch.py`).
  - Follow the structure `def generate_launch_description(): return LaunchDescription([...])`.
- **Type Hints**: Use Python type hints (`def func(a: int) -> bool:`) where applicable to improve readability and static analysis.

### 4. ROS 2 Architecture Rules
- **Messages**: All custom interfaces (messages, services, actions) are defined in the `bishe_msgs` package. When modifying interfaces, rebuild `bishe_msgs` first before building other dependent packages.
- **QoS Profiles**: Explicitly specify QoS (Quality of Service) profiles for publishers and subscribers (e.g., `rmw_qos_profile_sensor_data` for images, `10` depth for standard data).

## 🤖 AI Agent Instructions (Cursor / Copilot / OpenCode)
- **Language Requirements**: ALWAYS communicate, reason, and reply to the user in Chinese (简体中文). This is a strict rule.
- **Do not start implementing right away** unless explicitly told to. Assess the codebase first.
- When fixing issues, **prefer minimal changes** over widespread refactoring.
- If a C++ file fails to compile due to missing headers, check if the dependency is declared in both `CMakeLists.txt` (`find_package`, `ament_target_dependencies`) and `package.xml` (`<depend>`).
- For multi-step tasks involving ROS 2 messaging, clearly define the message in `bishe_msgs` first, then modify the Publisher node, and finally the Subscriber node.
- If asked to create a new node, use the existing nodes (e.g., `bishe_detector/src/detector_node.cpp`) as a boilerplate for structure and naming.
