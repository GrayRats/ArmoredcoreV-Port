#pragma once
// GPU-owner-only selection. Never reuse until the queue retires the slot and
// the publication callback clears occupied. A missing job is not completion.
// The retirement callback must not remove or destroy the pool slot.
#include <cstdint>
#include <memory>
#include <vector>
namespace acv {
template<class Slot,class Retire>
bool RetireOldestFittingSlot(const std::vector<std::unique_ptr<Slot>>& slots,
                            uint64_t bytes,Retire&& retire,Slot*& selected) {
  selected=nullptr;
  if(!bytes) return false;
  Slot* oldest=nullptr;
  for(const auto& slot:slots) {
    if(!slot->occupied || !slot->id || !slot->submission || slot->capacity<bytes) continue;
    if(!oldest || slot->submission<oldest->submission) oldest=slot.get();
  }
  if(!oldest) return true; // Caller may need to grow/reclaim different-sized allocations.
  try { if(!retire(oldest->id)) return false; }
  catch(...) { return false; }
  if(oldest->occupied) return false; // Failed/uncommitted copy still owns the allocation.
  selected=oldest;
  return true;
}
} // namespace acv
