#pragma once
// Include after SDK Vulkan feature macros. Optional cache, owned by one device.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace acv {
class PersistentPipelineCache {
  static constexpr size_t kLimit = 64 * 1024 * 1024;
  static constexpr uint64_t kMagic = 0x3143565043564341ULL;
  VkDevice device_;
  VkPhysicalDeviceProperties properties_;
  VkPipelineCache cache_{};
  PFN_vkCreatePipelineCache create_;
  PFN_vkGetPipelineCacheData get_;
  PFN_vkDestroyPipelineCache destroy_;
  std::string path_;
  int lock_ = -1;
  std::mutex mutex_;
  std::condition_variable condition_;
  std::thread worker_;
  bool pending_ = false, stopping_ = false;
  uint64_t creates_ = 0;
  std::chrono::steady_clock::time_point saved_{};
  static uint64_t Hash(const uint8_t *p, size_t n) {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < n; ++i) {
      h ^= p[i];
      h *= 1099511628211ULL;
    }
    return h;
  }
  bool Compatible(const std::vector<uint8_t> &data) const {
    if (data.size() < 32)
      return false;
    uint32_t h[4];
    std::memcpy(h, data.data(), 16);
    return h[0] >= 32 && h[0] <= data.size() &&
           h[1] == VK_PIPELINE_CACHE_HEADER_VERSION_ONE &&
           h[2] == properties_.vendorID && h[3] == properties_.deviceID &&
           !std::memcmp(data.data() + 16, properties_.pipelineCacheUUID,
                        VK_UUID_SIZE);
  }

