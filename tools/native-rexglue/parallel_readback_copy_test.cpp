#include "parallel_readback_copy.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

void Check(acv::ParallelReadbackCopy& pool, size_t bytes, unsigned seed) {
  std::vector<uint8_t> source(bytes + 129), destination(bytes + 131, 0xCE);
  for (size_t i = 0; i < source.size(); ++i) source[i] = (i * 97 + seed * 17) & 255;
  pool.Copy(destination.data()+67, source.data()+65, bytes);
  assert(std::equal(source.begin()+65, source.begin()+65+bytes,destination.begin()+67));
  assert(std::all_of(destination.begin(),destination.begin()+67,[](auto v){return v==0xCE;}));
  assert(std::all_of(destination.begin()+67+bytes,destination.end(),[](auto v){return v==0xCE;}));
}
int main(int argc,char**) {
  if (argc == 1) {
    for(size_t count : {size_t(1),size_t(2),size_t(4)}) {
      acv::ParallelReadbackCopy pool(count);
      pool.Copy(nullptr,nullptr,0);
      for(size_t n : {size_t(1),size_t(63),size_t(64),size_t(65),size_t(1048575),
                     size_t(1048576),size_t(1048577),size_t(2097169),size_t(8388611)}) Check(pool,n,7);
      // Repeated generations and concurrent producers must not reuse a lease early.
      std::thread a([&]{for(unsigned i=0;i<24;++i)Check(pool,1048576+i*79,i);});
      std::thread b([&]{for(unsigned i=0;i<24;++i)Check(pool,2097152+i*33,i+9);});
      a.join();b.join();
    }
    std::cout << "parallel copy: exact bytes, tails, guards, generations, concurrent callers PASS\n";
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
