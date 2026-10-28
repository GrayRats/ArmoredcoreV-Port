#pragma once
// Best-effort Linux page-cache preloading. No guest memory or cached file bytes.
// One owned worker, bounded queue, duplicated CLOEXEC descriptors pin the inode.
#if defined(__linux__)
#include <algorithm>
#include <condition_variable>
#include <chrono>
#include <pthread.h>
#include <cstdint>
#include <deque>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace acv {
struct ReadAheadFile {
  int fd = -1;
  uint64_t size = 0;
  uint64_t hinted_begin = 0, hinted_end = 0;
  ~ReadAheadFile() { if (fd >= 0) ::close(fd); }
  static std::shared_ptr<ReadAheadFile> Open(int source) {
    const int flags = ::fcntl(source, F_GETFL);
    struct stat info{};
    if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY ||
        ::fstat(source, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0) return {};
    const int copy = ::fcntl(source, F_DUPFD_CLOEXEC, 0);
    if (copy < 0) return {};
    try {
      auto result = std::make_shared<ReadAheadFile>();
      result->fd = copy; result->size = uint64_t(info.st_size);
      return result;
    } catch (...) { ::close(copy); throw; }
  }
};
class AssetReadAhead {
 public:
  using Advisor = std::function<int(int, uint64_t, uint64_t)>;
  struct Stats { uint64_t submitted=0, coalesced=0, dropped=0, completed=0, failed=0, advised_bytes=0; };
  explicit AssetReadAhead(size_t capacity=8, Advisor advisor={})
      : capacity_(std::max(size_t(1),capacity)), advisor_(std::move(advisor)) {
    if (!advisor_) advisor_=[](int fd,uint64_t start,uint64_t bytes) {
      return ::posix_fadvise(fd,off_t(start),off_t(bytes),POSIX_FADV_WILLNEED);
    };
    worker_=std::thread([this]{Run();});
  }
  ~AssetReadAhead() {
    {
      std::lock_guard lock(mutex_); stopping_=true; jobs_.clear();
    }
    work_.notify_all();
    if(worker_.joinable())worker_.join();
  }
  AssetReadAhead(const AssetReadAhead&)=delete;
  AssetReadAhead& operator=(const AssetReadAhead&)=delete;
  static AssetReadAhead& Global() { static AssetReadAhead service; return service; }

  bool Request(const std::shared_ptr<ReadAheadFile>& file,uint64_t begin,uint64_t window) {
    if(!file || !window || begin>=file->size)return false;
    // Limit each hint to128MiB and EOF. No unbounded producer or IO allocation.
    const uint64_t bytes=std::min({window,uint64_t(128)<<20,file->size-begin});
    try {
      std::lock_guard lock(mutex_);
      if(stopping_)return false;
      // Refill only after consuming half the hinted region, or after a seek.
      if(file->hinted_end>file->hinted_begin && begin>=file->hinted_begin &&
         begin<file->hinted_begin+(file->hinted_end-file->hinted_begin)/2)return false;
      auto it=std::find_if(jobs_.begin(),jobs_.end(),[&](const Job& job){return job.file==file;});
      if(it!=jobs_.end()) {
        it->begin=begin;it->bytes=bytes;++stats_.coalesced;
      } else {
        if(jobs_.size()>=capacity_){++stats_.dropped;return false;}
        jobs_.push_back({file,begin,bytes});++stats_.submitted;
      }
      file->hinted_begin=begin;file->hinted_end=begin+bytes;
      work_.notify_one();return true;
    } catch(const std::bad_alloc&) { return false; } // A hint may never fail a real read.
  }
  void SetReporter(std::function<void(const Stats&)> reporter) {
    std::lock_guard lock(mutex_);if(!reporter_)reporter_=std::move(reporter);
  }
  Stats statistics() {std::lock_guard lock(mutex_);return stats_;}
  void WaitIdle() {std::unique_lock lock(mutex_);idle_.wait(lock,[this]{return jobs_.empty()&&!active_;});}
 private:
  struct Job {std::shared_ptr<ReadAheadFile> file;uint64_t begin,bytes;};
  void Run() {
    (void)::pthread_setname_np(::pthread_self(),"Asset preload");
    std::unique_lock lock(mutex_);
    for(;;) {
      const auto now=std::chrono::steady_clock::now();
      if(reporter_ && now>=next_report_) {
        const auto report=reporter_;const auto stats=stats_;
        next_report_=now+std::chrono::seconds(10);
        lock.unlock();try {report(stats);}catch(...) {}lock.lock();
      }
      work_.wait_for(lock,std::chrono::seconds(1),[this]{return stopping_||!jobs_.empty();});
      if(stopping_)return;
      if(jobs_.empty())continue;
      Job job=std::move(jobs_.front());jobs_.pop_front();active_=true;
      lock.unlock();
      int status=-1;
      try {status=advisor_(job.file->fd,job.begin,job.bytes);}catch(...) {}
      // Release the last inode lease outside the queue lock.
      job.file.reset();
      lock.lock();
      ++stats_.completed;
      if(status)++stats_.failed;else stats_.advised_bytes+=job.bytes;
      active_=false;
      if(jobs_.empty())idle_.notify_all();
    }
  }
  const size_t capacity_;
  Advisor advisor_;
  std::mutex mutex_;
  std::condition_variable work_,idle_;
  std::deque<Job> jobs_;
  Stats stats_;
  bool stopping_=false,active_=false;
  std::function<void(const Stats&)> reporter_;
  std::chrono::steady_clock::time_point next_report_=std::chrono::steady_clock::now()+std::chrono::seconds(90);
  std::thread worker_;
};
}  // namespace acv
#endif
