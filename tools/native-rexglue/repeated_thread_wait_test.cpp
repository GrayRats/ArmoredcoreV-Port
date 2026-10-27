#include <rex/thread.h>
#include <atomic>
#include <cstdio>
#include <dlfcn.h>
#include <pthread.h>
#include <cerrno>
static std::atomic<int> joins{0};
extern "C" int pthread_join(pthread_t t, void** result) {
  // Prevent undefined repeated pthread_join in the baseline while observing SDK calls.
  static pthread_t target = t;
  if (pthread_equal(t, target) && ++joins > 1) return EINVAL;
  auto real = reinterpret_cast<int(*)(pthread_t,void**)>(dlsym(RTLD_NEXT,"pthread_join"));
  return real(t,result);
}
int main() {
  auto thread=rex::thread::Thread::Create({}, [] {});
  if (!thread) return 2;
  for (int i=0;i<3;++i)
    if (rex::thread::Wait(thread.get(),false,std::chrono::milliseconds(2000)) != rex::thread::WaitResult::kSuccess) return 3;
  std::printf("Repeated successful waits: 3; pthread_join calls: %d (expected 1)\n",joins.load());
  return joins==1 ? 0 : 1;
}
