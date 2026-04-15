#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "bishe_monitor/async_task_worker.hpp"

namespace
{
using namespace std::chrono_literals;
}

TEST(AsyncTaskWorkerTest, ExecutesEnqueuedTasks)
{
  AsyncTaskWorker worker;
  std::mutex mutex;
  std::condition_variable cv;
  bool done = false;

  ASSERT_TRUE(worker.enqueue([&]() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      done = true;
    }
    cv.notify_all();
  }));

  std::unique_lock<std::mutex> lock(mutex);
  EXPECT_TRUE(cv.wait_for(lock, 1s, [&]() { return done; }));
}

TEST(AsyncTaskWorkerTest, RejectsNewTasksAfterStop)
{
  AsyncTaskWorker worker;
  worker.stop();

  EXPECT_FALSE(worker.enqueue([]() {}));
}

TEST(AsyncTaskWorkerTest, DrainsQueuedTasksBeforeStopping)
{
  AsyncTaskWorker worker;
  std::atomic<int> executed{0};

  ASSERT_TRUE(worker.enqueue([&]() {
    std::this_thread::sleep_for(50ms);
    executed.fetch_add(1);
  }));

  worker.stop();

  EXPECT_EQ(executed.load(), 1);
}
