#include <gtest/gtest.h>

#include <string>

#include "bishe_monitor/alarm_playback_command.hpp"

TEST(AlarmPlaybackCommandTest, EscapesDangerousCharacters)
{
  const std::string input = "a\"b$c`d\\e";
  EXPECT_EQ(escapeForShellDoubleQuotes(input), "a\\\"b\\$c\\`d\\\\e");
}

TEST(AlarmPlaybackCommandTest, BuildsForegroundPlaybackCommandWithoutShellBackgrounding)
{
  const std::string command = buildAlarmPlaybackCommand("/tmp/test alarm.mp3");

  EXPECT_EQ(command, "gst-play-1.0 -q \"/tmp/test alarm.mp3\"");
  EXPECT_EQ(command.find('&'), std::string::npos);
  EXPECT_EQ(command.find(">/dev/null"), std::string::npos);
  EXPECT_EQ(command.find("2>&1"), std::string::npos);
}
