// Bounded host verification entry point; uses the production installer boundary.
#include "install.h"
#include <iostream>
#include <thread>
int main(int argc,char** argv){
  signal(SIGPIPE,SIG_IGN);
  if(argc!=5 && argc!=6){std::cerr<<"install-probe ISO DEST MANIFEST NATIVE_DIR [cancel]\n";return 2;}
  trg::Task task;
  std::vector<std::string> command={"python3","-I","-c",
    "import sys;sys.path.insert(0,sys.argv[1]);import acv_install;acv_install.publish(sys.argv[5],sys.argv[2],sys.argv[3]);print('verified and published')",
    argv[4],argv[2],argv[3]};
  task.Start("Extract",[&](trg::Task& t){return acv::InstallDisc(t,argv[1],std::filesystem::absolute(argv[2]),command);});
  while(task.running()){
    if(argc==6 && task.fraction()>0)task.Cancel();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  std::string error;auto state=task.Finish(&error);
  if(state==trg::Task::State::kCancelled){std::cout<<"cancelled\n";return 3;}
  if(state!=trg::Task::State::kDone){std::cerr<<error<<'\n';return 1;}
  std::cout<<"verified and published\n";return 0;
}
