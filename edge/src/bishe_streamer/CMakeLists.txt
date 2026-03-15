cmake_minimum_required(VERSION 3.8)
project(bishe_streamer)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(bishe_msgs REQUIRED)

include_directories(/opt/ros/humble/include)

add_executable(streamer_node src/streamer_node.cpp)

ament_target_dependencies(streamer_node
  rclcpp
  sensor_msgs
  image_transport
  cv_bridge
  OpenCV
  bishe_msgs
)

install(TARGETS streamer_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
