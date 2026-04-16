#pragma once

#include <memory>

#include <rclcpp/node.hpp>
#include <rclcpp/node_options.hpp>

namespace bishe_detector
{
std::shared_ptr<rclcpp::Node> make_detector_node(
  const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
}
