cmake_minimum_required(VERSION 3.8)
project(bishe_monitor)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_gtest REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(bishe_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

add_executable(monitor_node src/monitor_node.cpp)
target_include_directories(monitor_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${CURL_INCLUDE_DIRS}
  ${JSONCPP_INCLUDE_DIRS}
)
target_link_libraries(monitor_node ${CURL_LIBRARIES} ${JSONCPP_LIBRARIES})
ament_target_dependencies(monitor_node
  rclcpp
  std_msgs
  bishe_msgs
  cv_bridge
  OpenCV
)

install(TARGETS monitor_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

install(FILES config/config.yaml
  DESTINATION share/${PROJECT_NAME}/config
)

if(BUILD_TESTING)
  ament_add_gtest(test_async_task_worker test/test_async_task_worker.cpp)
  if(TARGET test_async_task_worker)
    target_include_directories(test_async_task_worker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  endif()

  ament_add_gtest(test_alarm_upload_gate test/test_alarm_upload_gate.cpp)
  if(TARGET test_alarm_upload_gate)
    target_include_directories(test_alarm_upload_gate PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  endif()

  ament_add_gtest(test_alarm_playback_command test/test_alarm_playback_command.cpp)
  if(TARGET test_alarm_playback_command)
    target_include_directories(test_alarm_playback_command PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  endif()

  ament_add_gtest(test_alarm_event_payload test/test_alarm_event_payload.cpp)
  if(TARGET test_alarm_event_payload)
    target_include_directories(test_alarm_event_payload PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${JSONCPP_INCLUDE_DIRS}
    )
    target_link_libraries(test_alarm_event_payload ${JSONCPP_LIBRARIES})
  endif()
endif()

ament_package()
