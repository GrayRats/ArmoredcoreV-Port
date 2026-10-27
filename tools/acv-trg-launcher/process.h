#pragma once
#include <spawn.h>
#include <sys/wait.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
extern char** environ;
namespace acv {
struct Result { int code=-1; std::string out,err; };
struct Fd {
  int value=-1;
  explicit Fd(int v=-1):value(v){}
  ~Fd(){Reset();}
  Fd(const Fd&)=delete; Fd& operator=(const Fd&)=delete;
  void Reset(){if(value>=0){close(value);value=-1;}}
};
inline std::vector<char*> Arguments(const std::vector<std::string>& args) {
  std::vector<char*> out;
  for(const auto& s:args)out.push_back(const_cast<char*>(s.c_str()));
  out.push_back(nullptr);return out;
}
inline void Check(int code){if(code)throw std::runtime_error(strerror(code));}
struct Spawn {
  posix_spawn_file_actions_t actions;
  posix_spawnattr_t attr;
  Spawn(){
    Check(posix_spawn_file_actions_init(&actions));
    int rc=posix_spawnattr_init(&attr);
    if(rc){posix_spawn_file_actions_destroy(&actions);Check(rc);}
    // Each owned command gets a separate group. Restore SIGPIPE for children.
    sigset_t defaults;sigemptyset(&defaults);sigaddset(&defaults,SIGPIPE);
    posix_spawnattr_setsigdefault(&attr,&defaults);
    posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP|POSIX_SPAWN_SETSIGDEF);
    posix_spawnattr_setpgroup(&attr,0);
  }
  ~Spawn(){posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attr);}
  void Dup(int from,int to){Check(posix_spawn_file_actions_adddup2(&actions,from,to));}
  pid_t Start(const std::vector<std::string>& args){
    if(args.empty()||args[0].empty())throw std::runtime_error("Empty command");
    auto argv=Arguments(args);pid_t pid=0;
    Check(posix_spawnp(&pid,argv[0],&actions,&attr,argv.data(),environ));return pid;
  }
};
inline void Pipe(Fd& a,Fd& b){int f[2];if(pipe2(f,O_CLOEXEC))Check(errno);a.value=f[0];b.value=f[1];}
inline void Nonblocking(int fd){int flags=fcntl(fd,F_GETFL);if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)Check(errno);}
inline int ExitCode(int status){return WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);}
// Run only on a worker. Concurrently drain both pipes and feed stdin; no shell.
// The host ignores SIGPIPE; children restore its normal disposition.
inline Result Run(const std::vector<std::string>& args,const std::string& input="",int timeout_ms=60000,std::function<bool()> cancelled={}) {
  Fd out,ow,err,ew,ir,in;Pipe(out,ow);Pipe(err,ew);Pipe(ir,in);
  Nonblocking(out.value);Nonblocking(err.value);Nonblocking(in.value);
  Spawn spawn;spawn.Dup(ir.value,0);spawn.Dup(ow.value,1);spawn.Dup(ew.value,2);
  pid_t pid=spawn.Start(args);ir.Reset();ow.Reset();ew.Reset();
  Result result;size_t sent=0;int status=0;bool exited=false;
  const auto start=std::chrono::steady_clock::now();
  try {
    while(out.value>=0 || err.value>=0 || !exited){
      if(cancelled && cancelled())throw std::runtime_error("Cancelled");
      if(sent==input.size())in.Reset();
      pollfd fds[]={{out.value,POLLIN,0},{err.value,POLLIN,0},{in.value,POLLOUT,0}};
      if(poll(fds,3,20)<0 && errno!=EINTR)Check(errno);
      auto drain=[](Fd& fd,std::string& text){
        if(fd.value<0)return;
        char buf[8192];
        for(;;){
          ssize_t n=read(fd.value,buf,sizeof(buf));
          if(n>0){
            if(text.size()+size_t(n)>4*1024*1024)throw std::runtime_error("Backend output exceeds 4 MiB");
            text.append(buf,size_t(n));
          }else if(n==0){fd.Reset();break;}
          else if(errno==EINTR)continue;
          else if(errno==EAGAIN)break;
          else Check(errno);
        }
      };
      drain(out,result.out);drain(err,result.err);
      if(in.value>=0 && (fds[2].revents&(POLLOUT|POLLERR|POLLHUP))){
        ssize_t n=write(in.value,input.data()+sent,input.size()-sent);
        if(n>0)sent+=size_t(n);
        else if(n<0 && errno!=EAGAIN && errno!=EINTR){if(errno==EPIPE)in.Reset();else Check(errno);}
      }
      if(!exited){pid_t r=waitpid(pid,&status,WNOHANG);if(r==pid)exited=true;else if(r<0 && errno!=EINTR)Check(errno);}
      if(std::chrono::steady_clock::now()-start>std::chrono::milliseconds(timeout_ms))
        throw std::runtime_error("Backend command timed out");
    }
  }catch(...){
    // Do not signal a reaped PID. Backend commands do not leave descendants.
    if(!exited){kill(-pid,SIGKILL);while(waitpid(pid,&status,0)<0 && errno==EINTR){}}
    throw;
  }
  result.code=ExitCode(status);return result;
}
// Supervisor writes to a file: launcher exit cannot break a pipe or end saves.
// No destructor kills the game. The probe owns and inherits the profile lock.
class Game {
 public:
  pid_t pid=0; int exit_code=0;
  void Start(const std::vector<std::string>& args,const std::string& log){
    if(pid)throw std::runtime_error("Game is already running");
    Fd fd(open(log.c_str(),O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC,0600));
    if(fd.value<0)Check(errno);
    Spawn spawn;spawn.Dup(fd.value,1);spawn.Dup(fd.value,2);
    Check(posix_spawn_file_actions_addopen(&spawn.actions,0,"/dev/null",O_RDONLY,0));
    pid=spawn.Start(args);
  }
  bool Poll(){
    if(!pid)return false;
    int status=0;pid_t r=waitpid(pid,&status,WNOHANG);
    if(r==pid){exit_code=ExitCode(status);pid=0;return false;}
    if(r<0 && errno!=EINTR)Check(errno);
    return true;
  }
  void Stop(){if(pid && kill(pid,SIGTERM)<0 && errno!=ESRCH)Check(errno);}
};
}
