#include "readback_pool_selection.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
#ifdef NDEBUG
#error This test requires enabled assertions.
#endif
struct Slot { uint64_t id, capacity, allocation, submission; bool occupied; };
using Pool=std::vector<std::unique_ptr<Slot>>;
using Result=acv::IdleReclaimResult;
int main() {
  Pool pool;
  uint64_t allocated=0;
  auto add=[&](uint64_t id,uint64_t cap,uint64_t allocation,bool occupied) {
    pool.push_back(std::make_unique<Slot>(Slot{id,cap,allocation,id,occupied})); allocated+=allocation;
  };
  std::vector<uint64_t> retired,released;
  auto retire=[&](uint64_t id){retired.push_back(id);return true;};
  auto release=[&](Slot& slot) noexcept {released.push_back(slot.id);};
  // Occupied fitting bytes remain leased. One idle small allocation supplies
  // sufficient budget for a new larger buffer without awaiting that fitting job.
  add(1,48,48,true);add(2,8,8,false);add(3,8,8,false);
  assert(acv::ReclaimIdleUndersizedSlots(pool,allocated,12,12,64,64,retire,release)==Result::kReclaimed);
  assert(allocated==48 && pool.size()==1 && pool[0]->occupied);
  assert(retired==std::vector<uint64_t>({2,3}) && released==retired);
  // Exact allocation rounding prevents a payload-only trim from admitting a
  // larger actual allocation. An infeasible plan must not call or free anything.
  pool.clear();allocated=0;retired.clear();released.clear();
  add(1,52,52,true);add(2,12,12,false);
  assert(acv::ReclaimIdleUndersizedSlots(pool,allocated,13,16,64,64,retire,release)==Result::kNotPossible);
  assert(allocated==64 && pool.size()==2 && retired.empty() && released.empty());
  // Count pressure alone, even when plenty of byte budget remains.
  pool.clear();allocated=0; add(1,8,8,false);add(2,8,8,false);add(3,16,16,true);
  assert(acv::ReclaimIdleUndersizedSlots(pool,allocated,12,12,3,128,retire,release)==Result::kReclaimed);
  assert(allocated==24 && pool.size()==2 && pool.back()->occupied);
  // Existing free fitting capacity, occupied small slots, and unknown IDs are
  // never reclaimed. The caller can retain the original retirement fallback.
  pool.clear();allocated=0;retired.clear();released.clear();
  add(1,16,16,false);add(2,8,8,true);add(0,8,8,false);
  assert(acv::ReclaimIdleUndersizedSlots(pool,allocated,12,12,3,32,retire,release)==Result::kNotPossible);
  assert(retired.empty() && released.empty() && allocated==32);
  // Queue rejection/exception must retain ALL resource ownership and accounting.
  for (bool throwing:{false,true}) {
    pool.clear();allocated=0;retired.clear();released.clear();add(1,8,8,false);add(2,8,8,false);
    auto reject=[&](uint64_t id){retired.push_back(id);if(id==2){if(throwing)throw std::runtime_error("retire");return false;}return true;};
    assert(acv::ReclaimIdleUndersizedSlots(pool,allocated,12,12,3,16,reject,release)==Result::kFailed);
    assert(allocated==16 && pool.size()==2 && released.empty());
  }
  // A stale occupied indication becoming busy during retirement is rejected.
  pool.clear();allocated=0;add(1,8,8,false);released.clear();
  assert(acv::ReclaimIdleUndersizedSlots(pool,allocated,12,12,1,16,[&](uint64_t){pool[0]->occupied=true;return true;},release)==Result::kFailed);
  assert(allocated==8 && released.empty());
  for(uint64_t requirement:{uint64_t(0),uint64_t(8),uint64_t(17)})
    assert(acv::ReclaimIdleUndersizedSlots(pool,allocated,12,requirement,1,16,retire,release)==Result::kNotPossible);
  std::cout<<"PASS idle undersized reclamation: exact allocation rounding, byte/count caps, occupied leases, no partial infeasible trim, retire failure/exception, accounting\n";
}
