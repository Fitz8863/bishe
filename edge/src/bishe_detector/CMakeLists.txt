cmake_minimum_required(VERSION 3.8)
project(bishe_detector)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
if (NOT TensorRT_DIR)
    set(TensorRT_DIR /home/ad/TensorRT-10.7.0.23/)
endif()

find_package(CUDA REQUIRED)
find_package(TensorRT REQUIRED)
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_gtest REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(bishe_msgs REQUIRED)

add_library(bishe_detector_component SHARED
  src/detector_node.cpp
  src/yolo.cc
  src/yolov8.cc
  src/trt_engine.cc
  src/int8_calibrator.cc
)

target_include_directories(bishe_detector_component PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/bishe_detector>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../bishe_msgs/include>
  $<INSTALL_INTERFACE:include>
  ${CUDA_INCLUDE_DIRS}
  ${TensorRT_INCLUDE_DIRS}
)

ament_target_dependencies(bishe_detector_component
  rclcpp
  rclcpp_components
  sensor_msgs
  image_transport
  cv_bridge
  OpenCV
  bishe_msgs
)

target_link_libraries(bishe_detector_component
  ${CUDA_LIBRARIES}
  ${TensorRT_LIBRARIES}
)

rclcpp_components_register_nodes(bishe_detector_component "bishe_detector::DetectorNode")

add_executable(detector_node src/detector_node_main.cpp)
target_include_directories(detector_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../bishe_msgs/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(detector_node bishe_detector_component)
ament_target_dependencies(detector_node rclcpp)

install(TARGETS bishe_detector_component detector_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
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
      ${CMAKE_CURRENT_SOURCE_DIR}/../bishe_msgs/include
    )
    ament_target_dependencies(test_detection_gate
      sensor_msgs
      bishe_msgs
    )
  endif()

  ament_add_gtest(test_shared_frame_ring test/test_shared_frame_ring.cpp)
  if(TARGET test_shared_frame_ring)
    target_include_directories(test_shared_frame_ring PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/include/bishe_detector
      ${CMAKE_CURRENT_SOURCE_DIR}/../bishe_msgs/include
    )
    ament_target_dependencies(test_shared_frame_ring
      bishe_msgs
    )
  endif()
endif()

ament_package()
