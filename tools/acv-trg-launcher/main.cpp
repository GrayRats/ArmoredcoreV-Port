#include "process.h"
#include "install.h"
#include <trg/launcher.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#if defined(ACV_LAUNCHER_HAS_VULKAN)
#include "vulkan_renderer.h"
#endif
#include <nlohmann/json.hpp>
#include <filesystem>
#include <future>
#include <fstream>
#include <iostream>
#include <map>
#include <cstdlib>
#include <clocale>
#if !defined(_WIN32)
#include <langinfo.h>
#include <signal.h>
#endif
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Args=std::vector<std::string>;
static fs::path AsPath(const std::string& value){return fs::u8path(value);}
static std::string AsUtf8(const fs::path& value){return value.u8string();}
#if defined(_WIN32)
static constexpr const char* kLauncherPlatform="WINDOWS";
static constexpr const char* kExecutableLabel="Исполняемый файл Windows";
static constexpr const char* kExecutableHint="Выберите Windows host executable для этой сборки; игровой Windows runtime bridge ещё не готов.";
static constexpr const char* kAboutText="ARMORED CORE V · Windows launcher UI / Vulkan renderer preview.\nИгровой Windows runtime bridge ещё не готов.\nИсходные игровые данные предоставляются владельцем игры.";
#else
static constexpr const char* kLauncherPlatform="LINUX";
static constexpr const char* kExecutableLabel="Исполняемый файл Linux";
static constexpr const char* kExecutableHint="Исполняемый файл — программа acv_linux_probe из папки bin/game. Нажмите «Игра из этого пакета», чтобы выбрать комплектную сборку, или «Обзор…» для другой Linux-сборки.";
static constexpr const char* kAboutText="ARMORED CORE V · экспериментальная Linux-сборка.\nTRG Launcher / SDL2 / Dear ImGui.\nИсходные игровые данные предоставляются владельцем игры.";
#endif
static std::string Text(const Json& j){return j.is_string()?j.get<std::string>():j.dump();}
static std::string PythonCommand(){
  if(const char* configured=std::getenv("ACV_PYTHON"))if(*configured)return configured;
#if defined(_WIN32)
  // Prefer the embeddable Python shipped next to the launcher (wine/python/python.exe in the package).
  wchar_t module[MAX_PATH];
  if(GetModuleFileNameW(nullptr,module,MAX_PATH)){
    fs::path bundled=fs::path(module).parent_path()/"python"/"python.exe";
    std::error_code ignored;
    if(fs::is_regular_file(bundled,ignored))return acv::Utf8(bundled.wstring());
  }
  return "python";
#else
  return "python3";
#endif
}
static Args Utf8Arguments(int argc,char** argv){
#if defined(_WIN32)
  (void)argc;(void)argv;
  int count=0;LPWSTR* wide=CommandLineToArgvW(GetCommandLineW(),&count);
  if(!wide)throw std::runtime_error("CommandLineToArgvW failed");
  struct FreeArgs{LPWSTR* p;~FreeArgs(){LocalFree(p);}} cleanup{wide};
  Args result;result.reserve(static_cast<size_t>(count));
  for(int i=0;i<count;++i)result.push_back(acv::Utf8(wide[i]));
  return result;
#else
  Args result;result.reserve(static_cast<size_t>(argc));
  for(int i=0;i<argc;++i)result.emplace_back(argv[i]);
  return result;
#endif
}
static fs::path FindRoot(){
  Args starts;
#if defined(_WIN32)
  DWORD needed=GetEnvironmentVariableW(L"ACV_ROOT",nullptr,0);
  if(needed){std::wstring value(needed,L'\0');DWORD length=GetEnvironmentVariableW(L"ACV_ROOT",value.data(),needed);if(length && length<needed){value.resize(length);starts.push_back(acv::Utf8(value));}}
#else
  if(auto env=getenv("ACV_ROOT"))starts.emplace_back(env);
#endif
  if(char* base=SDL_GetBasePath()){starts.emplace_back(base);SDL_free(base);}
  for(const auto& start:starts){
    auto p=fs::absolute(AsPath(start));
    while(!p.empty()){
      if(fs::is_regular_file(p/"tools/native-rexglue/probe_linux.py"))return p;
      if(p==p.parent_path())break;
      p=p.parent_path();
    }
  }
  throw std::runtime_error("Не найден каталог игры. Укажите ACV_ROOT.");
}
struct App {
  fs::path root;std::string profiles;
  trg::CallbackSettings store;
  Json snapshot,settings,saved_settings,observed,preview_input,ephemeral=Json::object(),last,backups;
  std::string edited_profile;
  std::chrono::steady_clock::time_point changed_at;
  std::future<Json> work;
  enum class Job {None,Snapshot,Save,Play,Reset,Profile,Last,Preview,Backups,Preset};
  Job job=Job::None;
  acv::Game game;trg::Task install;
  std::string message="Чтение настроек…",session_log;
  bool ready=false;
  std::chrono::steady_clock::time_point game_start,last_poll;
  explicit App(fs::path r,std::string p):root(std::move(r)),profiles(std::move(p)){
    store.true_value="true";store.false_value="false";
    store.get=[this](std::string_view key){
      auto [section,name]=Split(key);
      const auto& doc=section=="ui"?ephemeral:settings.value(section,Json::object());
      return doc.contains(name)?Text(doc.at(name)):"";
    };
    store.set=[this](std::string_view key,const std::string& value){
      auto [section,name]=Split(key);
      if(section=="ui"){ephemeral[name]=value;return;}
      if(section=="graphics"){
        const auto& cap=snapshot.at("capabilities").at(name);
        if(cap["options"].is_array()){
          for(const auto& opt:cap["options"])if(Text(opt["value"])==value){settings[section][name]=opt["value"];if(name=="color_depth" && opt["value"]==10)settings[section]["dither"]=false;return;}
          throw std::runtime_error("Некорректное значение настройки");
        }
      }
      if(section=="launcher" && (name=="allow_experimental" || name=="mangohud" || name=="mangohud_host_fps")){
        // TRG Toggle stores "on"/"off"; accept the textual booleans too.
        settings[section][name]=(value=="true" || value=="on" || value=="1");
      }else settings[section][name]=value;
    };
    store.save=[this](){return Save(false);};
    store.reset=[this](){if(!Busy()){for(auto i=snapshot.at("capabilities").begin();i!=snapshot.at("capabilities").end();++i)settings["graphics"][i.key()]=i.value().at("default");message="Графика сброшена; изменения не сохранены";}};
    Submit(Job::Snapshot,Bridge("snapshot"));
  }
  static std::pair<std::string,std::string> Split(std::string_view key){
    auto n=key.find('.');if(n==std::string_view::npos)throw std::runtime_error("Setting needs a section");
    return {std::string(key.substr(0,n)),std::string(key.substr(n+1))};
  }
  void Log(const std::string& event){
    if(!ready || !snapshot.contains("logs_root"))return;
    try{
      fs::path directory=AsPath(snapshot.at("logs_root").get<std::string>());fs::create_directories(directory);
      std::ofstream file(directory/"acv-trg-launcher.log",std::ios::app);
      auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      file<<now<<" "<<event<<'\n';
    }catch(const std::exception& e){std::cerr<<"journal: "<<e.what()<<std::endl;}
  }
  bool Busy()const{return job!=Job::None;}
  bool Installing()const{return install.running();}
  Args Bridge(const std::string& command)const{
    Args a={PythonCommand(),"-X","utf8","-I",AsUtf8(root/"tools/acv-trg-launcher/backend.py")};
    if(!profiles.empty()){a.push_back("--profiles-root");a.push_back(profiles);}
    a.push_back(command);return a;
  }
  Args Cli(Args a)const{
    Args out={PythonCommand(),"-X","utf8","-I",AsUtf8(root/"tools/native-rexglue/acv_profile_cli.py"),"--profiles-root",snapshot.at("profiles_root").get<std::string>()};
    out.insert(out.end(),a.begin(),a.end());return out;
  }
  static Json Execute(const Args& args,const std::string& input){
    auto result=acv::Run(args,input,120000);
    Json parsed;
    try{parsed=Json::parse(result.out);}catch(...){throw std::runtime_error("Не удалось прочитать ответ службы настроек: "+result.err+result.out);}
    if(result.code || parsed.contains("error"))throw std::runtime_error(parsed.is_object()?parsed.value("error",result.err):result.err);
    return parsed;
  }
  void Submit(Job kind,Args args,std::string input=""){
    if(Busy())return;
    if(kind==Job::Save || kind==Job::Play)Log("settings_requested "+input);
    job=kind;if(kind==Job::Preview)preview_input=settings;
    if(kind!=Job::Last && kind!=Job::Preview)message="Выполняется…";
    Args refresh=Bridge("snapshot");
    work=std::async(std::launch::async,[kind,args=std::move(args),input=std::move(input),refresh](){
      Json result=Execute(args,input);
      if(kind==Job::Save || kind==Job::Play || kind==Job::Reset || kind==Job::Profile){auto action=result;result=Execute(refresh,"");result["action_result"]=std::move(action);}
      return result;
    });
  }
  bool Save(bool play){
    if(Busy()||Installing()||!ready)return false;
    Submit(play?Job::Play:Job::Save,Bridge("save"),settings.dump());return true;
  }
  void Profile(Args a){if(!Busy() && !Installing())Submit(Job::Profile,Cli(std::move(a)));}
  std::string Selected()const{return ready?settings.at("launcher").value("profile",""):"";}
  void StartGame(){
#if defined(_WIN32)
    if(game.pid)throw std::runtime_error("Игра уже запущена");
    fs::path probe=root/"bin/game/acv_windows_probe.exe";
    std::error_code ignored;
    if(!fs::is_regular_file(probe,ignored))
      throw std::runtime_error("Windows-сборка игры не найдена: bin/game/acv_windows_probe.exe. Соберите её или выберите exe в «Выбор игры».");
    fs::path logs=root/"logs";fs::create_directories(logs,ignored);
    auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    session_log=AsUtf8(logs/("trg-session-"+std::to_string(stamp)+".log"));
    // Full command line and environment come from the same settings/profile/config logic as probe_linux.py
    // (backend.py launch-spec): SDK/graphics cvars, input layout (SDK default mnk_mode=false disables keyboard/mouse),
    // pilot name and gamertag, DLC/system mounts, profile XUID. Without them the probe hangs or misbehaves.
    Json spec=Execute(Bridge("launch-spec"),"");
    std::vector<std::string> args{AsUtf8(probe)};
    for(const auto& a:spec.at("args"))args.push_back(a.get<std::string>());
    for(const auto& item:spec.at("env").items())  // the child inherits this process environment
      SetEnvironmentVariableW(acv::Wide(item.key()).c_str(),acv::Wide(item.value().get<std::string>()).c_str());
    Log("launch_spec log="+spec.value("log_file","")+" input="+spec.value("input_mode","")+" args="+std::to_string(args.size()));
    game.Start(args,session_log);game_start=std::chrono::steady_clock::now();last_poll=game_start;
    last=nullptr;message="Игра запущена";
    Log("game_started pid="+std::to_string(game.pid)+" log="+session_log);
    return;
#else
    if(game.pid)throw std::runtime_error("Игра уже запущена");
    if(!snapshot.value("build_exists",false))throw std::runtime_error("Выбранный исполняемый файл игры не найден");
    if(!snapshot.value("game_data_exists",false))throw std::runtime_error("Игровые данные не готовы");
    if(settings["launcher"].value("mangohud",false) && !snapshot.at("mangohud").value("available",false))
      throw std::runtime_error(snapshot.at("mangohud").at("reason").get<std::string>());
    if(snapshot.at("shm").at("free").get<uint64_t>()<6ULL*1024*1024*1024)
      throw std::runtime_error("Для запуска требуется не менее 6 ГБ свободного /dev/shm");
    Args args={PythonCommand(),"-X","utf8","-I",AsUtf8(root/"tools/native-rexglue/probe_linux.py"),"--session","--profile",Selected(),"--profiles-root",snapshot.at("profiles_root"),"--settings",snapshot.at("settings_path"),"--executable",settings.at("launcher").at("build")};
    if(settings["launcher"].value("allow_experimental",false))args.push_back("--allow-experimental");
    auto driver=settings["launcher"].value("video_driver","x11");
    if(driver!="auto"){args.push_back("--video-driver");args.push_back(driver);}
    fs::path logs=AsPath(snapshot.at("logs_root").get<std::string>());fs::create_directories(logs);
    auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    session_log=AsUtf8(logs/("trg-session-"+std::to_string(stamp)+".log"));
    game.Start(args,session_log);game_start=std::chrono::steady_clock::now();last_poll=game_start;
    last=nullptr;message="Игра запущена";
    Log("game_started profile="+Selected()+" pid="+std::to_string(game.pid)+" log="+session_log);
    std::cout<<"game_started pid="<<game.pid<<" log="<<session_log<<std::endl;
#endif
  }
  bool Dirty()const{return ready && settings!=saved_settings;}
  void ApplyPreset(const std::string& name){
    if(Busy() || Installing())return;
    auto args=Bridge("preset");args.push_back("--preset");args.push_back(name);
    Submit(Job::Preset,std::move(args),settings.dump());
  }
  void SyncProfile(){
    auto selected=Selected();if(selected==edited_profile)return;
    edited_profile=selected;backups=nullptr;ephemeral["backup"]="";
    ephemeral["name"]="";ephemeral["gamertag"]="";ephemeral["pilot"]="";
    for(const auto& p:snapshot.at("profiles"))if(p.at("uuid")==selected){
      ephemeral["name"]=p.at("display_name");ephemeral["gamertag"]=p.at("gamertag");ephemeral["pilot"]=p.at("pilot_name");
    }
  }
  void Tick(){
    std::string install_error;auto installed=install.Finish(&install_error);
    if(installed==trg::Task::State::kDone){if(!Busy())Submit(Job::Snapshot,Bridge("snapshot"));}
    else if(installed==trg::Task::State::kFailed || installed==trg::Task::State::kCancelled){message=install_error.empty()?"Установка отменена":install_error;}
    if(Busy() && work.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
      Job finished=job;job=Job::None;
      try{
        Json result=work.get();
        if(finished==Job::Last){last=std::move(result);}
        else if(finished==Job::Backups){backups=std::move(result);message=backups.empty()?"Резервных копий пока нет":"Список копий обновлён";}
        else if(finished==Job::Preset){
          snapshot=std::move(result);settings=snapshot.at("settings");preview_input=nullptr;
          message="Набор применён к черновику. Сохраните изменения для запуска.";
        }
        else if(finished==Job::Preview){
          if(settings==preview_input){snapshot=std::move(result);message="Возможности сборки обновлены; изменения не сохранены";}
        }
        else{
          bool preserve=ready && (finished==Job::Snapshot || finished==Job::Profile);
          Json previous=settings;snapshot=std::move(result);settings=snapshot.at("settings");saved_settings=settings;
          if(preserve)settings=previous;
          if(finished==Job::Profile){
            settings=std::move(previous);
            const auto& action=snapshot.at("action_result");
            if(action.is_object() && action.contains("uuid")){settings["launcher"]["profile"]=action["uuid"];edited_profile.clear();}
            if(action.is_object() && action.contains("backup"))ephemeral["backup"]=action["backup"];
          }
          ready=true;preview_input=nullptr;message=(finished==Job::Save)?"Настройки сохранены":"Готово";SyncProfile();
        }
        if(finished==Job::Play)StartGame();
      }catch(const std::exception& e){message=e.what();Log("backend_error "+message);std::cerr<<"backend: "<<message<<std::endl;}
    }
    if(ready && observed!=settings){observed=settings;changed_at=std::chrono::steady_clock::now();}
    if(ready && !Busy() && !Installing() && snapshot.at("settings")!=settings && observed==settings &&
       std::chrono::steady_clock::now()-changed_at>std::chrono::milliseconds(400) && preview_input!=settings){
      Submit(Job::Preview,Bridge("preview"),settings.dump());
    }
    if(game.pid){
      try{if(!game.Poll()){
        message="Игра завершена, код "+std::to_string(game.exit_code);
        Log("game_exit code="+std::to_string(game.exit_code));
        std::cout<<"game_exit code="<<game.exit_code<<std::endl;
        if(!Busy())Submit(Job::Last,Cli({"last-run"}));
      }}catch(const std::exception& e){message=e.what();}
      auto now=std::chrono::steady_clock::now();
      if(game.pid && !Busy() && now-last_poll>std::chrono::seconds(3)){
        last_poll=now;Submit(Job::Last,Cli({"last-run"}));
      }
    }
  }
  trg::PlayCheck CanPlay()const{
    if(!ready)return {false,"Настройки ещё не загружены",0};
    if(Busy() || Installing())return {false,"Дождитесь завершения операции",0};
#if defined(_WIN32)
    if(game.pid)return {false,"Игра уже запущена",0};
    if(!std::filesystem::is_regular_file(root/"bin/game/acv_windows_probe.exe"))
      return {false,"Windows-сборка игры не найдена: bin/game/acv_windows_probe.exe",0};
    if(!snapshot.value("game_data_exists",false))return {false,"Файлы игры отсутствуют или не соответствуют проверенному набору",0};
    if(Selected().empty())return {false,"Создайте или выберите профиль",1};
    return {};
#else
    if(game.pid)return {false,"Игра уже запущена",0};
    if(!snapshot.value("build_exists",false))return {false,"Выберите исполняемый файл игры",0};
    if(!snapshot.value("game_data_exists",false))return {false,"Файлы игры отсутствуют или не соответствуют проверенному набору",0};
    if(Selected().empty())return {false,"Создайте или выберите профиль",1};
    return {};
#endif
  }
  void Choice(trg::Ui& ui,const char* key,const char* label,const char* help=""){
    const auto& cap=snapshot.at("capabilities").at(key);
    std::vector<trg::Option> options;std::string reasons;
    for(const auto& opt:cap["options"]){
      std::string v=Text(opt["value"]);
      if(opt.value("selectable",false))options.push_back({v,(std::string(key)=="fps_limit" && v=="0")?std::string("Без лимита"):Label(v)});
      else if(opt.contains("reason"))reasons+=Label(v)+": "+opt.at("reason").get<std::string>()+"\n";
    }
    std::string id=std::string("graphics.")+key;
    ui.Combo(label,help,id.c_str(),Text(cap["default"]).c_str(),options);
    if(!reasons.empty() && ui.Section((std::string("Недоступные варианты##")+key).c_str()))ui.Help(reasons.c_str());
  }
  static std::string Label(const std::string& s){
    static const std::map<std::string,std::string> labels={{"true","Вкл."},{"false","Выкл."},{"windowed","Окно"},{"fullscreen","Полный экран"},{"off","Выкл."},{"game","Как в игре"},{"default","По умолчанию"},{"bilinear","Билинейная"},{"cas","CAS"},{"fsr","FSR 1"},{"rgb_full","RGB Full Range"},{"auto","Авто"},{"full","Полный (точный)"},{"fast","Быстрый (отложенный)"},{"fxaa","FXAA"},{"fxaa_extreme","FXAA усиленное"}};
    auto i=labels.find(s);return i==labels.end()?s:i->second;
  }
  void LaunchPage(trg::Ui& ui){
    auto check=CanPlay();
    ui.StatusCard(game.pid?trg::Status::kBusy:(check.ok?trg::Status::kReady:trg::Status::kAttention),game.pid?"Игра работает":(check.ok?"Готово к запуску":"Требуется настройка"),game.pid?"Закрытие лаунчера оставит игру работающей":check.reason.c_str());
    if(!snapshot.value("game_data_exists",false)){
      ui.Paragraph(snapshot.at("installation").value("reason","").c_str());
      ui.InputText("Образ собственного диска","Поддерживается проверенная версия ACV; исходный ISO не меняется","ui.iso");
      if(ui.ButtonRow("Выбор образа","Можно также перетащить ISO в окно","Обзор…")){auto path=trg::BrowseForFile("Выберите образ диска",{{"Образы дисков","*.iso"}});if(!path.empty())ephemeral["iso"]=path;}
      if(!Installing() && ui.ButtonRow("Установка игры","Файлы будут проверены перед публикацией","Установить из ISO",true)){
        auto iso=store.Get("ui.iso");fs::path destination=AsPath(snapshot.at("game_root").get<std::string>());auto command=Bridge("publish-install");
        install.Start("Извлечение игровых файлов…",[iso,destination,command](trg::Task& t){return acv::InstallDisc(t,iso,destination,command);});
      }
      ui.EndRows();ui.TaskProgress(install);
    }
    if(game.pid){
      auto seconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-game_start).count();
      ui.Info("Время с запуска",(std::to_string(seconds)+" с").c_str());
      if(seconds<150)ui.Help("Первая загрузка обычно занимает 1–2 минуты.");
      if(last.is_object() && last.contains("idle_seconds") && last["idle_seconds"].is_number() && last["idle_seconds"].get<double>()>=60)
        ui.Help("В журнале нет новых записей более минуты. Возможна долгая загрузка или зависание.");
      if(ui.ButtonRow("Остановить игру","Штатное завершение процесса с сохранением профиля","Остановить"))game.Stop();
    }
    ui.InputText("Исполняемый файл",kExecutableHint,"launcher.build");
    ui.EndRows();
    ui.Help(kExecutableHint);
    ui.Help(("Файл комплектной игры: "+snapshot.at("bundled_build").get<std::string>()).c_str());
    if(ui.ButtonRow("Выбор игры",kExecutableLabel,"Обзор…")){auto path=trg::BrowseForFile("Выберите исполняемый файл игры");if(!path.empty())settings["launcher"]["build"]=path;}
    if(ui.ButtonRow("Игра из этого пакета","Вернуть путь к комплектной сборке","Выбрать"))settings["launcher"]["build"]=snapshot.at("bundled_build");
#if !defined(_WIN32)
    ui.Choice("Оконная система","X11 проверен; Wayland зависит от сборки","launcher.video_driver","x11",{{"x11","X11"},{"wayland","Wayland"},{"auto","Авто"}});
#endif
    ui.Choice("Управление","Раскладки: input_keyboard.ini и input_gamepad.ini. «Клавиатура» отключает мышь; «Геймпад» использует подключённый контроллер SDL. Применяется при следующем запуске игры","launcher.input_mode","keyboard_mouse",{{"keyboard_mouse","Клавиатура и мышь"},{"keyboard","Клавиатура"},{"gamepad","Геймпад"}});
    if(ui.ButtonRow("Контроллер",snapshot.at("input").value("status","").c_str(),"Обновить") && !Busy())Submit(Job::Snapshot,Bridge("snapshot"));
    ui.Toggle("MangoHud в игре","Guest — частота подачи кадров игрой, среднее за 300 кадров; счётчик Host FPS отключён (считает показы окна). Применяется при следующем запуске","launcher.mangohud",false,"Выкл.","Вкл.");
    ui.Help(snapshot.at("mangohud").at("reason").get<std::string>().c_str());
#if !defined(_WIN32)
    ui.Toggle("Host FPS (MangoHud)","Счётчик показов окна: повторяет последний кадр игры, не равен FPS игры. Выключено по умолчанию. Применяется при следующем запуске с MangoHud","launcher.mangohud_host_fps",false,"Выкл.","Вкл.");
#endif
    ui.Toggle("Экспериментальные настройки","Применять только доступные выбранной сборке варианты","launcher.allow_experimental",false,"Выкл.","Вкл.");
    if(ui.ButtonRow("Обновить состояние","Обновить сведения об игре, сохранив текущие правки","Обновить"))Submit(Job::Snapshot,Bridge("snapshot"));
    if(ui.ConfirmRow("Отменить правки","Вернуть последние сохранённые настройки","Вернуть","Отменить несохранённые изменения?","Вернуть","Отмена")){settings=saved_settings;preview_input=nullptr;}
    ui.FolderRow("Журналы игры","Открыть каталог журналов","Открыть",snapshot.at("logs_root"));
    if(ui.ButtonRow("Последняя сессия","Запрошенные и фактические параметры, причины fallback","Показать"))Submit(Job::Last,Cli({"last-run"}));
    if(last.is_object()){
      if(last.contains("applied") && last["applied"].is_object()){
        auto a=last["applied"];
        std::string text=std::to_string(a.value("width",0))+"×"+std::to_string(a.value("height",0))+", "+Text(a.value("bits",Json()))+" бит, "+a.value("present_mode","");
        ui.Info("Фактический вывод",text.c_str());
        ui.Info("Запрошенная глубина",Text(a.value("requested_bits",Json())).c_str());
        if(a.value("depth_fallback",false))ui.Help("Использован доступный формат поверхности вместо запрошенной глубины.");
      }
      if(last.contains("output") && last["output"].is_object()){
        auto o=last["output"];
        ui.Info("Обработка изображения",o.value("final_effect_name","").c_str());
        std::string geometry=std::to_string(o.value("source_width",0))+"×"+std::to_string(o.value("source_height",0))+" → "+std::to_string(o.value("width",0))+"×"+std::to_string(o.value("height",0));
        ui.Info("Размер изображения",geometry.c_str());
      }
      if(last.contains("pacing_last") && last["pacing_last"].is_object()){
        auto p=last["pacing_last"];
        std::string text=Text(p.value("fps",Json()))+" FPS, p95 "+Text(p.value("p95_ms",Json()))+" мс";
        ui.Info("Подача кадров",text.c_str());ui.Help("Частота подачи кадров не измеряет скорость игры.");
      }
      if(ui.Section("Подробности последней сессии"))ui.Paragraph(last.dump(2).c_str());
    }
  }
  void ProfilesPage(trg::Ui& ui){
    std::vector<trg::Option> options;
    for(const auto& p:snapshot.at("profiles"))options.push_back({p.at("uuid"),p.at("display_name")});
    ui.Combo("Профиль","UUID и XUID сохраняются при переименовании","launcher.profile","",options);
    SyncProfile();std::string selected=Selected();
    for(const auto& p:snapshot.at("profiles"))if(p.at("uuid")==selected){
      ui.Info("XUID",p.at("xuid").get_ref<const std::string&>().c_str());
      ui.Info("Позывной",p.at("gamertag").get_ref<const std::string&>().c_str());
      ui.Info("Пилот",p.at("pilot_name").get_ref<const std::string&>().c_str());
    }
    ui.InputText("Название","Новое имя профиля","ui.name");
    ui.InputText("Позывной","Не более 15 символов","ui.gamertag");
    ui.InputText("Пилот","Не более 32 символов","ui.pilot");
    auto name=store.Get("ui.name"),tag=store.Get("ui.gamertag"),pilot=store.Get("ui.pilot");
    ImGui::BeginDisabled(name.find_first_not_of(" \t\r\n")==std::string::npos);
    if(ui.ButtonRow("Новый профиль","Создать отдельные сохранения","Создать")){
      Args a={"create",name};if(!tag.empty()){a.push_back("--gamertag");a.push_back(tag);}if(!pilot.empty()){a.push_back("--pilot");a.push_back(pilot);}Profile(a);
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(selected.empty()||game.pid);
    ImGui::BeginDisabled(name.find_first_not_of(" \t\r\n")==std::string::npos);
    if(ui.ButtonRow("Переименовать","Идентичность и сохранения не меняются","Применить имя"))Profile({"rename",selected,name});
    ImGui::EndDisabled();
    if(ui.ButtonRow("Имена в игре","Изменения применяются при следующем вводе имени в игре","Применить")){
      Args a={"set-names",selected};if(!tag.empty()){a.push_back("--gamertag");a.push_back(tag);}if(!pilot.empty()){a.push_back("--pilot");a.push_back(pilot);}Profile(a);
    }
    if(ui.ButtonRow("Резервная копия","Проверка целостности всех файлов сохранений","Создать копию"))Profile({"backup",selected});
    if(ui.ButtonRow("Список резервных копий","Проверить копии выбранного профиля","Показать копии"))Submit(Job::Backups,Cli({"backups",selected}));
    if(backups.is_array() && ui.Section("Резервные копии",true)){
      for(const auto& backup:backups){
        std::string path=backup.at("path");
        ui.Info(backup.value("valid",false)?"Проверена":"Повреждена",path.c_str());
        if(backup.value("valid",false) && ui.ButtonRow(path.c_str(),"Выбрать для восстановления","Выбрать"))ephemeral["backup"]=path;
      }
    }
    ui.InputText("Каталог резервной копии","Для восстановления выбранного профиля","ui.backup");
    if(ui.ButtonRow("Выбрать копию","Каталог проверенной резервной копии","Обзор…")){auto path=trg::BrowseForFolder("Выберите резервную копию");if(!path.empty())ephemeral["backup"]=path;}
    ImGui::BeginDisabled(store.Get("ui.backup").empty());
    if(ui.ConfirmRow("Восстановление","Перед восстановлением будет создана копия текущих сохранений","Восстановить","Восстановить выбранную резервную копию?","Восстановить","Отмена"))Profile({"restore",selected,store.Get("ui.backup")});
    ImGui::EndDisabled();
    ui.InputText("Импорт сохранений","Каталог данных для выбранного профиля","ui.import");
    if(ui.ButtonRow("Выбрать данные","Каталог сохранений для импорта","Обзор…")){auto path=trg::BrowseForFolder("Выберите сохранения");if(!path.empty())ephemeral["import"]=path;}
    ImGui::BeginDisabled(store.Get("ui.import").empty());
    if(ui.ButtonRow("Импорт","Копирование с проверкой целостности","Импортировать"))Profile({"import",selected,store.Get("ui.import")});
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ui.FolderRow("Данные профилей","Резервные копии и сохранения","Открыть",snapshot.at("profiles_root"));
  }
  void ScreenPage(trg::Ui& ui){
    Choice(ui,"window_mode","Режим экрана");Choice(ui,"monitor","Монитор","Индекс дисплея игрового backend");
    ui.Combo("Группа разрешений","Фильтр списка; текущее разрешение сохраняется","ui.resolution_group","all",{{"all","Все"},{"16:9","16:9"},{"16:10","16:10"},{"UltraWide","UltraWide"},{"32:9","32:9"}});
    std::vector<trg::Option> resolutions;
    auto group=store.Get("ui.resolution_group","all");
    auto selected=store.Get("graphics.resolution","auto");
    for(const auto& mode:snapshot.at("resolution_presets")){
      std::string value=mode.at(0),label=mode.at(1);
      if(group=="all" || value=="auto" || value==selected || label.find("("+group+")")!=std::string::npos)resolutions.push_back({value,label});
    }
    ui.Combo("Разрешение","Размер вывода; в полном экране выбирайте режим вашего монитора. Расширенный обзор вычисляется по отношению ширины к высоте.","graphics.resolution","auto",resolutions);
    ui.EndRows();
    ui.Help("Для расширенного обзора выберите 16:10, UltraWide или 32:9 и включите «Расширенный обзор». Своё разрешение тоже поддерживается при отношении сторон от 1.2 до 4.0; полноэкранный режим должен поддерживаться монитором.");
    ui.InputText("Своё разрешение","Формат: 2560x1080; Авто задаётся значением auto","graphics.resolution");
    Choice(ui,"render_scale","Масштаб рендера");Choice(ui,"vsync","VSync");Choice(ui,"fps_limit","Лимит FPS","30/60/120 — потолок подачи кадров, а не гарантированная частота; «Без лимита» отключает ограничение. При включённом VSync частоту всё равно ограничивает монитор: для 120 и «Без лимита» выключите VSync. Замер: сама игра подаёт не более ~60 кадров/с, поэтому 120 и «Без лимита» пока не дают выигрыша выше 60.");
    Choice(ui,"wide_view","Расширенный обзор (эксперимент)","Поддерживает 16:10, UltraWide и 32:9; также своё отношение сторон от 1.2 до 4.0. Нужны явное разрешение и экспериментальные настройки. 16:9 сохраняет исходный обзор.");
    ui.EndRows();
    ui.Help("Расширенный обзор согласует перспективу и отсечение по краям; HUD сохраняет центральную область. Выберите явное разрешение, например 2560x1080.");
    const Json display=last.is_object() && last.contains("monitors")?last:snapshot.value("display_info",Json::object());
    if(display.is_object() && display.contains("display_modes") && display["display_modes"].is_array()){
      std::map<std::string,std::string> seen;
      for(const auto& mode:display["display_modes"]){
        if(!mode.is_object())continue;
        std::string value=std::to_string(mode.value("width",0))+"x"+std::to_string(mode.value("height",0));
        if(mode.value("width",0)>0 && mode.value("height",0)>0)seen[value]=value;
      }
      std::vector<trg::Option> modes;
      for(const auto& [value,label]:seen)modes.push_back({value,label});
      if(!modes.empty()){
        // Both pickers bind graphics.resolution; give the reported modes their own ID scope.
        // Tables push IDs too, so finish each table before pushing/popping our scope.
        ui.EndRows();ImGui::PushID("screen.reported_display_modes");
        ui.Combo("Режимы из последнего запуска","Данные игрового backend; можно также ввести размер вручную","graphics.resolution","auto",modes);
        ui.EndRows();ImGui::PopID();
      }
    }
    if(display.is_object() && display.contains("monitors") && display["monitors"].is_array())for(const auto& m:display["monitors"]){
      if(!m.is_object())continue;
      std::string label="Монитор "+std::to_string(m.value("index",0));
      std::string text=std::to_string(m.value("width",0))+"×"+std::to_string(m.value("height",0))+", "+Text(m.value("refresh_hz",Json()))+" Гц";
      ui.Info(label.c_str(),text.c_str());
    }
  }
  void ColorPage(trg::Ui& ui){
    ui.Help("10 бит — запрос SDR-формата. Для выбора включите «Экспериментальные настройки» на странице «Запуск». При отсутствии поддержки игра применит 8 бит и покажет фактический результат ниже.");
    Choice(ui,"color_encoding","Кодирование цвета");
    Choice(ui,"color_depth","Глубина цвета","10 бит — экспериментальный запрос SDR, не HDR. Нужна поддержка поверхности и экрана; иначе игра явно вернётся к 8 бит. Выбор 10 бит отключает 8-битный дизеринг.");
    ImGui::BeginDisabled(settings["graphics"].value("color_depth",8)==10);Choice(ui,"dither","Дизеринг","Сглаживает цветовые полосы при выводе в 8 бит; отключается при запросе 10 бит");ImGui::EndDisabled();
    ui.EndRows();ui.Help("Дизеринг добавляет слабый шум к цветам при переводе изображения в 8 бит. Он делает полосы на небе, дыме и других плавных градиентах менее заметными. Это не сглаживание контуров. При запросе 10 бит выключен.");
    if(ui.ButtonRow("Фактический формат","Прочитать результат последней игровой сессии","Обновить"))Submit(Job::Last,Cli({"last-run"}));
    const auto& display=last.is_object() && last.contains("applied")?last:snapshot.at("display_info");
    if(display.is_object() && display.contains("applied") && display["applied"].is_object()){
      const auto& applied=display.at("applied");
      ui.Info("Фактический формат последней сессии",(Text(applied.value("bits",Json()))+" бит · "+applied.value("format_name","")).c_str());
      if(applied.value("depth_fallback",false))ui.Help("Запрос 10 бит не поддержан поверхностью: применено 8 бит.");
    }else ui.Help("Фактическая глубина цвета появится после запуска. Наличие пункта 10 бит не подтверждает 10-битный вывод.");
  }
  void GraphicsPage(trg::Ui& ui){
    ui.Choice("Набор настроек","Применение меняет графику; кнопка «Сохранить» записывает изменения","ui.preset","compatibility",{{"compatibility","Производительность"},{"quality","Графика"},{"streaming","Плавность"}});
    if(ui.ButtonRow("Применить набор","Профиль и сохранения сохраняются","Применить")){
      ApplyPreset(store.Get("ui.preset","compatibility"));
    }
    ui.EndRows();ui.Help("Производительность: 1× рендер, AF 2×, без постсглаживания, билинейный вывод. Графика: AF 2×, FSR 1, удержание 800 с и увеличенный кэш; включает экспериментальные настройки. Плавность: средние параметры кэша и подгрузки, AF 4×. Наборы сохраняют разрешение и глубину цвета, задают лимит 60 FPS.");
    Choice(ui,"anisotropy","Анизотропная фильтрация");Choice(ui,"antialiasing","Сглаживание");Choice(ui,"output_effect","Масштабирование","CAS и FSR 1 интегрированы в эту сборку. Качество картинки не проверено; нужны экспериментальные настройки");ImGui::BeginDisabled(settings["graphics"].value("output_effect","")!="cas");Choice(ui,"sharpness","Резкость CAS");ImGui::EndDisabled();
    ImGui::BeginDisabled(settings["graphics"].value("output_effect","")!="fsr");Choice(ui,"fsr_sharpness","Резкость FSR");ImGui::EndDisabled();
    ui.Spacer();
    ui.Info("Память и подгрузка","Изменения применяются при следующем запуске игры");
    ui.EndRows();ui.Help(snapshot.at("memory").at("reason").get<std::string>().c_str());
    Choice(ui,"readback_resolve","Возврат текстур в RAM (readback)","Полный: данные текстур возвращаются синхронно, картинка точная (рекомендуется), FPS ниже. Быстрый: берутся данные предыдущего кадра, FPS заметно выше, но возможны чёрные детали мехов и территории. Применяется при следующем запуске");
    Choice(ui,"texture_cache_soft_mb","Целевой кэш текстур (МиБ)","Неиспользуемые текстуры сохраняются до срока удержания, пока не достигнут жёсткий предел");
    Choice(ui,"texture_cache_hard_mb","Предел кэша текстур (МиБ)","Оставляйте запас VRAM для экрана, буферов и других приложений");
    Choice(ui,"texture_cache_retention_s","Удержание текстур (сек.)","Уменьшает повторную загрузку недавно использованных текстур; жёсткий предел памяти сохраняется");
    Choice(ui,"asset_read_ahead_mb","Фоновая подгрузка (МиБ)","Авто: 16 МиБ в поддерживаемом runtime. Подготавливает следующие части игровых архивов в RAM; сохранения не затрагиваются");
    if(ui.ConfirmRow("Сброс графики","Профили и сохранения сохраняются","Сбросить","Сбросить настройки графики?","Сбросить","Отмена"))store.ResetToDefaults();
  }
  trg::LauncherConfig Config(){
    trg::LauncherConfig c;c.branding.title="ARMORED CORE V";c.branding.subtitle=kLauncherPlatform;c.theme=trg::Theme::Ember();c.settings=&store;
    c.on_file_drop=[this](const std::string& path){ephemeral["iso"]=path;};
    c.save_on_play=false;c.can_play=[this]{return CanPlay();};
    c.hints="Enter  Запуск      Esc  Выход      Ctrl+S  Сохранить";c.note="Настройки проверяются и сохраняются перед запуском.";
    auto page=[this](const char* name,const char* blurb,auto draw){return trg::Page{name,blurb,[this,draw](trg::Ui& ui){
      if(!ready){ui.Paragraph(message.c_str());return;}
      ImGui::BeginDisabled(Busy());(this->*draw)(ui);ImGui::EndDisabled();
      ui.Spacer();if(Dirty())ui.Help("Есть несохранённые изменения. Сохраните их или запустите игру, чтобы применить.");ui.Help(message.c_str());
    },{}};};
    c.pages.push_back(page("Запуск","Профиль, сборка и состояние игры",&App::LaunchPage));
    c.pages.push_back(page("Профиль и сохранения","Отдельные данные и резервные копии",&App::ProfilesPage));
    c.pages.push_back(page("Экран","Разрешение, пропорции и частота кадров",&App::ScreenPage));
    c.pages.push_back(page("Цвет","Формат вывода на экран",&App::ColorPage));
    c.pages.push_back(page("Графика","Фильтрация, сглаживание, CAS и FSR",&App::GraphicsPage));
    c.pages.push_back({"О проекте",kLauncherPlatform,[](trg::Ui& ui){ui.Paragraph(kAboutText);},{}});
    return c;
  }
};
int main(int argc,char** argv){
#if defined(_WIN32)
  SetConsoleOutputCP(CP_UTF8);SetConsoleCP(CP_UTF8);
#else
  // SDL X11 uses XmbTextListToTextProperty for WM_NAME: the C locale corrupts UTF-8.
  std::setlocale(LC_CTYPE, "");
  if(std::string(nl_langinfo(CODESET))!="UTF-8")std::setlocale(LC_CTYPE,"C.UTF-8");
  signal(SIGPIPE,SIG_IGN);
#endif
  try{
    fs::path root=FindRoot();std::string profiles,renderer="opengl";int smoke_frames=0;bool autoplay=false;auto arguments=Utf8Arguments(argc,argv);
    for(size_t i=1;i<arguments.size();++i){const std::string& a=arguments[i];
      if(a=="--profiles-root" && i+1<arguments.size())profiles=arguments[++i];
      else if(a=="--smoke-frames" && i+1<arguments.size())smoke_frames=std::stoi(arguments[++i]);
      else if(a=="--renderer" && i+1<arguments.size())renderer=arguments[++i];
      else if(a=="--autoplay")autoplay=true;  // test automation: same path as the Play button, once
      else if(a=="--help"){std::cout<<"acv-launcher [--profiles-root PATH] [--renderer opengl|vulkan] [--smoke-frames N] [--autoplay]\n";return 0;}
      else throw std::runtime_error("Неизвестный аргумент: "+a);
    }
    if(renderer!="opengl" && renderer!="vulkan")throw std::runtime_error("--renderer must be opengl or vulkan");
#if !defined(ACV_LAUNCHER_HAS_VULKAN)
    if(renderer=="vulkan")throw std::runtime_error("This launcher build has no Vulkan SDK support");
#endif
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0)throw std::runtime_error(SDL_GetError());
    struct SdlCleanup{~SdlCleanup(){SDL_Quit();}} cleanup;
    const bool use_vulkan=renderer=="vulkan";
    if(!use_vulkan){SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);}
    Uint32 window_flags=SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(use_vulkan?SDL_WINDOW_VULKAN:SDL_WINDOW_OPENGL);
    SDL_Window* window=SDL_CreateWindow("Armored Core V — Лаунчер",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1120,780,window_flags);
    if(!window)throw std::runtime_error(SDL_GetError());
    struct WinCleanup{SDL_Window* p;~WinCleanup(){SDL_DestroyWindow(p);}} wc{window};
    struct GlCleanup{SDL_GLContext p=nullptr;~GlCleanup(){if(p)SDL_GL_DeleteContext(p);}} gc;
    if(!use_vulkan){gc.p=SDL_GL_CreateContext(window);if(!gc.p)throw std::runtime_error(SDL_GetError());SDL_GL_SetSwapInterval(1);}
    SDL_SetWindowMinimumSize(window,800,600);
    IMGUI_CHECKVERSION();ImGui::CreateContext();
    struct ImCleanup{~ImCleanup(){ImGui::DestroyContext();}} ic;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad;
