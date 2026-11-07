#include "submission_fence_wait.h"
#include <cassert>
#include <iostream>
#ifdef NDEBUG
#error Assertions are required
#endif
int main() {
  unsigned cases=0;
  for (bool status : {false,true}) for (VkResult blocking : {VK_SUCCESS,VK_TIMEOUT,VK_ERROR_OUT_OF_HOST_MEMORY,VK_ERROR_DEVICE_LOST})
  for (unsigned required : {0u,2u}) for (unsigned successes : {0u,1u,2u})
  for (VkResult stop : {VK_NOT_READY,VK_ERROR_OUT_OF_HOST_MEMORY,VK_ERROR_UNKNOWN,VK_ERROR_DEVICE_LOST}) {
    std::vector<uint64_t> pending{1,2,3,4,5},free{99};uint64_t completed=100;
    unsigned polls=0,blocks=0;
    const unsigned ready=required && blocking==VK_SUCCESS?required:0;
    const VkResult native_stop=stop==VK_NOT_READY && !status?VK_TIMEOUT:stop;
    auto wait=[&](uint32_t count,const uint64_t* handles,uint64_t timeout) {
      assert(timeout==UINT64_MAX && count==required && count && ++blocks==1);
      for(unsigned i=0;i<count;++i)assert(handles[i]==i+1);
      return blocking;
    };
    auto poll=[&](uint64_t handle) {
      assert(handle==ready+polls+1);return polls++<successes?VK_SUCCESS:native_stop;
    };
    auto result=acv::CompleteSubmissionFences(pending,free,completed,required?100+required:0,false,wait,poll);
    const bool lost=(required && blocking==VK_ERROR_DEVICE_LOST)||stop==VK_ERROR_DEVICE_LOST;
    const unsigned retired=lost?0:ready+successes;
    assert(result.valid_range && result.device_lost==lost && result.retired==retired);
    assert(result.poll_result==native_stop && result.poll_calls==successes+1 && polls==successes+1);
    assert(blocks==(required?1u:0u) && result.blocking_fences==required);
    assert(completed==100+retired && pending.size()==5-retired && free.size()==1+retired);
    assert(free[0]==99);
    for(unsigned i=0;i<retired;++i)assert(free[1+i]==1+i);
    for(unsigned i=0;i<pending.size();++i)assert(pending[i]==retired+1+i);
    ++cases;
  }
  // Actual caller guard shared with SDK: failed EndSubmission may already notify
  // loss. Neither callback, completion nor free-list retirement may run then.
  for(bool initial_loss:{false,true}) {
    bool lost=initial_loss;uint64_t await=103,completed=100;unsigned ends=0,notifies=0,calls=0;
    std::vector<uint64_t> pending{1,2},free{99};
    bool prepared=acv::PrepareSubmissionFenceWait(await,lost,true,[&]{return 103;},[&]{++ends;lost=true;++notifies;return false;});
    if(prepared)acv::CompleteSubmissionFences(pending,free,completed,await,false,
      [&](uint32_t,const uint64_t*,uint64_t){++calls;return VK_SUCCESS;},
      [&](uint64_t){++calls;return VK_SUCCESS;});
    assert(!prepared && !calls && ends==!initial_loss && notifies==!initial_loss);
    assert(completed==100 && pending==std::vector<uint64_t>({1,2}) && free==std::vector<uint64_t>{99});
  }
  std::cout<<"PASS "<<cases<<" injected poll cases: native NOT_READY/TIMEOUT, exact prefix/free-list, OOM/fatal/device loss before/after success, blocking failure, shared EndSubmission guard\n";
}
