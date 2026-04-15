#pragma once

#include <string>

inline std::string escapeForShellDoubleQuotes(const std::string &value)
{
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (char c : value) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') {
      escaped.push_back('\\');
    }
    escaped.push_back(c);
  }
  return escaped;
}

inline std::string buildAlarmPlaybackCommand(const std::string &audio_file)
{
  return "gst-play-1.0 -q \"" + escapeForShellDoubleQuotes(audio_file) + "\"";
}
