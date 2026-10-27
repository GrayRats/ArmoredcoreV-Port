#include "install.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <cstring>
void Require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void BE(std::vector<unsigned char>& b,size_t p,uint32_t n){for(int i=0;i<4;i++)b.at(p+i)=(n>>(24-i*8))&255;}
void LE(std::vector<unsigned char>& b,size_t p,uint32_t n){for(int i=0;i<4;i++)b.at(p+i)=(n>>(i*8))&255;}
std::string Wait(trg::Task& t,trg::Task::State expected){
  auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(t.running() && std::chrono::steady_clock::now()<limit)std::this_thread::sleep_for(std::chrono::milliseconds(5));
  Require(!t.running(),"task did not finish");std::string e;auto state=t.Finish(&e);Require(state==expected,e.c_str());return e;
}
int main(){try{
 signal(SIGPIPE,SIG_IGN);char pattern[]="/tmp/acv-iso-XXXXXX";Require(mkdtemp(pattern)!=nullptr,"temporary folder");std::filesystem::path base=pattern;
 std::vector<unsigned char> image(0x12000+128);
 const char magic[]="MICROSOFT*XBOX*MEDIA";memcpy(image.data()+0x10000,magic,sizeof(magic));LE(image,0x10014,0x22);LE(image,0x10018,64);
 LE(image,0x11004,0x24);LE(image,0x11008,128);image[0x1100d]=11;memcpy(image.data()+0x1100e,"default.xex",11);
 memcpy(image.data()+0x12000,"XEX2",4);BE(image,0x12014,1);BE(image,0x12018,0x40006);BE(image,0x1201c,0x40);BE(image,0x1204c,0x4E4D0853);
 auto iso=base/"fixture.iso";{std::ofstream f(iso,std::ios::binary);f.write(reinterpret_cast<const char*>(image.data()),image.size());}
 Require(trg::ReadXbox360TitleId(iso.string())==0x4E4D0853,"title id");
 trg::Task t;auto extracted=base/"extracted";
 t.Start("extract",[&](trg::Task& task){return trg::ExtractXbox360Disc(task,iso.string(),extracted.string());});Wait(t,trg::Task::State::kDone);
 Require(std::filesystem::file_size(extracted/"default.xex")==128,"extraction bytes");
 std::ifstream f(extracted/"default.xex",std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(f)),{});
 Require(bytes==std::string(reinterpret_cast<char*>(image.data()+0x12000),128),"payload mismatch");
 auto destination=base/"game";
 t.Start("reject",[&](trg::Task& task){return acv::InstallDisc(task,iso.string(),destination,{"/bin/false"});});Wait(t,trg::Task::State::kFailed);
 Require(!std::filesystem::exists(destination),"failed install published");
 for(const auto& e:std::filesystem::directory_iterator(base))Require(e.path().filename().string().find(".staging-")==std::string::npos,"failed staging leaked");
 t.Start("cancel",[&](trg::Task& task){task.Cancel();return trg::ExtractXbox360Disc(task,iso.string(),(base/"cancelled").string());});Wait(t,trg::Task::State::kCancelled);
 image[0x1204f]=0;{std::ofstream f2(iso,std::ios::binary|std::ios::trunc);f2.write(reinterpret_cast<const char*>(image.data()),image.size());}
 t.Start("wrong title",[&](trg::Task& task){return acv::InstallDisc(task,iso.string(),destination,{"/bin/true"});});Wait(t,trg::Task::State::kFailed);
 std::filesystem::remove_all(base);std::cout<<"PASS XDVDFS title, extraction bytes, failed verification cleanup, cancellation, wrong title rejection\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
