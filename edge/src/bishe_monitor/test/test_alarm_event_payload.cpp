#include <gtest/gtest.h>

#include <jsoncpp/json/json.h>

#include <sstream>

#include "bishe_monitor/alarm_event_payload.hpp"

TEST(AlarmEventPayloadTest, BuildsParseablePayloadWithExpectedFields)
{
  const auto payload = buildAlarmEventPayload("001", "生产车间A区", "smoking", 123456789LL);

  Json::CharReaderBuilder reader;
  Json::Value root;
  std::string errors;
  std::istringstream input(payload);

  ASSERT_TRUE(Json::parseFromStream(reader, input, &root, &errors)) << errors;
  EXPECT_EQ(root["camera_id"].asString(), "001");
  EXPECT_EQ(root["location"].asString(), "生产车间A区");
  EXPECT_EQ(root["alarm_type"].asString(), "smoking");
  EXPECT_EQ(root["timestamp_ns"].asInt64(), 123456789LL);
}

TEST(AlarmEventPayloadTest, KeepsEnglishAlarmTypeUnchanged)
{
  const auto payload = buildAlarmEventPayload("002", "B区", "fire", 42LL);

  Json::CharReaderBuilder reader;
  Json::Value root;
  std::string errors;
  std::istringstream input(payload);

  ASSERT_TRUE(Json::parseFromStream(reader, input, &root, &errors)) << errors;
  EXPECT_EQ(root["alarm_type"].asString(), "fire");
}
