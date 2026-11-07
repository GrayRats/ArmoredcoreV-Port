// Standalone, bounded GPU-written staging -> actual production copy pool test.
// Build: clang++ -std=c++23 -O3 -march=x86-64-v2 -pthread FILE -lvulkan -o OUT
// Run under the project logger and timeout(1). JSON lines contain copy-only times.
#include "parallel_readback_copy.h"
#include <vulkan/vulkan.h>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "vulkan_readback_test_support.h"
using namespace acv::readback_test;
namespace {
double Median(std::vector<double> values) { std::sort(values.begin(),values.end()); return values[values.size()/2]; }
} // namespace
int main() try {
  const auto deadline=Clock::now()+std::chrono::seconds(90);
  constexpr size_t capacity=64*1024*1024, max_bytes=7249920;
  Vulkan vk; vk.Init(); vk.Allocate(0,max_bytes,false); vk.Allocate(1,capacity,true);
  std::vector<uint32_t> expected(max_bytes/4);
  for(size_t i=0;i<expected.size();++i) expected[i]=uint32_t(i)*2654435761u ^ uint32_t(i>>11)*2246822519u;
  vk.Begin();
  for(size_t offset=0;offset<max_bytes;offset+=65536)
    vkCmdUpdateBuffer(vk.command,vk.buffers[0].handle,offset,std::min(size_t(65536),max_bytes-offset),
                      reinterpret_cast<const uint8_t*>(expected.data())+offset);
  vk.Barrier(vk.buffers[0].handle,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
  vk.SubmitWait();
  std::unique_ptr<uint8_t, decltype(&std::free)> destination(static_cast<uint8_t*>(std::aligned_alloc(4096,capacity)),&std::free);
  Require(bool(destination),"host allocation failed"); std::memset(destination.get(),0xCE,capacity);
  acv::ParallelReadbackCopy libc(2,false),forward(2,true);
  for(size_t bytes:{size_t(3768320),size_t(7249920),size_t(4194304)}) {
    std::memset(destination.get(),0xCE,capacity); // Reset guards when slot geometry changes.
    const size_t stride=(bytes+8191)/4096*4096+4096, slots=capacity/stride;
    std::array<std::vector<double>,2> samples;
    for(unsigned trial=0;trial<6;++trial) {
      std::array<std::vector<double>,2> trial_samples;
      for(unsigned iteration=0;iteration<18;++iteration) {
        for(unsigned order=0;order<2;++order) {
          Require(Clock::now()<deadline,"90s benchmark deadline exceeded");
          const unsigned policy=(order+trial+iteration)%2;
          const size_t offset=(iteration%slots)*stride+4096;
          std::memset(destination.get()+offset,0xCE,bytes);
          vk.Stage(offset,bytes); // Fresh device write before EACH policy, same source/range.
          const auto start=Clock::now();
          (policy?forward:libc).Copy(destination.get()+offset,static_cast<uint8_t*>(vk.buffers[1].mapping)+offset,bytes);
          const double us=std::chrono::duration<double,std::micro>(Clock::now()-start).count();
          Require(std::memcmp(destination.get()+offset,expected.data(),bytes)==0,"copy byte mismatch");
          Require(std::all_of(destination.get()+offset-4096,destination.get()+offset,[](uint8_t b){return b==0xCE;}),"prefix guard changed");
          Require(std::all_of(destination.get()+offset+bytes,destination.get()+offset+bytes+4096,[](uint8_t b){return b==0xCE;}),"suffix guard changed");
          if(iteration>=2) { samples[policy].push_back(us); trial_samples[policy].push_back(us); }
        }
      }
      std::cout<<"{\"bytes\":"<<bytes<<",\"threads\":2,\"trial\":"<<trial
        <<",\"libc_median_us\":"<<Median(trial_samples[0])<<",\"forward128_median_us\":"<<Median(trial_samples[1])<<"}\n";
    }
    std::cout<<"{\"summary\":true,\"bytes\":"<<bytes<<",\"samples_per_policy\":"<<samples[0].size()
      <<",\"libc_median_us\":"<<Median(samples[0])<<",\"forward128_median_us\":"<<Median(samples[1])
      <<",\"forward_speedup\":"<<Median(samples[0])/Median(samples[1])<<",\"bytes_guards_ok\":true}\n";
  }
  std::cout<<"{\"status\":\"PASS\",\"scope\":\"GPU-written staging, exact bytes and guards; copy latency only, no FPS claim\"}\n";
  return 0;
} catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
