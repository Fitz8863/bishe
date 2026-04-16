#include <gtest/gtest.h>

#include <vector>

#include "bishe_msgs/shared_frame_ring.hpp"

TEST(SharedFrameRingTest, WritesAndReadsSlotPayload)
{
  bishe_msgs::shared_memory::SharedFrameRingConfig config;
  config.shm_name = "/bishe_test_ring";
  config.slot_count = 2;
  config.width = 4;
  config.height = 2;
  config.channels = 3;

  bishe_msgs::shared_memory::SharedFrameRing::unlink(config.shm_name);
  bishe_msgs::shared_memory::SharedFrameRing writer(config, true);
  bishe_msgs::shared_memory::SharedFrameRing reader(config, false);

  const std::vector<uint8_t> payload = {
      1, 2, 3, 4, 5, 6,
      7, 8, 9, 10, 11, 12,
      13, 14, 15, 16, 17, 18,
      19, 20, 21, 22, 23, 24,
  };

  ASSERT_TRUE(writer.write(1, 42, payload.data(), payload.size()));
  ASSERT_TRUE(reader.acquire(1, 42));

  std::vector<uint8_t> readback(payload.size());
  ASSERT_TRUE(reader.read(1, 42, readback.data(), readback.size()));
  reader.release(1);
  EXPECT_EQ(readback, payload);
  bishe_msgs::shared_memory::SharedFrameRing::unlink(config.shm_name);
}
