#include <gtest/gtest.h>

#include <chrono>

#include <sensor_msgs/msg/image.hpp>

#include "bishe_detector/detector_result_utils.hpp"
#include "bishe_detector/detection_gate.hpp"

namespace
{
using namespace std::chrono_literals;
}

TEST(DetectionGateTest, AllowsFirstFrameImmediately)
{
  DetectionGate gate(1000ms, 3000ms);
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_TRUE(gate.should_process(t0));
  EXPECT_EQ(gate.mode(), DetectionMode::Sampling);
}

TEST(DetectionGateTest, SkipsFramesUntilSamplingIntervalExpires)
{
  DetectionGate gate(1000ms, 3000ms);
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_TRUE(gate.should_process(t0));
  EXPECT_FALSE(gate.should_process(t0 + 400ms));
  EXPECT_TRUE(gate.should_process(t0 + 1000ms));
}

TEST(DetectionGateTest, DetectionEntersLockedModeAndAllowsAllFrames)
{
  DetectionGate gate(1000ms, 3000ms);
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_TRUE(gate.should_process(t0));
  gate.on_detection(t0);

  EXPECT_EQ(gate.mode(), DetectionMode::Locked);
  EXPECT_TRUE(gate.should_process(t0 + 500ms));
  EXPECT_TRUE(gate.should_process(t0 + 2500ms));
}

TEST(DetectionGateTest, DetectionDuringLockRefreshesDeadline)
{
  DetectionGate gate(1000ms, 3000ms);
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_TRUE(gate.should_process(t0));
  gate.on_detection(t0);
  EXPECT_TRUE(gate.should_process(t0 + 2500ms));

  gate.on_detection(t0 + 2500ms);

  EXPECT_TRUE(gate.should_process(t0 + 5200ms));
  EXPECT_FALSE(gate.should_process(t0 + 5600ms));
  EXPECT_EQ(gate.mode(), DetectionMode::Sampling);
}

TEST(DetectionGateTest, ReturnsToSamplingAfterLockExpiresWithoutNewDetection)
{
  DetectionGate gate(1000ms, 3000ms);
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_TRUE(gate.should_process(t0));
  gate.on_detection(t0);

  EXPECT_TRUE(gate.should_process(t0 + 2900ms));
  EXPECT_FALSE(gate.should_process(t0 + 3100ms));
  EXPECT_EQ(gate.mode(), DetectionMode::Sampling);
  EXPECT_TRUE(gate.should_process(t0 + 4000ms));
}

TEST(DetectionGateTest, UpdatingConfigAppliesNewIntervals)
{
  DetectionGate gate(1000ms, 3000ms);
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_TRUE(gate.should_process(t0));
  gate.update_config(200ms, 500ms, t0);

  EXPECT_TRUE(gate.should_process(t0 + 200ms));
  gate.on_detection(t0 + 200ms);
  EXPECT_TRUE(gate.should_process(t0 + 650ms));
  EXPECT_FALSE(gate.should_process(t0 + 750ms));
}

TEST(DetectorResultUtilsTest, BuildsPassThroughResultFromRawFrame)
{
  auto image = std::make_shared<sensor_msgs::msg::Image>();
  image->header.frame_id = "camera_frame";
  image->header.stamp.sec = 123;
  image->header.stamp.nanosec = 456;
  image->height = 2;
  image->width = 2;
  image->encoding = "bgr8";
  image->is_bigendian = false;
  image->step = 6;
  image->data = {
      1, 2, 3, 4, 5, 6,
      7, 8, 9, 10, 11, 12,
  };

  const auto result = buildPassThroughResult(image, 0.35f);

  EXPECT_FALSE(result.has_violation);
  EXPECT_FLOAT_EQ(result.confidence, 0.0f);
  EXPECT_FLOAT_EQ(result.nms_threshold, 0.35f);
  EXPECT_TRUE(result.violation_type.empty());
  EXPECT_EQ(result.annotated_image.header.frame_id, image->header.frame_id);
  EXPECT_EQ(result.annotated_image.header.stamp.sec, image->header.stamp.sec);
  EXPECT_EQ(result.annotated_image.header.stamp.nanosec, image->header.stamp.nanosec);
  EXPECT_EQ(result.annotated_image.encoding, image->encoding);
  EXPECT_EQ(result.annotated_image.height, image->height);
  EXPECT_EQ(result.annotated_image.width, image->width);
  EXPECT_EQ(result.annotated_image.data, image->data);
}
