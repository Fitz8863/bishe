#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace bishe_msgs::shared_memory
{
struct SharedFrameRingConfig
{
  std::string shm_name;
  uint32_t slot_count{8};
  uint32_t width{640};
  uint32_t height{360};
  uint32_t channels{3};
};

enum class SlotState : uint32_t
{
  Free = 0,
  Writing = 1,
  Ready = 2,
  InUse = 3,
};

struct SharedFrameRingHeader
{
  uint32_t magic;
  uint32_t version;
  uint32_t slot_count;
  uint32_t width;
  uint32_t height;
  uint32_t channels;
  uint32_t bytes_per_frame;
};

struct SharedFrameSlotHeader
{
  uint32_t state;
  uint64_t sequence;
  uint32_t bytes_used;
  uint32_t width;
  uint32_t height;
  uint32_t step;
  char encoding[16];
};

struct SharedFrameView
{
  const uint8_t *data{nullptr};
  uint32_t bytes_used{0};
  uint32_t width{0};
  uint32_t height{0};
  uint32_t step{0};
  std::string encoding;
};

class SharedFrameRing
{
public:
  static constexpr uint32_t kMagic = 0x4253484d;
  static constexpr uint32_t kVersion = 1;

  SharedFrameRing(const SharedFrameRingConfig &config, bool initialize_if_needed)
  : config_(config), initialize_if_needed_(initialize_if_needed)
  {
    if (config_.shm_name.empty() || config_.shm_name.front() != '/') {
      throw std::runtime_error("shared memory name must start with '/'");
    }
    fd_ = shm_open(config_.shm_name.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd_ < 0) {
      throw std::runtime_error("failed to open shared memory");
    }

    total_size_ = computeTotalSize(config_);
    if (ftruncate(fd_, static_cast<off_t>(total_size_)) != 0) {
      close(fd_);
      throw std::runtime_error("failed to resize shared memory");
    }

    mapping_ = mmap(nullptr, total_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED) {
      close(fd_);
      throw std::runtime_error("failed to mmap shared memory");
    }

    header_ = static_cast<SharedFrameRingHeader *>(mapping_);
    if (initialize_if_needed_ || header_->magic != kMagic || header_->version != kVersion) {
      initialize();
    }
  }

  ~SharedFrameRing()
  {
    if (mapping_ && mapping_ != MAP_FAILED) {
      munmap(mapping_, total_size_);
    }
    if (fd_ >= 0) {
      close(fd_);
    }
  }

  SharedFrameRing(const SharedFrameRing &) = delete;
  SharedFrameRing &operator=(const SharedFrameRing &) = delete;

  static void unlink(const std::string &shm_name)
  {
    if (!shm_name.empty()) {
      shm_unlink(shm_name.c_str());
    }
  }

  bool write(uint32_t slot_index, uint64_t sequence, const uint8_t *data, size_t bytes_used)
  {
    auto *slot = slotHeader(slot_index);
    if (slot->state != static_cast<uint32_t>(SlotState::Free)) {
      return false;
    }
    slot->state = static_cast<uint32_t>(SlotState::Writing);
    std::memcpy(slotData(slot_index), data, bytes_used);
    slot->bytes_used = static_cast<uint32_t>(bytes_used);
    slot->width = header_->width;
    slot->height = header_->height;
    slot->step = header_->width * header_->channels;
    std::memset(slot->encoding, 0, sizeof(slot->encoding));
    std::strncpy(slot->encoding, "bgr8", sizeof(slot->encoding) - 1);
    slot->sequence = sequence;
    slot->state = static_cast<uint32_t>(SlotState::Ready);
    return true;
  }

  bool writeNext(
    uint64_t sequence,
    const uint8_t *data,
    uint32_t bytes_used,
    uint32_t width,
    uint32_t height,
    uint32_t step,
    const std::string &encoding,
    uint32_t &slot_index)
  {
    for (uint32_t attempt = 0; attempt < config_.slot_count; ++attempt) {
      const uint32_t idx = next_write_index_++ % config_.slot_count;
      auto *slot = slotHeader(idx);
      if (slot->state != static_cast<uint32_t>(SlotState::Free)) {
        continue;
      }
      slot->state = static_cast<uint32_t>(SlotState::Writing);
      std::memcpy(slotData(idx), data, bytes_used);
      slot->bytes_used = bytes_used;
      slot->width = width;
      slot->height = height;
      slot->step = step;
      std::memset(slot->encoding, 0, sizeof(slot->encoding));
      std::strncpy(slot->encoding, encoding.c_str(), sizeof(slot->encoding) - 1);
      slot->sequence = sequence;
      slot->state = static_cast<uint32_t>(SlotState::Ready);
      slot_index = idx;
      return true;
    }
    return false;
  }

  bool acquire(uint32_t slot_index, uint64_t sequence)
  {
    auto *slot = slotHeader(slot_index);
    if (slot->sequence != sequence) {
      return false;
    }
    if (slot->state != static_cast<uint32_t>(SlotState::Ready)) {
      return false;
    }
    slot->state = static_cast<uint32_t>(SlotState::InUse);
    return true;
  }

  bool read(uint32_t slot_index, uint64_t sequence, void *buffer, size_t buffer_size) const
  {
    SharedFrameView view;
    if (!viewSlot(slot_index, sequence, view)) {
      return false;
    }
    if (buffer_size < view.bytes_used) {
      return false;
    }
    std::memcpy(buffer, view.data, view.bytes_used);
    return true;
  }

  bool viewSlot(uint32_t slot_index, uint64_t sequence, SharedFrameView &view) const
  {
    const auto *slot = slotHeader(slot_index);
    if (slot->state != static_cast<uint32_t>(SlotState::InUse)) {
      return false;
    }
    if (slot->sequence != sequence) {
      return false;
    }
    view.data = slotData(slot_index);
    view.bytes_used = slot->bytes_used;
    view.width = slot->width;
    view.height = slot->height;
    view.step = slot->step;
    view.encoding = slot->encoding;
    return true;
  }

  void release(uint32_t slot_index)
  {
    auto *slot = slotHeader(slot_index);
    slot->state = static_cast<uint32_t>(SlotState::Free);
  }

private:
  static size_t computeTotalSize(const SharedFrameRingConfig &config)
  {
    const size_t bytes_per_frame = static_cast<size_t>(config.width) * config.height * config.channels;
    return sizeof(SharedFrameRingHeader) +
      config.slot_count * (sizeof(SharedFrameSlotHeader) + bytes_per_frame);
  }

  void initialize()
  {
    const auto bytes_per_frame = static_cast<uint32_t>(config_.width * config_.height * config_.channels);
    header_->magic = kMagic;
    header_->version = kVersion;
    header_->slot_count = config_.slot_count;
    header_->width = config_.width;
    header_->height = config_.height;
    header_->channels = config_.channels;
    header_->bytes_per_frame = bytes_per_frame;
    for (uint32_t i = 0; i < config_.slot_count; ++i) {
      auto *slot = slotHeader(i);
      slot->state = static_cast<uint32_t>(SlotState::Free);
      slot->sequence = 0;
      slot->bytes_used = 0;
      slot->width = 0;
      slot->height = 0;
      slot->step = 0;
      std::memset(slot->encoding, 0, sizeof(slot->encoding));
    }
  }

  SharedFrameSlotHeader *slotHeader(uint32_t slot_index) const
  {
    auto *base = reinterpret_cast<uint8_t *>(mapping_) + sizeof(SharedFrameRingHeader);
    const size_t slot_stride = sizeof(SharedFrameSlotHeader) + header_->bytes_per_frame;
    return reinterpret_cast<SharedFrameSlotHeader *>(base + slot_index * slot_stride);
  }

  uint8_t *slotData(uint32_t slot_index) const
  {
    return reinterpret_cast<uint8_t *>(slotHeader(slot_index) + 1);
  }

  SharedFrameRingConfig config_;
  bool initialize_if_needed_{false};
  int fd_{-1};
  void *mapping_{nullptr};
  size_t total_size_{0};
  SharedFrameRingHeader *header_{nullptr};
  uint32_t next_write_index_{0};
};
}  // namespace bishe_msgs::shared_memory
