# 🤖 Agent Guidelines for `bishe` Workspace (ROS 2 Humble)

This workspace contains the edge-side detection code running on Jetson Orin Nano for the Chemical Plant Hazard Detection System.

## 🏗️ Environment & Architecture
- **Framework**: ROS 2 Humble (Ubuntu 22.04).
- **Primary Languages**: C++17 and Python 3.10.
- **Build System**: Colcon with `ament_cmake` or `ament_python`.
- **Key Dependencies**: TensorRT 10.7, OpenCV (CUDA), Paho MQTT C++.

### ⚠️ Critical Environment Quirks
- **TensorRT Path**: Hardcoded in `bishe_detector/CMakeLists.txt` as `/home/ad/TensorRT-10.7.0.23/`.
- **Dependency Chain**: `bishe_msgs` is the core dependency. **You MUST rebuild `bishe_msgs` first** after any `.msg` changes.
- **MQTT Bridge**: `mqtt_node` bridges ROS topics (like `/alarm/event`) to MQTT. Ensure subscriptions are initialized in constructors.
- **Default Device**: `jetson-orin-nano`.

## 🛠️ Build & Test Commands
Always run from the workspace root (`/home/jetson/projects/bishe/edge`).

- **Build All**: `colcon build --symlink-install`
- **Build Single Package**: `colcon build --packages-select <package_name> --symlink-install`
- **Build with Compile Commands**: `colcon build --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
- **Source Environment**: `source install/setup.bash`
- **Test All**: `colcon test`

## 📝 Code Style & Conventions

### 1. General Rules
- **Language**: Logic and variable names MUST be in English.
- **Comments**: **Chinese comments are strongly encouraged** for clarity.
- **Safety**: Ensure all background threads and timers are explicitly joined/stopped in destructors.

### 2. C++ Guidelines (ROS 2 Style)
- **Formatting**: 2-space indentation. Braces on new lines for classes/functions, but often same line for control flow in existing code.
- **Naming**:
  - `CamelCase`: Classes/Structs (e.g., `DetectorNode`).
  - `snake_case`: Variables, functions, namespaces.
  - `member_variable_`: Trailing underscore for class members.
  - `kConstantName`: 'k' prefix for constants.
- **Includes**:
  1. Standard library (`<vector>`, `<string>`)
  2. ROS 2 headers (`<rclcpp/rclcpp.hpp>`)
  3. Other libraries (`<opencv2/opencv.hpp>`, `<NvInfer.h>`)
  4. Project headers (`"utils.h"`)
- **Paradigms**:
  - Inheritance: Use `public rclcpp::Node`.
  - Pointers: Use `std::unique_ptr` for ownership, `SharedPtr` for ROS interfaces.
  - Parameters: Declare and get in constructor (`this->declare_parameter`).
- **Error Handling**: Use `try-catch` for library calls (OpenCV, MQTT). Use `throw std::runtime_error` for fatal init failures. Use `CUDA_CHECK` macro for CUDA calls.

### 3. Python Guidelines
- **Standard**: PEP 8. 4-space indentation.
- **Launch Files**: Modern Python launch files (`.launch.py`). Return `LaunchDescription`.
- **Type Hints**: Use `def func(a: int) -> bool:` for clarity.

### 4. ROS 2 Patterns
- **Logging**: Use `RCLCPP_INFO`, `RCLCPP_ERROR`, etc. Avoid `std::cout`.
- **QoS**: Use `rmw_qos_profile_sensor_data` for image streams.
- **Communication**: Custom messages are in `bishe_msgs`. Rebuild it first after changes.
- **Execution**: Use `rclcpp::TimerBase` for periodic tasks, `std::thread` for heavy background processing (like `DetectorNode` worker threads).

## 🤖 AI Agent Instructions
- **Response Language**: ALWAYS reply to the user in **Chinese (简体中文)**.
- **Context First**: Read `package.xml` and `CMakeLists.txt` before proposing dependency changes.
- **Minimalism**: Prefer small, focused fixes. Avoid large refactors unless requested.
- **Safety**: Ensure all background threads/timers are properly shut down in destructors.
- **Boilerplate**: Use `detector_node.cpp` as a reference for complex node structure (threading, queues, parameters).
