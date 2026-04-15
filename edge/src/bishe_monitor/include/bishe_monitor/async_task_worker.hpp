#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

class AsyncTaskWorker
{
public:
  AsyncTaskWorker()
    : worker_thread_(&AsyncTaskWorker::run, this)
  {
  }

  ~AsyncTaskWorker()
  {
    stop();
  }

  AsyncTaskWorker(const AsyncTaskWorker &) = delete;
  AsyncTaskWorker & operator=(const AsyncTaskWorker &) = delete;

  bool enqueue(std::function<void()> task)
  {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_) {
        return false;
      }
      tasks_.push(std::move(task));
    }
    cv_.notify_one();
    return true;
  }

  void stop()
  {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_) {
        return;
      }
      stopping_ = true;
    }
    cv_.notify_all();

    if (worker_thread_.joinable()) {
      worker_thread_.join();
    }
  }

private:
  void run()
  {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
        if (stopping_ && tasks_.empty()) {
          return;
        }
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      task();
    }
  }

  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> tasks_;
  bool stopping_{false};
  std::thread worker_thread_;
};
