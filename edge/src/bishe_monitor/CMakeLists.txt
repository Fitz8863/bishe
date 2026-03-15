cmake_minimum_required(VERSION 3.8)
project(bishe_monitor)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(bishe_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

# Find libcurl
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

include_directories(/opt/ros/humble/include ${CURL_INCLUDE_DIRS})

add_executable(monitor_node src/monitor_node.cpp)

ament_target_dependencies(monitor_node
  rclcpp
  bishe_msgs
  cv_bridge
  OpenCV
  CURL
)

install(TARGETS monitor_node
  DESTINATION lib/${PROJECT_NAME}
)

install(FILES config/config.yaml
  DESTINATION share/${PROJECT_NAME}/config
)

ament_package()
