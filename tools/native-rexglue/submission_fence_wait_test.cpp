#include "submission_fence_wait.h"
#include <cassert>
#include <iostream>
#ifdef NDEBUG
#error This test requires assertions.
#endif
struct Case {
  uint64_t required;
  VkResult blocking;
  std::vector<VkResult> polls;
  size_t retired;
  bool lost;
};
int main() {
  // Explicit expectations cover the existing failure behavior as well as the
  // new selected-fence index. The SAME function moves SDK production handles.
  const std::vector<Case> cases={
    {3,VK_SUCCESS,{VK_SUCCESS,VK_TIMEOUT},4,false},
    {0,VK_SUCCESS,{VK_SUCCESS,VK_SUCCESS,VK_TIMEOUT},2,false},
    {3,VK_TIMEOUT,{VK_SUCCESS,VK_TIMEOUT},1,false},
    {3,VK_ERROR_OUT_OF_HOST_MEMORY,{VK_SUCCESS,VK_SUCCESS,VK_TIMEOUT},2,false},
    {3,VK_ERROR_DEVICE_LOST,{VK_SUCCESS,VK_TIMEOUT},0,true},
    {3,VK_SUCCESS,{VK_ERROR_DEVICE_LOST},0,true},
    {3,VK_SUCCESS,{VK_ERROR_OUT_OF_HOST_MEMORY},3,false},
    {0,VK_SUCCESS,{VK_ERROR_OUT_OF_HOST_MEMORY},0,false},
    {5,VK_SUCCESS,{},5,false},
    {1,VK_SUCCESS,{VK_TIMEOUT},1,false},
    {0,VK_SUCCESS,{VK_SUCCESS,VK_SUCCESS,VK_SUCCESS,VK_SUCCESS,VK_SUCCESS},5,false},
  };
  for (bool latest:{false,true}) for(const auto& test:cases) {
    std::vector<uint64_t> pending{1,2,3,4,5},free{99};uint64_t completed=100;
    size_t blocking_calls=0,poll_calls=0;
    const auto result=acv::CompleteSubmissionFences(pending,free,completed,
        test.required?100+test.required:0,latest,
        [&](uint32_t count,const uint64_t* fences,uint64_t timeout) {
          if(timeout) {
            assert(timeout==UINT64_MAX && test.required && blocking_calls++==0);
            assert(count==(latest?1:test.required));
            for(unsigned i=0;i<count;++i)assert(fences[i]==(latest?test.required:1+i));
            return test.blocking;
          }
          assert(count==1 && poll_calls<test.polls.size());
          const uint64_t first=test.required && test.blocking==VK_SUCCESS ? test.required+1 : 1;
          assert(fences[0]==first+poll_calls);
          return test.polls[poll_calls++];
        });
    assert(result.valid_range && result.retired==test.retired && result.device_lost==test.lost);
    assert(completed==100+test.retired && free.size()==1+test.retired && pending.size()==5-test.retired);
    assert(free[0]==99);
    for(size_t i=0;i<test.retired;++i)assert(free[i+1]==i+1);
    for(size_t i=0;i<pending.size();++i)assert(pending[i]==test.retired+1+i);
    assert(result.requested_prefix==test.required && blocking_calls==(test.required?1:0));
    assert(result.blocking_fences==(test.required?(latest?1:test.required):0));
    assert(poll_calls==test.polls.size() && result.poll_calls==poll_calls);
    assert(result.blocking_result==(test.required?test.blocking:VK_SUCCESS));
    assert(result.poll_result==(test.polls.empty()?VK_SUCCESS:test.polls.back()));
  }
  for(bool latest:{false,true}) {
    std::vector<uint64_t> pending{1,2},free{99};uint64_t completed=100;
    const auto invalid=acv::CompleteSubmissionFences(pending,free,completed,103,latest,
      [](uint32_t,const uint64_t*,uint64_t)->VkResult {assert(false);return VK_ERROR_UNKNOWN;});
    assert(!invalid.valid_range && completed==100 && pending==std::vector<uint64_t>({1,2}) && free==std::vector<uint64_t>{99});
    auto wait=[](uint32_t,const uint64_t*,uint64_t){return VK_SUCCESS;};
    assert(acv::CompleteSubmissionFences(pending,free,completed,102,latest,wait).retired==2);
    const auto empty=acv::CompleteSubmissionFences(pending,free,completed,102,latest,
      [](uint32_t,const uint64_t*,uint64_t)->VkResult {assert(false);return VK_ERROR_UNKNOWN;});
    assert(empty.valid_range && !empty.retired && completed==102 && free==std::vector<uint64_t>({99,1,2}));
  }
  for(bool initially_lost:{false,true}) for(bool latest:{false,true}) {
    std::vector<uint64_t> pending{1,2},free{99};uint64_t completed=100,await=103;
    bool lost=initially_lost;unsigned end_calls=0,notifications=0,wait_calls=0;
    const bool prepared=acv::PrepareSubmissionFenceWait(await,lost,true,
        [&]{return completed+pending.size()+1;},[&]{++end_calls;lost=true;++notifications;return false;});
    if(prepared)acv::CompleteSubmissionFences(pending,free,completed,await,latest,
        [&](uint32_t,const uint64_t*,uint64_t){++wait_calls;return VK_SUCCESS;});
    assert(!prepared && !wait_calls && end_calls==(initially_lost?0:1));
    assert(notifications==(initially_lost?0:1) && completed==100);
    assert(pending==std::vector<uint64_t>({1,2}) && free==std::vector<uint64_t>{99});
  }
  for(bool submit_success:{false,true}) {
    std::vector<uint64_t> pending{1,2},free;uint64_t completed=100,await=103;bool lost=false,open=true;
    assert(acv::PrepareSubmissionFenceWait(await,lost,open,[&]{return completed+pending.size()+1;},[&]{
      if(submit_success){pending.push_back(3);open=false;}return submit_success;
    }));
    assert(await==(submit_success?103:102) && open!=submit_success);
    const auto result=acv::CompleteSubmissionFences(pending,free,completed,await,true,
        [](uint32_t,const uint64_t*,uint64_t){return VK_SUCCESS;});
    assert(result.retired==(submit_success?3:2) && completed==await && pending.empty());
  }
  std::cout<<"PASS shared fence completion: both policies, exact prefix/free-list order, later polls, timeout/host failure/device loss, invalid/empty prefix, API counters, EndSubmission loss/no duplicate notify and failed-submit clamp\n";
}
