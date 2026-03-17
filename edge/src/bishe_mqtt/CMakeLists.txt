cmake_minimum_required(VERSION 3.8)
project(bishe_mqtt)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(rclcpp REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# MQTT - use system installed EMQX SDK
set(MQTT_LIBRARIES paho-mqtt3as paho-mqttpp3)

add_executable(mqtt_node src/mqtt_node.cpp)

target_include_directories(mqtt_node PUBLIC ${JSONCPP_INCLUDE_DIRS})

target_link_libraries(mqtt_node ${MQTT_LIBRARIES} ${JSONCPP_LIBRARIES})

ament_target_dependencies(mqtt_node
  rclcpp
)

install(TARGETS mqtt_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
