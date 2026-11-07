#pragma once
// GPU-owner-only selection. Never reuse until the queue retires the slot and
// the publication callback clears occupied. A missing job is not completion.
// The retirement callback must not remove or destroy the pool slot.
#include <cstdint>
#include <memory>
#include <type_traits>
#include <algorithm>
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

// Allocation pressure can coexist with idle small slots that are unusable for
// the new payload. Plan using the exact VkMemoryRequirements::size, not payload
// length. Never partially trim when the complete plan cannot satisfy both caps.
enum class IdleReclaimResult { kNotPossible, kReclaimed, kFailed };
template<class Slot, class Retire, class Release>
IdleReclaimResult ReclaimIdleUndersizedSlots(
    std::vector<std::unique_ptr<Slot>>& slots, uint64_t& allocated,
    uint64_t payload_bytes, uint64_t allocation_bytes, size_t max_slots,
    uint64_t budget, Retire&& retire, Release&& release) {
  static_assert(std::is_nothrow_invocable_v<Release, Slot&>);
  if (!payload_bytes || allocation_bytes < payload_bytes || !max_slots ||
      allocation_bytes > budget || allocated > budget)
    return IdleReclaimResult::kNotPossible;
  const auto fits = [&](uint64_t freed, size_t count) {
    return slots.size() - count < max_slots &&
           allocated - freed <= budget - allocation_bytes;
  };
  if (fits(0, 0)) return IdleReclaimResult::kNotPossible; // No pressure to repair.
  std::vector<Slot*> reclaim;
  try { reclaim.reserve(slots.size()); }
  catch (...) { return IdleReclaimResult::kFailed; }
  uint64_t freed = 0;
  for (const auto& slot : slots) {
    if (slot->occupied || !slot->id || slot->capacity >= payload_bytes) continue;
    if (!slot->allocation || slot->allocation > allocated - freed)
      return IdleReclaimResult::kFailed; // Inconsistent accounting, never wrap.
    reclaim.push_back(slot.get()); freed += slot->allocation;
    if (fits(freed, reclaim.size())) break;
  }
  if (!fits(freed, reclaim.size())) return IdleReclaimResult::kNotPossible;
  // Publication callbacks hold raw Slot pointers. Retire/collect EVERY selected
  // ID before freeing ANY slot, even though occupied=false means published.
  // Neither callback may add, remove or replace slots. Only release destroys Vk
  // resources; owner-thread slot removal happens here after the queue is safe.
  try {
    for (auto* slot : reclaim) if (!retire(slot->id)) return IdleReclaimResult::kFailed;
  } catch (...) { return IdleReclaimResult::kFailed; }
  for (auto* slot : reclaim) if (slot->occupied) return IdleReclaimResult::kFailed;
  for (auto* slot : reclaim) release(*slot);
  slots.erase(std::remove_if(slots.begin(), slots.end(), [&](const auto& slot) {
    return std::find(reclaim.begin(), reclaim.end(), slot.get()) != reclaim.end();
  }), slots.end());
  allocated -= freed;
  return IdleReclaimResult::kReclaimed;
}
} // namespace acv
