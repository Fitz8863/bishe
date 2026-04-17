#pragma once

#include <algorithm>
#include <chrono>

struct AlarmGateResult
{
  bool should_upload{false};
  int new_count{0};
};

class AlarmUploadGate
{
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Duration = std::chrono::seconds;

  AlarmUploadGate(int upload_after_alarm_count, bool reset_after_upload, Duration quiet_timeout)
  {
    updateConfig(upload_after_alarm_count, reset_after_upload, quiet_timeout);
  }

  AlarmGateResult onAlarmTriggered(TimePoint now)
  {
    if (quiet_timeout_.count() > 0 &&
        last_alarm_time_ != TimePoint::min() &&
        (now - last_alarm_time_) > quiet_timeout_) {
      alarm_trigger_count_ = 0;
    }

    last_alarm_time_ = now;
    ++alarm_trigger_count_;

    return AlarmGateResult{
      alarm_trigger_count_ >= upload_after_alarm_count_,
      alarm_trigger_count_
    };
  }

  void onUploadCompleted()
  {
    if (reset_after_upload_) {
      alarm_trigger_count_ = 0;
    }
  }

  void updateConfig(int upload_after_alarm_count, bool reset_after_upload, Duration quiet_timeout)
  {
    upload_after_alarm_count_ = std::max(1, upload_after_alarm_count);
    reset_after_upload_ = reset_after_upload;
    quiet_timeout_ = quiet_timeout.count() < 0 ? Duration{0} : quiet_timeout;
  }

  int currentCount() const
  {
    return alarm_trigger_count_;
  }

private:
  int upload_after_alarm_count_{2};
  bool reset_after_upload_{true};
  Duration quiet_timeout_{Duration{30}};
  int alarm_trigger_count_{0};
  TimePoint last_alarm_time_{TimePoint::min()};
};
