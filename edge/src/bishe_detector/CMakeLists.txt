cmake_minimum_required(VERSION 3.8)
project(bishe_detector)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# TensorRT CMake logic
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
if (NOT TensorRT_DIR)
    set(TensorRT_DIR /home/ad/TensorRT-10.7.0.23/)
endif()

find_package(CUDA REQUIRED)
find_package(TensorRT REQUIRED)

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_gtest REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

# Find bishe_msgs
find_package(bishe_msgs REQUIRED)

add_executable(detector_node 
  src/detector_node.cpp
  src/yolo.cc
  src/yolov8.cc
  src/trt_engine.cc
  src/int8_calibrator.cc
)

target_include_directories(detector_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/bishe_detector>
  $<INSTALL_INTERFACE:include>
  ${CUDA_INCLUDE_DIRS}
  ${TensorRT_INCLUDE_DIRS}
)

ament_target_dependencies(detector_node
  rclcpp
  sensor_msgs
  image_transport
  cv_bridge
  OpenCV
  bishe_msgs
)

# Use plain signature to match ament_target_dependencies
target_link_libraries(detector_node
  ${CUDA_LIBRARIES}
  ${TensorRT_LIBRARIES}
)

install(TARGETS detector_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

if(BUILD_TESTING)
  ament_add_gtest(test_detection_gate test/test_detection_gate.cpp)
  if(TARGET test_detection_gate)
    target_include_directories(test_detection_gate PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/include/bishe_detector
    )
    ament_target_dependencies(test_detection_gate
      sensor_msgs
      bishe_msgs
    )
  endif()
endif()

ament_package()