public:
  std::atomic<VkResult> last_result{VK_SUCCESS};
  bool loaded = false;
  std::atomic<bool> write_failed{false};
  PersistentPipelineCache(VkDevice device,
                          const VkPhysicalDeviceProperties &properties,
                          PFN_vkGetDeviceProcAddr proc, const char *path)
      : device_(device), properties_(properties), path_(path) {
    create_ = reinterpret_cast<PFN_vkCreatePipelineCache>(
        proc(device, "vkCreatePipelineCache"));
    get_ = reinterpret_cast<PFN_vkGetPipelineCacheData>(
        proc(device, "vkGetPipelineCacheData"));
    destroy_ = reinterpret_cast<PFN_vkDestroyPipelineCache>(
        proc(device, "vkDestroyPipelineCache"));
    if (!create_ || !get_ || !destroy_) {
      last_result = VK_ERROR_INITIALIZATION_FAILED;
      return;
    }
    lock_ = open((path_ + ".lock").c_str(),
                 O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (lock_ >= 0 && flock(lock_, LOCK_EX | LOCK_NB)) {
      close(lock_);
      lock_ = -1;
    }
    std::vector<uint8_t> data;
    int fd = open(path_.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd >= 0) {
      struct stat st{};
      std::array<uint64_t, 3> header{};
      if (!fstat(fd, &st) && S_ISREG(st.st_mode) &&
          st.st_size >= int64_t(sizeof(header)) &&
          st.st_size <= int64_t(kLimit + sizeof(header)) &&
          read(fd, header.data(), sizeof(header)) == ssize_t(sizeof(header)) &&
          header[0] == kMagic &&
          header[1] == uint64_t(st.st_size - sizeof(header))) {
        try {
          data.resize(size_t(header[1]));
          size_t done = 0;
          while (done < data.size()) {
            auto n = read(fd, data.data() + done, data.size() - done);
            if (n <= 0)
              break;
            done += size_t(n);
          }
          if (done != data.size() ||
              Hash(data.data(), data.size()) != header[2] || !Compatible(data))
            data.clear();
        } catch (...) {
          data.clear();
          write_failed = true;
        }
      }
      close(fd);
    }
    VkPipelineCacheCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    ci.initialDataSize = data.size();
    ci.pInitialData = data.empty() ? nullptr : data.data();
    last_result = create_(device_, &ci, nullptr, &cache_);
    loaded = last_result == VK_SUCCESS && !data.empty();
    if (last_result != VK_SUCCESS) {
      cache_ = VK_NULL_HANDLE;
      if (!data.empty() && last_result != VK_ERROR_DEVICE_LOST) {
        ci.initialDataSize = 0;
        ci.pInitialData = nullptr;
        last_result = create_(device_, &ci, nullptr, &cache_);
        if (last_result != VK_SUCCESS)
          cache_ = VK_NULL_HANDLE;
      }
    }
    if (cache_ && lock_ >= 0) {
      try {
        worker_ = std::thread([this] { Writer(); });
      } catch (...) {
        write_failed = true;
      }
    }
  }
  ~PersistentPipelineCache() {
    if (worker_.joinable()) {
      {
        std::lock_guard<std::mutex> guard(mutex_);
        stopping_ = true;
        pending_ = true;
      }
      condition_.notify_one();
      worker_.join();
    }
    if (cache_)
      destroy_(device_, cache_, nullptr);
    if (lock_ >= 0)
      close(lock_);
  }
  PersistentPipelineCache(const PersistentPipelineCache &) = delete;
  PersistentPipelineCache &operator=(const PersistentPipelineCache &) = delete;
  VkPipelineCache handle() const { return cache_; }
  // Called after a successful pipeline creation. No GPU waits. Saves
  // periodically because the ACV host may hard-exit without running renderer
  // destructors.
  bool writable() const { return lock_ >= 0; }
  void Created() noexcept {
    try {
      std::lock_guard<std::mutex> guard(mutex_);
      ++creates_;
      if (worker_.joinable()) {
        pending_ = true;
        condition_.notify_one();
      }
    } catch (...) {
      write_failed = true;
    }
  }
  void Save() noexcept {
    try {
      std::lock_guard<std::mutex> guard(mutex_);
      if (worker_.joinable()) {
        pending_ = true;
        condition_.notify_one();
      }
    } catch (...) {
      write_failed = true;
    }
  }

private:
  void Writer() noexcept {
    std::unique_lock<std::mutex> guard(mutex_);
    for (;;) {
      condition_.wait(guard, [this] { return pending_ || stopping_; });
      if (!stopping_ && saved_ != std::chrono::steady_clock::time_point{})
        condition_.wait_until(guard, saved_ + std::chrono::seconds(5),
                              [this] { return stopping_; });
      bool stop = stopping_;
      pending_ = false;
      saved_ = std::chrono::steady_clock::now();
      guard.unlock();
      try {
        SaveLocked();
      } catch (...) {
        write_failed = true;
      }
      guard.lock();
      if (stop)
        break;
      if (last_result == VK_INCOMPLETE)
        pending_ = true;
    }
  }
  void SaveLocked() {
    if (!cache_ || lock_ < 0)
      return;
    size_t size = 0;
    last_result = get_(device_, cache_, &size, nullptr);
    if (last_result != VK_SUCCESS || !size || size > kLimit) {
      write_failed = true;
      return;
    }
    std::vector<uint8_t> data(size);
    last_result = get_(device_, cache_, &size, data.data());
    if (last_result != VK_SUCCESS || size > data.size()) {
      write_failed = true;
      return;
    }
    data.resize(size);
    if (!Compatible(data))
      return;
    std::array<uint64_t, 3> header{kMagic, uint64_t(data.size()),
                                   Hash(data.data(), data.size())};
    std::string temp = path_ + ".tmp.XXXXXX";
    std::vector<char> name(temp.begin(), temp.end());
    name.push_back(0);
    int fd = mkstemp(name.data());
    if (fd < 0) {
      write_failed = true;
      return;
    }
    bool ok = true;
    auto put = [&](const void *ptr, size_t n) {
      auto *bytes = static_cast<const uint8_t *>(ptr);
      while (n) {
        auto count = write(fd, bytes, n);
        if (count <= 0) {
          ok = false;
          return;
        }
        bytes += count;
        n -= size_t(count);
      }
    };
    put(header.data(), sizeof(header));
    if (ok)
      put(data.data(), data.size());
    if (ok && fsync(fd))
      ok = false;
    if (close(fd))
      ok = false;
    if (ok && rename(name.data(), path_.c_str()))
      ok = false;
    if (!ok) {
      unlink(name.data());
      write_failed = true;
    }
  }
};
} // namespace acv
