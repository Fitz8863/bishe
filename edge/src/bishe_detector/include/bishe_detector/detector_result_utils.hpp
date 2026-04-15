#pragma once

#include <sensor_msgs/msg/image.hpp>

#include "bishe_msgs/msg/detector_result.hpp"

inline bishe_msgs::msg::DetectorResult buildPassThroughResult(
    const sensor_msgs::msg::Image::ConstSharedPtr &image,
    float nms_threshold)
{
  bishe_msgs::msg::DetectorResult result;
  result.has_violation = false;
  result.confidence = 0.0f;
  result.nms_threshold = nms_threshold;
  result.violation_type = "";
  result.annotated_image = *image;
  return result;
}
