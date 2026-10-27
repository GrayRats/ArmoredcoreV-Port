#include "physical_readback_providers.h"
#include <cassert>
#include <iostream>
#include <random>
using Registry=acv::PhysicalReadbackProviders;
struct Item {Registry::Range range;std::shared_ptr<bool> ready;};
int main(){
 std::recursive_mutex mutex;Registry registry(mutex);std::unique_lock<std::recursive_mutex> lock(mutex);
 std::vector<Item> pending;std::mt19937 random(0xAC0560);
 auto overlap=[](Registry::Range a,Registry::Range b){return uint64_t(a.address)<uint64_t(b.address)+b.bytes && uint64_t(b.address)<uint64_t(a.address)+a.bytes;};
 auto sync=[&](Registry::Range range){
  size_t last=0;for(size_t i=0;i<pending.size();++i)if(overlap(pending[i].range,range))last=i+1;
  assert(registry.Synchronize(lock,range));pending.erase(pending.begin(),pending.begin()+last);
 };
 auto collect=[&]{assert(registry.CollectPublished(lock));while(!pending.empty() && *pending.front().ready)pending.erase(pending.begin());};
 auto check=[&](Registry::Range range,bool pinned=false,Registry::Range pin={0,0}){
  bool expected=true;for(const auto& item:pending)if(overlap(item.range,range))expected=false;
  if(pinned && overlap(pin,range))expected=false;
  assert(registry.CanAccessWithoutWait(lock,range)==expected);
 };
 for(unsigned step=0;step<4000;++step){
  Registry::Range range{uint32_t((random()%1024)*4),uint32_t((random()%16+1)*4)};
  switch(random()%4){
   case 0:{
    if(pending.size()==64)sync({0,0x20000000});
    if(step%7==0)range={0x1FFFFFFC,4};
    auto ready=std::make_shared<bool>(false);
    assert(registry.RegisterLocked(lock,{range,[]{return true;},[ready]{*ready=true;return true;},[ready]{return *ready;}}));
    pending.push_back({range,ready});break;
   }
   case 1:sync(range);break;
   case 2:if(!pending.empty())*pending[random()%pending.size()].ready=true;collect();break;
   case 3:{
    sync(range);auto token=registry.BeginCpuAccess(lock,range);assert(token);
    for(unsigned i=0;i<8;++i)check({uint32_t((random()%1024)*4),4},true,range);
    assert(!registry.CanAccessWithoutWait(lock,range));assert(registry.EndCpuAccess(lock,token));break;
   }
  }
  for(unsigned i=0;i<16;++i)check({uint32_t((random()%1024)*4),uint32_t((random()%16+1)*4)});
  check({0x1FFFFFFC,4});check({0x10000000,4});assert(registry.pending()==pending.size());
 }
 sync({0,0x20000000});assert(pending.empty());check({0,4});check({0x1FFFFFFC,4});
 assert(registry.CancelAfterDeviceStopped());assert(!registry.CanAccessWithoutWait(lock,{0,4}));
 std::cout<<"PASS provider range bounds vs independent interval model:4000 register/collect/prefix-sync/pin operations, >64000 range queries and512MiB boundary\n";
}
