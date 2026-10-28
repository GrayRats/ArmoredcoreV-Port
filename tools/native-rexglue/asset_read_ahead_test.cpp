#include "asset_read_ahead.h"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <span>
#include <vector>
static int g_window_mb=1;
#define REXCVAR_GET(name) g_window_mb
#define REXLOG_WARN(...) ((void)0)
#define REXLOG_INFO(...) ((void)0)
class FileHandle {
 public:
  explicit FileHandle(std::filesystem::path){}
  virtual ~FileHandle()=default;
  virtual bool Read(size_t,void*,size_t,size_t*)=0;
  virtual bool Write(size_t,const void*,size_t,size_t*)=0;
  virtual bool SetLength(size_t)=0;
  virtual void Flush()=0;
};
#include "posix_filehandle_actual.inc"
int main(int argc,char** argv) {
  assert(argc==2);
  const auto root=std::filesystem::path(argv[1]);
  auto make=[&](const char* name) {
    auto path=root/name;int w=::open(path.c_str(),O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600);assert(w>=0);
    std::vector<uint8_t> data(8<<20);for(size_t i=0;i<data.size();++i)data[i]=uint8_t(i*31+i/4096);
    assert(::write(w,data.data(),data.size())==ssize_t(data.size()));::close(w);
    return ::open(path.c_str(),O_RDONLY|O_CLOEXEC);
  };
  int a=make("a.bdt"),b=make("b.bdt"),c=make("c.bdt");
  assert(a>=0&&b>=0&&c>=0);
  auto first=acv::ReadAheadFile::Open(a),second=acv::ReadAheadFile::Open(b),third=acv::ReadAheadFile::Open(c);
  const int pinned=first->fd;struct stat original{};assert(::fstat(pinned,&original)==0);
  std::mutex gate;std::condition_variable changed;bool entered=false,released=false;unsigned advice=0;
  {
    acv::AssetReadAhead service(1,[&](int fd,uint64_t start,uint64_t bytes) {
      assert((::fcntl(fd,F_GETFD)&FD_CLOEXEC)!=0 && bytes>0 && bytes<=2u<<20);
      if(advice++==0) {
        std::unique_lock lock(gate);entered=true;changed.notify_one();
        changed.wait(lock,[&]{return released;});
        struct stat now{};assert(::fstat(fd,&now)==0 && now.st_ino==original.st_ino);
      } else assert(start==3u<<20); // Queued same-file work coalesced to latest hint.
      return 0;
    });
    assert(service.Request(first,0,2u<<20));
    {std::unique_lock lock(gate);changed.wait(lock,[&]{return entered;});}
    assert(!service.Request(first,0,2u<<20));
    assert(service.Request(second,0,2u<<20));
    assert(service.Request(second,3u<<20,2u<<20));
    assert(!service.Request(third,0,2u<<20));
    assert(!service.Request(third,third->size,2u<<20));
    std::weak_ptr<acv::ReadAheadFile> lease=first;first.reset();::close(a);
    assert(!lease.expired()); // Worker owns the duplicated inode after close.
    {std::lock_guard lock(gate);released=true;}changed.notify_one();service.WaitIdle();
    assert(lease.expired() && ::fcntl(pinned,F_GETFD)==-1 && errno==EBADF);
    auto stats=service.statistics();
    assert(stats.submitted==2 && stats.coalesced==1 && stats.dropped==1 &&
           stats.completed==2 && stats.failed==0 && stats.advised_bytes==(4u<<20));
  }
  {
    acv::AssetReadAhead service(1,[](int,uint64_t,uint64_t){return EINVAL;});
    assert(service.Request(third,third->size-17,1u<<20));service.WaitIdle();
    assert(service.statistics().failed==1); // Advice failure is independent of real IO.
  }
  ::close(b);::close(c);second.reset();third.reset();
  // Actual SDK class: exact random/partial/EOF reads, real Linux page-cache advice.
  {
    int fd=::open((root/"a.bdt").c_str(),O_RDONLY|O_CLOEXEC);assert(fd>=0);
    PosixFileHandle file(root/"a.bdt",fd);
    std::vector<uint8_t> bytes(100003);size_t count=0;
    assert(file.Read(12345,bytes.data(),bytes.size(),&count)&&count==bytes.size());
    for(size_t i=0;i<count;++i)assert(bytes[i]==uint8_t((12345+i)*31+(12345+i)/4096));
    assert(file.Read((8u<<20)-7,bytes.data(),100,&count)&&count==7);
    assert(file.Read(8u<<20,bytes.data(),100,&count)&&count==0);
  }
  acv::AssetReadAhead::Global().WaitIdle();
  // Writable archives cannot enter read-only prefetch; actual writes stay coherent.
  {
    int fd=::open((root/"a.bdt").c_str(),O_RDWR|O_CLOEXEC);assert(fd>=0);
    assert(!acv::ReadAheadFile::Open(fd));
    PosixFileHandle file(root/"a.bdt",fd);uint32_t value=0xABCDEF12,got=0;size_t count=0;
    assert(file.Write(44,&value,sizeof(value),&count)&&count==4);
    assert(file.Read(44,&got,sizeof(got),&count)&&got==value);
  }
  std::cout<<"PASS bounded read-ahead: queue/drop/coalesce, close-during-IO inode lease, CLOEXEC, EOF, advice failure, actual SDK random reads and writable bypass\n";
}
