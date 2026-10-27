#include <rex/thread.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <thread>
#include <vector>
using namespace std::chrono_literals;
double cpu_ms() { timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec*1000.0+t.tv_nsec/1e6; }
int main() {
 std::vector<std::unique_ptr<rex::thread::Event>> events;
 std::vector<rex::thread::WaitHandle*> handles;
 for(int i=0;i<65;++i){events.push_back(rex::thread::Event::CreateAutoResetEvent(false));handles.push_back(events.back().get());}
 auto start=std::chrono::steady_clock::now();double cpu=cpu_ms();
 auto result=rex::thread::WaitAny(handles.data(),handles.size(),true,120ms);
 double spent=cpu_ms()-cpu;double wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 assert(result.first==rex::thread::WaitResult::kTimeout);assert(wall>=100&&wall<1000);
 std::printf("WaitAny 65 unsignaled handles: wall %.2fms, thread CPU %.2fms (%.1f%%)\n",wall,spent,100*spent/wall);
 // The old fractional-millisecond truncation consumes nearly a full CPU.
 bool efficient=spent<wall*0.2;
 events[64]->Set();result=rex::thread::WaitAny(handles.data(),handles.size(),false,0ms);
 assert(result.first==rex::thread::WaitResult::kSuccess&&result.second==64);
 assert(rex::thread::WaitAny(handles.data(),handles.size(),false,0ms).first==rex::thread::WaitResult::kTimeout);
 auto sem=rex::thread::Semaphore::Create(0,1000);assert(sem);
 rex::thread::WaitHandle* pair[]={sem.get(),events[0].get()};
 std::thread producer([&]{for(int i=0;i<100;++i){std::this_thread::sleep_for(100us);assert(sem->Release(1,nullptr));}});
 for(int i=0;i<100;++i){result=rex::thread::WaitAny(pair,2,false,1000ms);assert(result.first==rex::thread::WaitResult::kSuccess&&result.second==0);}
 producer.join();assert(rex::thread::Wait(sem.get(),false,0ms)==rex::thread::WaitResult::kTimeout);
 events[0]->Set();
 assert(rex::thread::WaitAll(handles.data(),2,false,2ms)==rex::thread::WaitResult::kTimeout);
 events[1]->Set();
 assert(rex::thread::WaitAll(handles.data(),2,false,10ms)==rex::thread::WaitResult::kSuccess);
 assert(rex::thread::WaitAny(handles.data(),2,false,0ms).first==rex::thread::WaitResult::kTimeout);
 std::atomic<bool> waiting{false},callback{false},alerted{false};
 auto worker=rex::thread::Thread::Create({},[&]{
   waiting=true;
   auto r=rex::thread::WaitAny(handles.data(),handles.size(),true,1000ms);
   alerted=(r.first==rex::thread::WaitResult::kUserCallback);
 });
 assert(worker);
 while(!waiting.load())std::this_thread::yield();
 worker->QueueUserCallback([&]{callback=true;});
 assert(rex::thread::Wait(worker.get(),false,2000ms)==rex::thread::WaitResult::kSuccess);
 assert(callback&&alerted);
 std::puts("PASS timeout, indexed auto-reset, counted concurrent semaphore signals, atomic WaitAll and alertable callback");

 return efficient?0:1;
}
