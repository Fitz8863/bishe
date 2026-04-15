#include <gtest/gtest.h>

#include <chrono>

#include "bishe_monitor/alarm_upload_gate.hpp"

namespace
{
using namespace std::chrono_literals;
}

TEST(AlarmUploadGateTest, FirstAlarmDoesNotUploadWhenThresholdGreaterThanOne)
{
  AlarmUploadGate gate(2, true, std::chrono::seconds(30));
  const auto t0 = std::chrono::steady_clock::time_point{};

  const auto result = gate.onAlarmTriggered(t0);

  EXPECT_FALSE(result.should_upload);
  EXPECT_EQ(result.new_count, 1);
}

TEST(AlarmUploadGateTest, ReachingThresholdRequestsUpload)
{
  AlarmUploadGate gate(2, true, std::chrono::seconds(30));
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_FALSE(gate.onAlarmTriggered(t0).should_upload);
  const auto result = gate.onAlarmTriggered(t0 + 5s);

  EXPECT_TRUE(result.should_upload);
  EXPECT_EQ(result.new_count, 2);
}

TEST(AlarmUploadGateTest, UploadCanResetCountWhenConfigured)
{
  AlarmUploadGate gate(2, true, std::chrono::seconds(30));
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_FALSE(gate.onAlarmTriggered(t0).should_upload);
  EXPECT_TRUE(gate.onAlarmTriggered(t0 + 5s).should_upload);

  gate.onUploadCompleted();

  const auto result = gate.onAlarmTriggered(t0 + 10s);
  EXPECT_FALSE(result.should_upload);
  EXPECT_EQ(result.new_count, 1);
}

TEST(AlarmUploadGateTest, QuietTimeoutClearsStaleCountBeforeNextAlarm)
{
  AlarmUploadGate gate(3, true, std::chrono::seconds(10));
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_FALSE(gate.onAlarmTriggered(t0).should_upload);

  const auto result = gate.onAlarmTriggered(t0 + 15s);
  EXPECT_FALSE(result.should_upload);
  EXPECT_EQ(result.new_count, 1);
}

TEST(AlarmUploadGateTest, DisablingQuietTimeoutKeepsCountAcrossLongGaps)
{
  AlarmUploadGate gate(2, true, std::chrono::seconds(0));
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_FALSE(gate.onAlarmTriggered(t0).should_upload);

  const auto result = gate.onAlarmTriggered(t0 + 2min);
  EXPECT_TRUE(result.should_upload);
  EXPECT_EQ(result.new_count, 2);
}
