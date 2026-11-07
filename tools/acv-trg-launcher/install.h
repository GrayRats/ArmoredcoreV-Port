#pragma once
#include "process.h"
#include <trg/platform.h>
#include <trg/xbox360.h>
#include <filesystem>
#include <atomic>
#include <cstdint>
#include <random>
namespace acv {
// Keep upstream read-only XDVDFS extraction separate from verified publication.
inline std::string InstallDisc(trg::Task& task,const std::string& iso,
                               const std::filesystem::path& destination,
                               const std::vector<std::string>& publish_command) {
  namespace fs=std::filesystem;
  fs::path stage;
  try{
    if(fs::exists(destination))return "Установка уже существует; выберите новую папку программы.";
    if(trg::ReadXbox360TitleId(iso)!=0x4E4D0853)return "Этот ISO не соответствует ACV (Title ID 4E4D0853).";
    fs::create_directories(destination.parent_path());
    static std::atomic<uint64_t> sequence{0};
    std::random_device random;
    std::error_code create_error;
    for(unsigned attempt=0;attempt<32 && stage.empty();++attempt){
      stage=destination;
      stage += ".staging-"+std::to_string(random())+"-"+
               std::to_string(sequence.fetch_add(1,std::memory_order_relaxed));
      if(fs::create_directory(stage,create_error))break;
      if(create_error){stage.clear();throw fs::filesystem_error("create staging directory",destination,create_error);}
      stage.clear();
    }
    if(stage.empty())throw std::runtime_error("Could not allocate a unique staging directory");
    std::string error=trg::ExtractXbox360Disc(task,iso,stage.u8string());
    if(!error.empty() || task.cancelled()){fs::remove_all(stage);return error.empty()?"Cancelled":error;}
    task.SetLabel("Проверка 26 игровых файлов и установка…");
    auto args=publish_command;args.push_back("--staging");args.push_back(stage.u8string());
    auto result=Run(args,"",600000,[&task]{return task.cancelled();});
    if(result.code!=0){fs::remove_all(stage);return result.out.empty() && result.err.empty()?"Проверка установки завершилась с кодом "+std::to_string(result.code):result.out+result.err;}
    // Backend committed by a no-replace atomic rename. No stage remains.
    return "";
  }catch(const std::exception& e){
    if(!stage.empty()){std::error_code ec;fs::remove_all(stage,ec);}
    return e.what();
  }
}
}
