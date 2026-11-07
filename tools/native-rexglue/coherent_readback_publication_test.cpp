#include "coherent_readback_publication.h"
#include "readback_batch_test_support.h"
#include <stdexcept>
int main() {
 const VkMemoryPropertyFlags coherent=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
 for(bool enabled:{false,true}) for(bool known:{false,true}) for(auto flags:{VkMemoryPropertyFlags(0),VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT),VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),coherent}) {
  int invalidates=0,publishes=0;bool waited=false;
  auto invalidate=[&]{assert(waited);++invalidates;return VK_SUCCESS;};
  auto publish=[&]{assert(waited);++publishes;return true;};
  assert(!acv::PublishCompletedReadback(false,enabled,known,flags,invalidate,publish));
  assert(!invalidates && !publishes);waited=true;
  assert(acv::PublishCompletedReadback(true,enabled,known,flags,invalidate,publish));
  const bool skip=enabled && known && flags==coherent;
  assert(invalidates==(skip?0:1) && publishes==1);
 }
 for(bool throws:{false,true}) {
  HostMemory memory;Dispatcher dispatch;bool waited=false;unsigned copies=0;
  Bridge bridge(memory,memory.mutex,[&](auto fn){dispatch.Post(std::move(fn));},[&](uint64_t){waited=true;return true;});
  auto state=bridge.MakeState({0,4},1);
  {auto lock=std::unique_lock<std::recursive_mutex>(memory.mutex);assert(bridge.RegisterLocked(lock,state));}
  auto lease=std::make_shared<int>(1);std::weak_ptr<int> held=lease;
  assert(bridge.Commit(state,1,[&,lease]{return acv::PublishCompletedReadback(waited,true,false,coherent,
    [&]()->VkResult{if(throws)throw std::runtime_error("injected invalidate exception");return VK_ERROR_DEVICE_LOST;},
    [&]{++copies;memory.data[0]=99;return true;});}));lease.reset();
  uint32_t value=0;assert(!memory.Read(0,&value,4));assert(waited && !copies && !value && state->failed && !state->published && !held.expired());
  bridge.Close();assert(bridge.CancelAfterDeviceStopped() && held.expired());
 }
 unsigned calls=0;
 assert(!acv::PublishCompletedReadback(true,true,true,coherent,[]{return VK_SUCCESS;},[&]()->bool{++calls;throw std::runtime_error("publication failed");}));assert(calls==1);
 assert(!acv::PublishCompletedReadback(true,true,true,coherent,[]{return VK_SUCCESS;},[]{return false;}));
 std::cout<<"PASS coherent publication: flag0/1,realflags/unknown/noncoherent,completed proof,failures/exceptions prevent publication and retain leases\n";
}
