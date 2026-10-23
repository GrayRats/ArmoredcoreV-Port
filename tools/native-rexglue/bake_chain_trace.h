// Opt-in bounded cross-thread CPU/Resolve correlation. One file per DSO domain.
#pragma once
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
class AcvBakeChainTrace {
 public:
  explicit AcvBakeChainTrace(const char* domain) {
    const char* base=std::getenv("ACV_TRACE_BAKE_CHAIN");
    if(!base || !*base)return;
    if(*base!='/') {
      std::fprintf(stderr,"[acv-bake] Absolute unique output prefix required\n");return;
    }
    const std::string name=std::string(base)+"."+domain+".jsonl";
    int fd=::open(name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    if(fd<0) {std::fprintf(stderr,"[acv-bake] Cannot create trace: errno=%d\n",errno);return;}
    file_=::fdopen(fd,"w");
    if(!file_) {::close(fd);return;}
    enabled_.store(true);
  }
  ~AcvBakeChainTrace() {if(file_)std::fclose(file_);}
  bool enabled() const {return enabled_.load(std::memory_order_relaxed);}
  void Record(const char* format,...) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!enabled())return;
    if(sequence_>=4096) {
      std::fputs("{\"event\":\"truncated\",\"limit\":4096}\n",file_);
      std::fflush(file_);enabled_.store(false);return;
    }
    auto us=std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    std::fprintf(file_,"{\"sequence\":%u,\"time_us\":%llu,\"host_tid\":%ld,",
        ++sequence_,(unsigned long long)us,::syscall(SYS_gettid));
    va_list args;va_start(args,format);std::vfprintf(file_,format,args);va_end(args);
    std::fputs("}\n",file_);
    if(std::fflush(file_)!=0) {
      enabled_.store(false);std::fprintf(stderr,"[acv-bake] Trace write failed: errno=%d\n",errno);
    }
  }
 private:
  std::mutex mutex_;
  std::atomic<bool> enabled_{false};
  FILE* file_=nullptr;
  uint32_t sequence_=0;
};
bool AcvBakeSnapshot(const char* suffix,const uint8_t* data,uint32_t bytes) {
  const char* prefix=std::getenv("ACV_TRACE_BAKE_CHAIN");
  if(!prefix || *prefix!='/')return false;
  const std::string path=std::string(prefix)+suffix;
  const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
  if(fd<0) {
    std::fprintf(stderr,"[acv-bake] Cannot create snapshot: errno=%d\n",errno);return false;
  }
  uint32_t done=0;
  while(done<bytes) {
    const auto n=::write(fd,data+done,bytes-done);
    if(n<0 && errno==EINTR)continue;
    if(n<=0)break;
    done+=uint32_t(n);
  }
  const bool closed=::close(fd)==0;
  if(done!=bytes || !closed)std::fprintf(stderr,"[acv-bake] Incomplete snapshot: %u/%u bytes\n",done,bytes);
  return done==bytes && closed;
}
uint64_t AcvBakeSampleHash(const uint8_t* data,uint32_t bytes) {
  uint64_t hash=14695981039346656037ULL;
  for(uint32_t i=0;i<std::min(bytes,4096u);++i)hash=(hash^data[i])*1099511628211ULL;
  return hash;
}
} // namespace
