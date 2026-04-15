#pragma once

#include <algorithm>
#include <chrono>

enum class DetectionMode
{
  Sampling,
  Locked,
};

class DetectionGate
{
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Duration = std::chrono::milliseconds;

  DetectionGate(Duration sampling_interval, Duration lock_duration)
  {
    update_config(sampling_interval, lock_duration, TimePoint{});
    next_sample_time_ = TimePoint::min();
  }

  bool should_process(TimePoint now)
  {
    if (mode_ == DetectionMode::Locked) {
      if (now < locked_until_) {
        return true;
      }
      mode_ = DetectionMode::Sampling;
      next_sample_time_ = locked_until_ + sampling_interval_;
      return false;
    }

    if (next_sample_time_ == TimePoint::min() || now >= next_sample_time_) {
      next_sample_time_ = now + sampling_interval_;
      return true;
    }

    return false;
  }

  void on_detection(TimePoint now)
  {
    mode_ = DetectionMode::Locked;
    locked_until_ = now + lock_duration_;
  }

  void update_config(Duration sampling_interval, Duration lock_duration, TimePoint now)
  {
    sampling_interval_ = std::max(Duration{1}, sampling_interval);
    lock_duration_ = std::max(Duration{0}, lock_duration);

    if (mode_ == DetectionMode::Sampling && next_sample_time_ != TimePoint::min()) {
      next_sample_time_ = now + sampling_interval_;
    }
    if (mode_ == DetectionMode::Locked) {
      locked_until_ = now + lock_duration_;
    }
  }

  DetectionMode mode() const
  {
    return mode_;
  }

private:
  DetectionMode mode_{DetectionMode::Sampling};
  Duration sampling_interval_{1000};
  Duration lock_duration_{3000};
  TimePoint next_sample_time_{TimePoint::min()};
  TimePoint locked_until_{TimePoint::min()};
};
