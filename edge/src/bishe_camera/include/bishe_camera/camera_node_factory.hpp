#pragma once

#include <memory>

#include <rclcpp/node.hpp>
#include <rclcpp/node_options.hpp>

namespace bishe_camera
{
std::shared_ptr<rclcpp::Node> make_camera_node(
  const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
}
