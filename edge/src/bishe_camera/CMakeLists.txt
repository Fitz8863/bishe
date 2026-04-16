cmake_minimum_required(VERSION 3.8)
project(bishe_camera)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(bishe_msgs REQUIRED)

add_library(bishe_camera_component SHARED
  src/camera_node.cpp
)

target_include_directories(bishe_camera_component PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../bishe_msgs/include>
  $<INSTALL_INTERFACE:include>
  /opt/ros/humble/include
)

ament_target_dependencies(bishe_camera_component
  rclcpp
  rclcpp_components
  sensor_msgs
  image_transport
  cv_bridge
  OpenCV
  bishe_msgs
)

rclcpp_components_register_nodes(bishe_camera_component "bishe_camera::CameraNode")

add_executable(camera_node src/camera_node_main.cpp)
target_include_directories(camera_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../bishe_msgs/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(camera_node bishe_camera_component)
ament_target_dependencies(camera_node rclcpp)

install(TARGETS bishe_camera_component camera_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

ament_package()
