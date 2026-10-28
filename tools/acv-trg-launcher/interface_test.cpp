// Exercises the real App state and Python boundary without a display or game.
#define main acv_launcher_entrypoint
#include "main.cpp"
#undef main
#include <thread>
#include <imgui_internal.h>
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void Finish(App& app){
  auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
  do{app.Tick();if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("UI operation timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));}while(app.Busy());
}
int main(int argc,char** argv){
  try{
    Check(argc==2,"project root argument required");
    char temp[]="/tmp/acv-interface-XXXXXX";auto path=mkdtemp(temp);Check(path,"temporary directory");
    fs::path profiles=path;struct Cleanup{fs::path p;~Cleanup(){fs::remove_all(p);}} cleanup{profiles};
    fs::path root=fs::canonical(argv[1]);
    auto created=App::Execute({"python3","-I",(root/"tools/native-rexglue/acv_profile_cli.py").string(),"--profiles-root",profiles.string(),"create","Профиль"},"");
    App app(root,profiles.string());Finish(app);Check(app.ready,"initial snapshot");
    Check(app.store.Get("ui.name")=="Профиль","profile form must be prefilled");
    app.store.Set("graphics.resolution","2560x1080");
    app.store.Set("launcher.allow_experimental","true");
    app.Submit(App::Job::Preview,app.Bridge("preview"),app.settings.dump());Finish(app);
    Check(app.Dirty(),"preview must retain unsaved changes");
    Check(!fs::exists(profiles/"settings.json"),"preview must not save");
    Check(app.snapshot.at("settings").at("launcher").at("allow_experimental")==true,"draft capabilities");
    app.Submit(App::Job::Snapshot,app.Bridge("snapshot"));Finish(app);
    Check(app.store.Get("graphics.resolution")=="2560x1080","refresh must preserve draft");
    Check(app.Dirty(),"refresh must preserve dirty indicator");
    Check(app.Save(false),"save submitted");Finish(app);Check(!app.Dirty(),"successful save clears dirty");
    Check(fs::exists(profiles/"settings.json"),"explicit save writes settings");
    app.last=Json{{"session","sentinel"}};
    app.Submit(App::Job::Backups,app.Cli({"backups",app.Selected()}));Finish(app);
    Check(app.backups.is_array(),"separate backup list");Check(app.last.at("session")=="sentinel","backup list must retain session");
    app.ephemeral["backup"]="sentinel-copy";
    app.Submit(App::Job::Snapshot,app.Bridge("snapshot"));Finish(app);
    Check(app.store.Get("ui.backup")=="sentinel-copy","refresh must retain selected backup");
    app.store.Set("launcher.video_driver","auto");app.store.ResetToDefaults();
    Check(app.store.Get("launcher.video_driver")=="auto","graphics reset retains launcher draft");
    Check(app.store.Get("graphics.resolution")=="auto","graphics reset restores defaults");
    Check(app.Dirty(),"graphics reset is unsaved");
    auto disk=App::Execute(app.Bridge("snapshot"),"");Check(disk.at("settings").at("graphics").at("resolution")=="2560x1080","reset must not write settings");
    // Real ScreenPage with advanced options and reported modes: the former duplicate-ID case.
    app.settings["launcher"]["build"]=(root.parent_path()/"ACV-Linux-release-1.0/bin/game/acv_linux_probe").string();
    app.settings["launcher"]["allow_experimental"]=true;
    app.settings["graphics"]["resolution"]="2560x1080";
    app.settings["graphics"]["wide_view"]="auto";
    app.Submit(App::Job::Preview,app.Bridge("preview"),app.settings.dump());Finish(app);
    app.snapshot["display_info"]={{"monitors",Json::array()}, {"display_modes",Json::array({{{"width",2560},{"height",1080}}})}};
    for(const auto* key:{"window_mode","monitor","render_scale","vsync","fps_limit","wide_view"})
      for(const auto& option:app.snapshot.at("capabilities").at(key).at("options"))
        Check(option.value("selectable",false),"fixture must keep every screen option selectable");
    auto* context=ImGui::CreateContext();
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize=ImVec2(2400,10000);io.DeltaTime=1.0f/60;
    auto* font=io.Fonts->AddFontDefault();io.Fonts->Build();
    auto config=app.Config();config.fonts={font,font,font,18};int resolution_items=0;
    config.pages[2].draw=[&](trg::Ui& ui){
      ImGuiID table=ImGui::GetID("##rows0");
      ImGuiID scope=ImHashStr("graphics.resolution",0,table);
      ImGuiID first=ImHashStr("##combo",0,scope);
      GImGui->HoveredIdPreviousFrame=first;GImGui->HoveredIdPreviousFrameItemCount=0;
      app.ScreenPage(ui);
      resolution_items=GImGui->HoveredIdPreviousFrameItemCount;
    };
    {
      trg::Launcher screen(std::move(config));screen.GoToPage(2);
      for(int i=0;i<3;++i){ImGui::NewFrame();screen.Frame();ImGui::Render();}
      std::cout<<"screen resolution controls sharing first ID="<<resolution_items<<std::endl;
      Check(resolution_items==1,"resolution pickers must not share an ID");
      Check(io.ConfigDebugHighlightIdConflicts,"ID conflict warning must remain enabled");
    }
    ImGui::DestroyContext(context);
    std::cout<<"interface: preview, dirty refresh, save, profile fields, backup/session isolation, draft reset passed"<<std::endl;return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
