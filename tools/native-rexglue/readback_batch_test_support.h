#pragma once
#include "general_readback_bridge.h"
#include "physical_readback_providers.h"
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <iostream>
#include <thread>
struct HostMemory {
  using PhysicalReadbackProvider=acv::PhysicalReadbackProviders::Provider;
  std::recursive_mutex mutex;
  acv::PhysicalReadbackProviders providers{mutex};
  uint32_t data[16]={};
  bool RegisterExplicitPhysicalReadbackLocked(std::unique_lock<std::recursive_mutex>& lock,
                                             PhysicalReadbackProvider provider) {
    return providers.RegisterLocked(lock,std::move(provider));
  }
  bool CollectPublishedPhysicalReadbacks() {
    auto lock=std::unique_lock<std::recursive_mutex>(mutex);return providers.CollectPublished(lock);
  }
  bool Read(uint32_t offset,void* destination,uint32_t bytes) {
    assert(offset+bytes<=sizeof(data));auto lock=std::unique_lock<std::recursive_mutex>(mutex);
    if(!providers.Synchronize(lock,{offset,bytes})) return false;
    std::memcpy(destination,reinterpret_cast<uint8_t*>(data)+offset,bytes);return true;
  }
};
struct Dispatcher {
  std::thread::id owner=std::this_thread::get_id();
  std::mutex mutex;std::condition_variable cv;
  std::deque<std::function<void()>> work;
  void Post(std::function<void()> fn) {
    if(std::this_thread::get_id()==owner) {fn();return;}
    {std::lock_guard lock(mutex);work.push_back(std::move(fn));}cv.notify_all();
  }
  void Wait() {std::unique_lock lock(mutex);cv.wait(lock,[&]{return !work.empty();});}
  void Pump() {
    for(;;) {std::function<void()> fn;
      {std::lock_guard lock(mutex);if(work.empty()) return;fn=std::move(work.front());work.pop_front();}
      fn();
    }
  }
};
using Bridge=acv::GeneralReadbackBridge<HostMemory>;
