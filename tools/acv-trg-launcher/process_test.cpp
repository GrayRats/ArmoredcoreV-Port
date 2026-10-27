#include "process.h"
#include <iostream>
#include <fstream>
#include <filesystem>
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
  signal(SIGPIPE,SIG_IGN);
  auto r=acv::Run({"python3","-c","import sys; print(sys.argv[1]); sys.stderr.write('separate'); sys.exit(7)","$(touch /should-not-exist); `false`"});
  Require(r.code==7 && r.err=="separate" && r.out=="$(touch /should-not-exist); `false`\n","argv or exit separation");
  r=acv::Run({"python3","-c","import sys; sys.stdout.write('o'*200000); sys.stderr.write('e'*200000); d=sys.stdin.read(); print(len(d))"},std::string(200000,'i'));
  Require(r.code==0 && r.out.size()==200007 && r.err.size()==200000,"bidirectional pipe drain");
  r=acv::Run({"python3","-c","pass"},std::string(200000,'i'));
  Require(r.code==0,"early stdin closure");
  bool failed=false;try{acv::Run({"/acv/nonexistent"});}catch(const std::exception&){failed=true;}
  Require(failed,"missing executable");
  failed=false;try{acv::Run({"python3","-c","import time; time.sleep(10)"},"",100);}catch(const std::exception&){failed=true;}
  Require(failed,"timeout");
  char dir[]="/tmp/acv-process-XXXXXX";Require(mkdtemp(dir)!=nullptr,"temporary directory");
  std::string log=std::string(dir)+"/game.log";pid_t owned=0;
  {acv::Game game;game.Start({"python3","-c","import time; time.sleep(.2); print('survived launcher')"},log);owned=game.pid;}
  int status=0;while(waitpid(owned,&status,0)<0 && errno==EINTR){}
  std::ifstream stream(log);std::string line;std::getline(stream,line);
  Require(WIFEXITED(status)&&WEXITSTATUS(status)==0&&line=="survived launcher","launcher ownership exit");
  std::filesystem::remove_all(dir);
  std::cout<<"PASS argv, stdout/stderr, exit, bidirectional IO, EPIPE, missing executable, timeout, game survives launcher owner\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
