#pragma once

#include <jsoncpp/json/json.h>

#include <cstdint>
#include <string>

inline std::string buildAlarmEventPayload(
  const std::string &camera_id,
  const std::string &location,
  const std::string &alarm_type,
  std::int64_t timestamp_ns)
{
  Json::Value root(Json::objectValue);
  root["camera_id"] = camera_id;
  root["location"] = location;
  root["alarm_type"] = alarm_type;
  root["timestamp_ns"] = Json::Int64(timestamp_ns);

  Json::StreamWriterBuilder writer;
  writer["indentation"] = "";
  return Json::writeString(writer, root);
}
