#pragma once
#include <cstdint>
namespace acv {
// Shared production/test boundary: all early exits fail pending recorded jobs.
// Successful GPU submission must explicitly propagate append failure to caller.
template<class Bridge> class ReadbackSubmissionScope {
 public:
  ReadbackSubmissionScope(Bridge* bridge,uint64_t ticket):bridge_(bridge),ticket_(ticket) {}
  ~ReadbackSubmissionScope() {if(bridge_ && !settled_) bridge_->SubmissionFailed(ticket_);}
  bool Submitted() {
    if(settled_) return false;
    settled_=true;
    return !bridge_ || bridge_->SubmissionSucceeded(ticket_);
  }
 private:
  Bridge* bridge_;uint64_t ticket_;bool settled_=false;
};
} // namespace acv
