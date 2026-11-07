#include "parallel_readback_copy.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

void Check(acv::ParallelReadbackCopy& pool, size_t bytes, unsigned seed) {
  std::vector<uint8_t> source(bytes + 129), destination(bytes + 131, 0xCE);
  for (size_t i = 0; i < source.size(); ++i) source[i] = (i * 97 + seed * 17) & 255;
  pool.Copy(destination.data()+67, source.data()+65, bytes);
  assert(std::equal(source.begin()+65, source.begin()+65+bytes,destination.begin()+67));
  assert(std::all_of(destination.begin(),destination.begin()+67,[](auto v){return v==0xCE;}));
  assert(std::all_of(destination.begin()+67+bytes,destination.end(),[](auto v){return v==0xCE;}));
}
#if defined(__linux__)
// A protected page immediately after the last byte catches vector overreads and
// overwrites that padded std::vector tests cannot see. Page-sized observed copies
// also reproduce the equal low12 address bits seen in live GPU readback.
void CheckProtectedEnd(acv::ParallelReadbackCopy& pool, size_t bytes) {
  const long queried_page = sysconf(_SC_PAGESIZE);
  assert(queried_page > 0);
  const size_t page = static_cast<size_t>(queried_page);
  const size_t rounded = (bytes + page - 1) / page * page;
  const size_t mapped = rounded + 2 * page;
  auto* source = static_cast<uint8_t*>(mmap(nullptr, mapped, PROT_NONE,
      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  auto* destination = static_cast<uint8_t*>(mmap(nullptr, mapped, PROT_NONE,
      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  assert(source != MAP_FAILED && destination != MAP_FAILED);
  assert(mprotect(source + page, rounded, PROT_READ | PROT_WRITE) == 0);
  assert(mprotect(destination + page, rounded, PROT_READ | PROT_WRITE) == 0);
  auto* src = source + page + rounded - bytes;
  auto* dst = destination + page + rounded - bytes;
  for (size_t i = 0; i < bytes; ++i) src[i] = (i * 97 + (i >> 12) * 31) & 255;
  std::memset(destination + page, 0xCE, rounded);
  assert(mprotect(source + page, rounded, PROT_READ) == 0);
  pool.Copy(dst, src, bytes);
  assert(std::memcmp(dst, src, bytes) == 0);
  assert(std::all_of(destination + page, dst, [](auto b) { return b == 0xCE; }));
  assert(munmap(source, mapped) == 0);
  assert(munmap(destination, mapped) == 0);
}
#endif
int main(int argc,char**) {
  if (argc == 1) {
    for(bool forward : {false,true}) for(size_t count : {size_t(1),size_t(2),size_t(4)}) {
      acv::ParallelReadbackCopy pool(count, forward);
      pool.Copy(nullptr,nullptr,0);
      for(size_t n : {size_t(1),size_t(63),size_t(64),size_t(65),size_t(127),size_t(128),size_t(129),size_t(1048575),
                     size_t(1048576),size_t(1048577),size_t(2097169),size_t(8388611)}) Check(pool,n,7);
#if defined(__linux__)
      for (size_t n : {size_t(127), size_t(128), size_t(129), size_t(1048575),
                       size_t(1048576), size_t(1048577), size_t(3768320), size_t(7249920)})
        CheckProtectedEnd(pool, n);
#endif
      // Repeated generations and concurrent producers must not reuse a lease early.
      std::thread a([&]{for(unsigned i=0;i<24;++i)Check(pool,1048576+i*79,i);});
      std::thread b([&]{for(unsigned i=0;i<24;++i)Check(pool,2097152+i*33,i+9);});
      a.join();b.join();
    }
    std::cout << "parallel copy: exact bytes, tails, guards, protected page ends, generations, concurrent callers PASS\n";
    return 0;
  }
  constexpr size_t capacity=128*1024*1024, bytes=4*1024*1024;
  std::vector<uint8_t> source(capacity),destination(capacity);
  for(size_t i=0;i<capacity;++i)source[i]=(i*19+i/4096)&255;
  // Alternating policies, each with 512MiB of identical streaming copies.
  for(unsigned trial=0;trial<3;++trial)for(size_t count :
      (trial%2 ? std::vector<size_t>{4,2,1} : std::vector<size_t>{1,2,4})) {
    acv::ParallelReadbackCopy pool(count);
    for(unsigned i=0;i<32;++i)pool.Copy(destination.data()+i*bytes,source.data()+i*bytes,bytes);
    auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<128;++i) {
      const size_t off=(i%32)*bytes;
      pool.Copy(destination.data()+off,source.data()+off,bytes);
    }
    const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    assert(source==destination);
    std::cout << "{\"threads\":"<<count<<",\"trial\":"<<trial
              <<",\"bytes\":"<<128*bytes<<",\"ms\":"<<ms<<"}\n";
  }
}
