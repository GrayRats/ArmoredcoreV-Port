#pragma once
#include "process.h"
#include <trg/platform.h>
#include <trg/xbox360.h>
#include <filesystem>
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
    std::string pattern=destination.string()+".staging-XXXXXX";
    std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    if(!mkdtemp(name.data()))Check(errno);
    stage=name.data();
    std::string error=trg::ExtractXbox360Disc(task,iso,stage.string());
    if(!error.empty() || task.cancelled()){fs::remove_all(stage);return error.empty()?"Cancelled":error;}
    task.SetLabel("Проверка 26 игровых файлов и установка…");
    auto args=publish_command;args.push_back("--staging");args.push_back(stage.string());
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
