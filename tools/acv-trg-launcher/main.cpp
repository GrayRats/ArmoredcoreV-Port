#include "process.h"
#include "install.h"
#include <trg/launcher.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <future>
#include <fstream>
#include <iostream>
#include <map>
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Args=std::vector<std::string>;
static std::string Text(const Json& j){return j.is_string()?j.get<std::string>():j.dump();}
static fs::path FindRoot(){
  Args starts;
  if(auto env=getenv("ACV_ROOT"))starts.emplace_back(env);
  starts.push_back(fs::canonical("/proc/self/exe").parent_path().string());
  for(const auto& start:starts){
    auto p=fs::absolute(start);
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
  Json snapshot,settings,ephemeral=Json::object(),last;
  std::future<Json> work;
  enum class Job {None,Snapshot,Save,Play,Reset,Profile,Last};
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
          for(const auto& opt:cap["options"])if(Text(opt["value"])==value){settings[section][name]=opt["value"];return;}
          throw std::runtime_error("Некорректное значение настройки");
        }
      }
      if(section=="launcher" && name=="allow_experimental"){
        settings[section][name]=(value=="true");
      }else settings[section][name]=value;
    };
    store.save=[this](){return Save(false);};
    store.reset=[this](){if(!Busy())Submit(Job::Reset,Bridge("reset-graphics"));};
    Submit(Job::Snapshot,Bridge("snapshot"));
  }
  static std::pair<std::string,std::string> Split(std::string_view key){
    auto n=key.find('.');if(n==std::string_view::npos)throw std::runtime_error("Setting needs a section");
    return {std::string(key.substr(0,n)),std::string(key.substr(n+1))};
  }
  void Log(const std::string& event){
    if(!ready || !snapshot.contains("logs_root"))return;
    try{
      fs::path directory=snapshot.at("logs_root").get<std::string>();fs::create_directories(directory);
      std::ofstream file(directory/"acv-trg-launcher.log",std::ios::app);
      auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      file<<now<<" "<<event<<'\n';
    }catch(const std::exception& e){std::cerr<<"journal: "<<e.what()<<std::endl;}
  }
  bool Busy()const{return job!=Job::None;}
  bool Installing()const{return install.running();}
  Args Bridge(const std::string& command)const{
    Args a={"python3","-I",(root/"tools/acv-trg-launcher/backend.py").string()};
    if(!profiles.empty()){a.push_back("--profiles-root");a.push_back(profiles);}
    a.push_back(command);return a;
  }
  Args Cli(Args a)const{
    Args out={"python3","-I",(root/"tools/native-rexglue/acv_profile_cli.py").string(),"--profiles-root",snapshot.at("profiles_root").get<std::string>()};
    out.insert(out.end(),a.begin(),a.end());return out;
  }
  static Json Execute(const Args& args,const std::string& input){
    auto result=acv::Run(args,input,120000);
    Json parsed;
    try{parsed=Json::parse(result.out);}catch(...){throw std::runtime_error("Некорректный ответ backend: "+result.err+result.out);}
    if(result.code || parsed.contains("error"))throw std::runtime_error(parsed.is_object()?parsed.value("error",result.err):result.err);
    return parsed;
  }
  void Submit(Job kind,Args args,std::string input=""){
    if(Busy())return;
    if(kind==Job::Save || kind==Job::Play)Log("settings_requested "+input);
    job=kind;if(kind!=Job::Last)message="Выполняется…";
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
    if(game.pid)throw std::runtime_error("Игра уже запущена");
    if(snapshot.at("shm").at("free").get<uint64_t>()<6ULL*1024*1024*1024)
      throw std::runtime_error("Для запуска требуется не менее 6 ГБ свободного /dev/shm");
    Args args={"python3","-I",(root/"tools/native-rexglue/probe_linux.py").string(),"--session","--profile",Selected(),"--profiles-root",snapshot.at("profiles_root"),"--settings",snapshot.at("settings_path"),"--executable",settings.at("launcher").at("build")};
    if(settings["launcher"].value("allow_experimental",false))args.push_back("--allow-experimental");
    auto driver=settings["launcher"].value("video_driver","x11");
    if(driver!="auto"){args.push_back("--video-driver");args.push_back(driver);}
    fs::path logs=snapshot.at("logs_root").get<std::string>();fs::create_directories(logs);
    auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    session_log=(logs/("trg-session-"+std::to_string(stamp)+".log")).string();
    game.Start(args,session_log);game_start=std::chrono::steady_clock::now();last_poll=game_start;
    last=nullptr;message="Игра запущена";
    Log("game_started profile="+Selected()+" pid="+std::to_string(game.pid)+" log="+session_log);
    std::cout<<"game_started pid="<<game.pid<<" log="<<session_log<<std::endl;
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
        else{
          Json previous=settings;snapshot=std::move(result);settings=snapshot.at("settings");
          if(finished==Job::Profile){
            settings=std::move(previous);
            const auto& action=snapshot.at("action_result");
            if(action.is_object() && action.contains("uuid"))settings["launcher"]["profile"]=action["uuid"];
          }
          ready=true;message="Готово";
        }
        if(finished==Job::Play)StartGame();
      }catch(const std::exception& e){message=e.what();Log("backend_error "+message);std::cerr<<"backend: "<<message<<std::endl;}
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
    if(game.pid)return {false,"Игра уже запущена",0};
    if(!snapshot.value("build_exists",false))return {false,"Выберите исполняемый файл игры",0};
    if(!snapshot.value("game_data_exists",false))return {false,"Файлы игры отсутствуют или не соответствуют проверенному набору",0};
    if(Selected().empty())return {false,"Создайте или выберите профиль",1};
    return {};
  }
  void Choice(trg::Ui& ui,const char* key,const char* label,const char* help=""){
    const auto& cap=snapshot.at("capabilities").at(key);
    std::vector<trg::Option> options;std::string reasons;
    for(const auto& opt:cap["options"]){
      std::string v=Text(opt["value"]);
      if(opt.value("selectable",false))options.push_back({v,Label(v)});
      else if(opt.contains("reason"))reasons+=Label(v)+": "+opt.at("reason").get<std::string>()+"\n";
    }
    std::string id=std::string("graphics.")+key;
    ui.Combo(label,help,id.c_str(),Text(cap["default"]).c_str(),options);
    if(!reasons.empty() && ui.Section((std::string("Недоступные варианты##")+key).c_str()))ui.Help(reasons.c_str());
  }
  static std::string Label(const std::string& s){
    static const std::map<std::string,std::string> labels={{"true","Вкл."},{"false","Выкл."},{"windowed","Окно"},{"fullscreen","Полный экран"},{"off","Выкл."},{"game","Как в игре"},{"default","По умолчанию"},{"bilinear","Билинейная"},{"cas","CAS"},{"fsr","FSR 1"},{"fsr2","FSR 2"},{"fsr3","FSR 3"},{"rgb_full","RGB Full Range"}};
    auto i=labels.find(s);return i==labels.end()?s:i->second;
  }
  void LaunchPage(trg::Ui& ui){
    auto check=CanPlay();
    ui.StatusCard(game.pid?trg::Status::kBusy:(check.ok?trg::Status::kReady:trg::Status::kAttention),game.pid?"Игра работает":(check.ok?"Готово к запуску":"Требуется настройка"),game.pid?"Закрытие лаунчера оставит игру работающей":check.reason.c_str());
    if(!snapshot.value("game_data_exists",false)){
      ui.Paragraph(snapshot.at("installation").value("reason","").c_str());
      ui.InputText("Образ собственного диска","Поддерживается проверенная версия ACV; исходный ISO не меняется","ui.iso");
      if(!Installing() && ui.ButtonRow("Установка игры","Файлы будут проверены перед публикацией","Установить из ISO",true)){
        auto iso=store.Get("ui.iso");fs::path destination=snapshot.at("game_root").get<std::string>();auto command=Bridge("publish-install");
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
    ui.InputText("Исполняемый файл","Путь к выбранной Linux-сборке","launcher.build");
    ui.Choice("Оконная система","X11 проверен; Wayland зависит от сборки","launcher.video_driver","x11",{{"x11","X11"},{"wayland","Wayland"},{"auto","Авто"}});
    if(ui.Toggle("Экспериментальные настройки","Применять только доступные выбранной сборке варианты","launcher.allow_experimental",false,"Выкл.","Вкл.")){
      // Re-evaluate choices on an explicit action; no backend calls while drawing.
      Save(false);
    }
    if(ui.ButtonRow("Обновить состояние","Повторно прочитать настройки и возможности сборки","Обновить"))Submit(Job::Snapshot,Bridge("snapshot"));
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
    std::string selected=Selected();
    for(const auto& p:snapshot.at("profiles"))if(p.at("uuid")==selected){
      ui.Info("XUID",p.at("xuid").get_ref<const std::string&>().c_str());
      ui.Info("Позывной",p.at("gamertag").get_ref<const std::string&>().c_str());
      ui.Info("Пилот",p.at("pilot_name").get_ref<const std::string&>().c_str());
    }
    ui.InputText("Название","Новое имя профиля","ui.name");
    ui.InputText("Позывной","Не более 15 символов","ui.gamertag");
    ui.InputText("Пилот","Не более 32 символов","ui.pilot");
    auto name=store.Get("ui.name"),tag=store.Get("ui.gamertag"),pilot=store.Get("ui.pilot");
    if(ui.ButtonRow("Новый профиль","Создать отдельные сохранения","Создать")){
      Args a={"create",name};if(!tag.empty()){a.push_back("--gamertag");a.push_back(tag);}if(!pilot.empty()){a.push_back("--pilot");a.push_back(pilot);}Profile(a);
    }
    ImGui::BeginDisabled(selected.empty()||game.pid);
    if(ui.ButtonRow("Переименовать","Идентичность и сохранения не меняются","Применить имя"))Profile({"rename",selected,name});
    if(ui.ButtonRow("Имена в игре","Изменения применяются при следующем вводе имени в игре","Применить")){
      Args a={"set-names",selected};if(!tag.empty()){a.push_back("--gamertag");a.push_back(tag);}if(!pilot.empty()){a.push_back("--pilot");a.push_back(pilot);}Profile(a);
    }
    if(ui.ButtonRow("Резервная копия","Проверка целостности всех файлов сохранений","Создать копию"))Profile({"backup",selected});
    if(ui.ButtonRow("Список резервных копий","Проверить копии выбранного профиля","Показать копии"))Submit(Job::Last,Cli({"backups",selected}));
    if(last.is_array() && ui.Section("Резервные копии",true)){
      for(const auto& backup:last){
        std::string path=backup.at("path");
        ui.Info(backup.value("valid",false)?"Проверена":"Повреждена",path.c_str());
        if(backup.value("valid",false) && ui.ButtonRow(path.c_str(),"Выбрать для восстановления","Выбрать"))ephemeral["backup"]=path;
      }
    }
    ui.InputText("Каталог резервной копии","Для восстановления выбранного профиля","ui.backup");
    if(ui.ConfirmRow("Восстановление","Текущее состояние будет сохранено backend перед восстановлением","Восстановить","Восстановить выбранную резервную копию?","Восстановить","Отмена"))Profile({"restore",selected,store.Get("ui.backup")});
    ui.InputText("Импорт сохранений","Каталог данных для выбранного профиля","ui.import");
    if(ui.ButtonRow("Импорт","Копирование через проверяемый backend","Импортировать"))Profile({"import",selected,store.Get("ui.import")});
    ImGui::EndDisabled();
    ui.FolderRow("Данные профилей","Резервные копии и сохранения","Открыть",snapshot.at("profiles_root"));
  }
  void ScreenPage(trg::Ui& ui){
    Choice(ui,"window_mode","Режим экрана");Choice(ui,"monitor","Монитор","Индекс дисплея игрового backend");
    ui.Combo("Разрешение","UltraWide вписывает исходное изображение с сохранением пропорций","graphics.resolution","auto",{{"auto","Авто"},{"1280x720","1280×720"},{"1920x1080","1920×1080"},{"1920x1200","1920×1200"},{"2560x1080","2560×1080 (UltraWide)"},{"2560x1440","2560×1440"},{"2560x1600","2560×1600"},{"3440x1440","3440×1440 (UltraWide)"},{"3840x2160","3840×2160"}});
    Choice(ui,"render_scale","Масштаб рендера");Choice(ui,"vsync","VSync");Choice(ui,"fps_limit","Лимит FPS","Высокие частоты требуют проверки темпа игры");
    Choice(ui,"wide_view","Расширенный обзор (эксперимент)","Для вывода шире 16:9: расширяет обзор и центрирует HUD по выбранному разрешению. Нужны разрешение 2560×1080 или шире и разрешение экспериментальных настроек");
    ui.Help("Расширенный обзор UltraWide экспериментальный: отсечение по краям, прицел и ролики не проверены. «Авто» без явного разрешения недоступно.");
    if(last.is_object() && last.contains("monitors"))for(const auto& m:last["monitors"]){
      std::string label="Монитор "+std::to_string(m.value("index",0));
      std::string text=std::to_string(m.value("width",0))+"×"+std::to_string(m.value("height",0))+", "+Text(m.value("refresh_hz",Json()))+" Гц";
      ui.Info(label.c_str(),text.c_str());
    }
  }
  void ColorPage(trg::Ui& ui){Choice(ui,"color_encoding","Кодирование цвета");Choice(ui,"color_depth","Глубина цвета","10 бит — запрос, фактический формат зависит от surface игры");Choice(ui,"dither","Дизеринг");}
  void GraphicsPage(trg::Ui& ui){
    ui.Choice("Набор настроек","Применение меняет графику; Save сохраняет изменения","ui.preset","compatibility",{{"compatibility","Совместимость"},{"quality","Качество"}});
    if(ui.ButtonRow("Применить набор","Профиль и сохранения сохраняются","Применить")){
      auto name=store.Get("ui.preset","compatibility");
      for(auto i=snapshot.at("presets").at(name).begin();i!=snapshot.at("presets").at(name).end();++i)settings["graphics"][i.key()]=i.value();
    }
    Choice(ui,"anisotropy","Анизотропная фильтрация");Choice(ui,"antialiasing","Сглаживание");Choice(ui,"output_effect","Масштабирование","CAS / FSR 1 требуют FidelityFX в выбранном runtime");Choice(ui,"sharpness","Резкость CAS");Choice(ui,"fsr_sharpness","Резкость FSR");
    if(ui.ConfirmRow("Сброс графики","Профили и сохранения сохраняются","Сбросить","Сбросить настройки графики?","Сбросить","Отмена"))store.ResetToDefaults();
  }
  trg::LauncherConfig Config(){
    trg::LauncherConfig c;c.branding.title="ARMORED CORE V";c.branding.subtitle="LINUX · ЛАУНЧЕР";c.theme=trg::Theme::Ember();c.settings=&store;
    c.on_file_drop=[this](const std::string& path){ephemeral["iso"]=path;};
    c.save_on_play=false;c.can_play=[this]{return CanPlay();};
    c.hints="Enter  Запуск      Esc  Выход      Ctrl+S  Сохранить";c.note="Настройки проверяются и сохраняются перед запуском.";
    auto page=[this](const char* name,const char* blurb,auto draw){return trg::Page{name,blurb,[this,draw](trg::Ui& ui){
      if(!ready){ui.Paragraph(message.c_str());return;}
      ImGui::BeginDisabled(Busy());(this->*draw)(ui);ImGui::EndDisabled();
      ui.Spacer();ui.Help(message.c_str());
    },{}};};
    c.pages.push_back(page("Запуск","Профиль, сборка и состояние игры",&App::LaunchPage));
    c.pages.push_back(page("Профиль и сохранения","Отдельные данные и резервные копии",&App::ProfilesPage));
    c.pages.push_back(page("Экран","Разрешение, пропорции и частота кадров",&App::ScreenPage));
    c.pages.push_back(page("Цвет","Формат вывода на экран",&App::ColorPage));
    c.pages.push_back(page("Графика","Фильтрация, сглаживание, CAS и FSR",&App::GraphicsPage));
    c.pages.push_back({"О проекте","Native Linux static recompilation",[](trg::Ui& ui){ui.Paragraph("ARMORED CORE V · экспериментальная Linux-сборка.\nTRG Launcher / SDL2 / Dear ImGui.\nИсходные игровые данные предоставляются владельцем игры.");},{}});
    return c;
  }
};
int main(int argc,char** argv){
  signal(SIGPIPE,SIG_IGN);
  try{
    fs::path root=FindRoot();std::string profiles;int smoke_frames=0;
    for(int i=1;i<argc;++i){std::string a=argv[i];
      if(a=="--profiles-root" && i+1<argc)profiles=argv[++i];
      else if(a=="--smoke-frames" && i+1<argc)smoke_frames=std::stoi(argv[++i]);
      else if(a=="--help"){std::cout<<"acv-launcher [--profiles-root PATH] [--smoke-frames N]\n";return 0;}
      else throw std::runtime_error("Неизвестный аргумент: "+a);
    }
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0)throw std::runtime_error(SDL_GetError());
    struct SdlCleanup{~SdlCleanup(){SDL_Quit();}} cleanup;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_Window* window=SDL_CreateWindow("Armored Core V — Лаунчер",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1120,780,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!window)throw std::runtime_error(SDL_GetError());
    struct WinCleanup{SDL_Window* p;~WinCleanup(){SDL_DestroyWindow(p);}} wc{window};
    auto context=SDL_GL_CreateContext(window);if(!context)throw std::runtime_error(SDL_GetError());
    struct GlCleanup{SDL_GLContext p;~GlCleanup(){SDL_GL_DeleteContext(p);}} gc{context};
    SDL_GL_SetSwapInterval(1);SDL_SetWindowMinimumSize(window,800,600);
    IMGUI_CHECKVERSION();ImGui::CreateContext();
    struct ImCleanup{~ImCleanup(){ImGui::DestroyContext();}} ic;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad;
    App app(root,profiles);auto config=app.Config();
    auto font=root/"data/fonts/DejaVuSans.ttf",bold=root/"data/fonts/DejaVuSans-Bold.ttf";
    if(fs::is_regular_file(font) && fs::is_regular_file(bold)){
      auto regular=io.Fonts->AddFontFromFileTTF(font.c_str(),18.0f);
      auto strong=io.Fonts->AddFontFromFileTTF(bold.c_str(),18.0f);
      if(!regular || !strong)throw std::runtime_error("Не удалось загрузить шрифты пакета");
      config.fonts={regular,strong,strong,18.0f};
    }else config.fonts=trg::LoadSystemFonts();
    if(!ImGui_ImplSDL2_InitForOpenGL(window,context))throw std::runtime_error("SDL ImGui initialization failed");
    if(!ImGui_ImplOpenGL3_Init("#version 330 core")){ImGui_ImplSDL2_Shutdown();throw std::runtime_error("OpenGL ImGui initialization failed");}
    struct Backends{~Backends(){ImGui_ImplOpenGL3_Shutdown();ImGui_ImplSDL2_Shutdown();}} backends;
    trg::Launcher launcher(std::move(config));bool quit=false;int frames=0;std::string shown;
    std::cout<<"launcher_ready root="<<root<<" SDL="<<SDL_MAJOR_VERSION<<'.'<<SDL_MINOR_VERSION<<'.'<<SDL_PATCHLEVEL<<" ImGui="<<IMGUI_VERSION<<std::endl;
    while(!quit){
      SDL_Event e;while(SDL_PollEvent(&e)){
        ImGui_ImplSDL2_ProcessEvent(&e);
        if(e.type==SDL_QUIT || (e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_CLOSE && e.window.windowID==SDL_GetWindowID(window)))quit=true;
        if(e.type==SDL_DROPFILE){launcher.DropFile(e.drop.file);launcher.GoToPage(0);SDL_free(e.drop.file);}
      }
      app.Tick();
      if(smoke_frames>0 && app.ready){
        launcher.GoToPage((frames/20)%6);
        if(frames==40)SDL_SetWindowSize(window,960,720);
        if(!launcher.config().fonts.regular || !launcher.config().fonts.regular->IsGlyphInFont(0x042F))throw std::runtime_error("Cyrillic font missing");
      }
      if(shown!=app.message){shown=app.message;launcher.SetStatus(shown,120.0);}
      ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();
      auto result=launcher.Frame();if(result==trg::Result::kQuit)quit=true;
      if(result==trg::Result::kPlay)app.Save(true);
      ImGui::Render();if(smoke_frames>0 && ImGui::GetDrawData()->TotalVtxCount==0)throw std::runtime_error("Empty UI frame");int w=0,h=0;SDL_GL_GetDrawableSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.05f,.05f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());SDL_GL_SwapWindow(window);
      if(smoke_frames>0 && ++frames>=smoke_frames)quit=true;
    }
    std::cout<<"launcher_exit game_pid="<<app.game.pid<<" backend_ready="<<app.ready<<std::endl;
    return smoke_frames>0 && !app.ready?2:0;
  }catch(const std::exception& e){std::cerr<<"launcher: "<<e.what()<<std::endl;return 1;}
}
