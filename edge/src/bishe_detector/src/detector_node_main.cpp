#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "bishe_detector/detector_node_factory.hpp"

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  auto node = bishe_detector::make_detector_node();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
