#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "bishe_camera/camera_node_factory.hpp"

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  auto node = bishe_camera::make_camera_node();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
