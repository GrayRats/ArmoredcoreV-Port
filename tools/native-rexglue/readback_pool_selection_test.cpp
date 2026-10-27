#include "readback_pool_selection.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
struct Slot{uint64_t id,capacity,submission;bool occupied;};
using Pool=std::vector<std::unique_ptr<Slot>>;
int main(){
 Pool pool;auto add=[&](uint64_t id,uint64_t cap,uint64_t ticket,bool busy){pool.push_back(std::make_unique<Slot>(Slot{id,cap,ticket,busy}));};
 add(1,4,100,true);add(2,16,20,true);add(3,16,30,true);add(4,8,1,true);add(5,32,40,false);add(6,32,0,true);add(0,32,1,true);
 Slot* selected=nullptr;unsigned callbacks=0;
 assert(acv::RetireOldestFittingSlot(pool,16,[&](uint64_t id){++callbacks;assert(id==2);pool[1]->occupied=false;return true;},selected));
 assert(selected==pool[1].get() && callbacks==1 && pool[2]->occupied && pool[5]->occupied);
 assert(acv::RetireOldestFittingSlot(pool,16,[&](uint64_t id){++callbacks;assert(id==3);pool[2]->occupied=false;return true;},selected));
 assert(selected==pool[2].get() && callbacks==2);
 assert(acv::RetireOldestFittingSlot(pool,16,[&](uint64_t){assert(false);return true;},selected) && !selected);
 assert(acv::RetireOldestFittingSlot(pool,64,[&](uint64_t){assert(false);return true;},selected) && !selected);
 selected=pool[0].get();assert(!acv::RetireOldestFittingSlot(pool,0,[](uint64_t){assert(false);return true;},selected) && !selected);
 pool.clear();add(1,16,10,true);add(2,16,10,true);
 assert(!acv::RetireOldestFittingSlot(pool,16,[](uint64_t){return false;},selected) && !selected && pool[0]->occupied && pool[1]->occupied);
 // A successful queue lookup without a known publication is not safe reuse.
 assert(!acv::RetireOldestFittingSlot(pool,16,[](uint64_t){return true;},selected) && !selected && pool[0]->occupied);
 assert(!acv::RetireOldestFittingSlot(pool,16,[](uint64_t)->bool{throw std::runtime_error("retirement failed");},selected) && !selected && pool[0]->occupied);
 assert(acv::RetireOldestFittingSlot(pool,16,[&](uint64_t id){assert(id==1);pool[0]->occupied=false;return true;},selected) && selected==pool[0].get());
 std::cout<<"PASS: oldest fitting known-ticket occupied slot, stable ties, no-fit/growth, unknown IDs/tickets, failure/exception/missing-publication ownership guards\n";
}
