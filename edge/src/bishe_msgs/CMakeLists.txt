cmake_minimum_required(VERSION 3.8)
project(bishe_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

# Message generation
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/DetectorResult.msg"
  "msg/SharedFrameRef.msg"
  DEPENDENCIES sensor_msgs std_msgs
)

install(DIRECTORY include/
  DESTINATION include
)

ament_export_include_directories(include)
ament_export_dependencies(rosidl_default_runtime)

ament_package()