#if defined(ACV_LAUNCHER_HAS_VULKAN)
    acv::ui::VulkanRenderer vulkan_renderer;
#endif
    App app(root,profiles);auto config=app.Config();
    auto font=root/"data/fonts/DejaVuSans.ttf",bold=root/"data/fonts/DejaVuSans-Bold.ttf";
    if(fs::is_regular_file(font) && fs::is_regular_file(bold)){
      auto font_path=font.u8string(),bold_path=bold.u8string();
      auto regular=io.Fonts->AddFontFromFileTTF(font_path.c_str(),18.0f,nullptr,io.Fonts->GetGlyphRangesCyrillic());
      auto strong=io.Fonts->AddFontFromFileTTF(bold_path.c_str(),18.0f,nullptr,io.Fonts->GetGlyphRangesCyrillic());
      if(!regular || !strong)throw std::runtime_error("Не удалось загрузить шрифты пакета");
      config.fonts={regular,strong,strong,18.0f};
    }else config.fonts=trg::LoadSystemFonts();
#if defined(ACV_LAUNCHER_HAS_VULKAN)
    if(use_vulkan)vulkan_renderer.Initialize(window);
    else
#endif
    {
      if(!ImGui_ImplSDL2_InitForOpenGL(window,gc.p))throw std::runtime_error("SDL ImGui initialization failed");
      if(!ImGui_ImplOpenGL3_Init("#version 330 core")){ImGui_ImplSDL2_Shutdown();throw std::runtime_error("OpenGL ImGui initialization failed");}
    }
    struct Backends{bool gl;~Backends(){if(gl){ImGui_ImplOpenGL3_Shutdown();ImGui_ImplSDL2_Shutdown();}}} backends{!use_vulkan};
    trg::Launcher launcher(std::move(config));bool quit=false;int frames=0;std::string shown;
    std::cout<<"launcher_ready root="<<root<<" renderer="<<renderer<<" SDL="<<SDL_MAJOR_VERSION<<'.'<<SDL_MINOR_VERSION<<'.'<<SDL_PATCHLEVEL<<" ImGui="<<IMGUI_VERSION<<std::endl;
    while(!quit){
      SDL_Event e;while(SDL_PollEvent(&e)){
        ImGui_ImplSDL2_ProcessEvent(&e);
        if(e.type==SDL_QUIT || (e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_CLOSE && e.window.windowID==SDL_GetWindowID(window)))quit=true;
        if(e.type==SDL_DROPFILE){launcher.DropFile(e.drop.file);launcher.GoToPage(0);SDL_free(e.drop.file);}
      }
      app.Tick();
      if(autoplay && app.ready && !app.Busy()){autoplay=false;app.Save(true);std::cout<<"autoplay_started"<<std::endl;}
      if(smoke_frames>0 && app.ready){
        launcher.GoToPage((frames/20)%6);
        if(frames==40)SDL_SetWindowSize(window,960,720);
        if(!launcher.config().fonts.regular || !launcher.config().fonts.regular->IsGlyphInFont(0x042F))throw std::runtime_error("Cyrillic font missing");
      }
      if(shown!=app.message){shown=app.message;launcher.SetStatus(shown,120.0);}
#if defined(ACV_LAUNCHER_HAS_VULKAN)
      if(use_vulkan){vulkan_renderer.NewFrame();}
      else{ImGui_ImplOpenGL3_NewFrame();}
#else
      ImGui_ImplOpenGL3_NewFrame();
#endif
      ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();
      auto result=launcher.Frame();if(result==trg::Result::kQuit)quit=true;
      if(result==trg::Result::kPlay)app.Save(true);
      ImGui::Render();if(smoke_frames>0 && ImGui::GetDrawData()->TotalVtxCount==0)throw std::runtime_error("Empty UI frame");
#if defined(ACV_LAUNCHER_HAS_VULKAN)
      if(use_vulkan){if(!vulkan_renderer.Render(ImGui::GetDrawData()))SDL_Delay(10);}
      else
#endif
      {int w=0,h=0;SDL_GL_GetDrawableSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.05f,.05f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());SDL_GL_SwapWindow(window);}
      if(smoke_frames>0 && ++frames>=smoke_frames)quit=true;
    }
    std::cout<<"launcher_exit game_pid="<<app.game.pid<<" backend_ready="<<app.ready<<std::endl;
    return smoke_frames>0 && !app.ready?2:0;
  }catch(const std::exception& e){std::cerr<<"launcher: "<<e.what()<<std::endl;return 1;}
}
