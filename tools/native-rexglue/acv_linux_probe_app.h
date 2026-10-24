// acv_linux_probe - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include "acv_build_info.h"
#include <rex/rex_app.h>
#include <rex/filesystem/vfs.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/runtime.h>
#include <cstdlib>
#include <filesystem>
#include <rex/system/function_dispatcher.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/ui/keybinds.h>
#include <rex/input/input_system.h>
#include <rex/ui/windowed_app_context.h>
#include <rex/ui/window_sdl.h>
#include <chrono>
#include <thread>
#include <charconv>
#include <sstream>
#include <vector>

REXCVAR_DEFINE_BOOL(acv_input_trace, false, "ACV", "Trace host Start key delivery and player-one XInput state");
REXCVAR_DEFINE_UINT32(acv_probe_start_after, 0, "ACV", "Diagnostic only: inject one SDL Start press after 1..60 seconds; zero disables");
REXCVAR_DEFINE_STRING(acv_probe_keys, "", "ACV", "Diagnostic only: timed SDL scancodes, e.g. 15:Return,25:Space; disabled when empty");
void AcvStopTextureWriterProbe();
void AcvRegisterRetailLeaves(rex::runtime::FunctionDispatcher& dispatcher);

class AcvLinuxProbeApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<AcvLinuxProbeApp>(new AcvLinuxProbeApp(ctx, "acv_linux_probe",
        PPCImageConfig));
  }

  void OnPostLoadXexImage() override {
    AcvRegisterRetailLeaves(*runtime()->function_dispatcher());
  }

  void OnPostSetup() override {
    // dlc_sound: serves the game's own sound banks, stored in dvdbnd under path hashes. The directory is
    // built by tools/build_dlc_sound_dir.py and passed by the probe (ACV_DLC_SOUND_ROOT). Read-only mount.
    if (const char* sound_root = std::getenv("ACV_DLC_SOUND_ROOT"); sound_root && *sound_root) {
      const std::filesystem::path root_path(sound_root);
      auto* vfs = runtime()->file_system();
      if (vfs && std::filesystem::is_directory(root_path)) {
        // Must not start with "\\Device\\Harddisk0": the SDK NullDevice owns that prefix and is matched first.
        const char* mount = "\\Device\\AcvDlcSound";
        auto device = std::make_unique<rex::filesystem::HostPathDevice>(mount, root_path, true);
        if (device->Initialize() && vfs->RegisterDevice(std::move(device)) &&
            vfs->RegisterSymbolicLink("dlc_sound:", mount)) {
          REXLOG_INFO("[acv-vfs] dlc_sound: mounted from {}", root_path.string());
        } else {
          REXLOG_ERROR("[acv-vfs] dlc_sound: mount failed for {}", root_path.string());
        }
      } else {
        REXLOG_ERROR("[acv-vfs] ACV_DLC_SOUND_ROOT is not a directory: {}", root_path.string());
      }
    }
    // Serve verified retail emblem TPFs extracted by build_dlc_image_dir.py.
    if (const char* image_root = std::getenv("ACV_DLC_IMAGE_ROOT"); image_root && *image_root) {
      const std::filesystem::path root_path(image_root);
      auto* vfs = runtime()->file_system();
      if (vfs && std::filesystem::is_directory(root_path)) {
        const char* mount = "\\Device\\AcvDlcImage";
        auto device = std::make_unique<rex::filesystem::HostPathDevice>(mount, root_path, true);
        if (device->Initialize() && vfs->RegisterDevice(std::move(device)) &&
            vfs->RegisterSymbolicLink("dlc_image:", mount)) {
          REXLOG_INFO("[acv-vfs] dlc_image: mounted verified retail assets from {}", root_path.string());
        } else {
          REXLOG_ERROR("[acv-vfs] dlc_image: mount failed for {}", root_path.string());
        }
      } else {
        REXLOG_ERROR("[acv-vfs] ACV_DLC_IMAGE_ROOT is not a directory: {}", root_path.string());
      }
    }
    // system: is an optional startup-properties device (system:\system.properties, loader 0x82C22B90). The file
    // does not exist in the retail image and the guest skips parsing when it is missing, so this mounts an EMPTY
    // read-only directory: the request still fails with "no such file" (same guest-visible result as before),
    // but the SDK no longer logs "ResolvePath(system:\) failed - device not found". No content is invented.
    if (const char* system_root = std::getenv("ACV_SYSTEM_ROOT"); system_root && *system_root) {
      const std::filesystem::path root_path(system_root);
      auto* vfs = runtime()->file_system();
      if (vfs && std::filesystem::is_directory(root_path)) {
        const char* mount = "\\Device\\AcvSystem";
        auto device = std::make_unique<rex::filesystem::HostPathDevice>(mount, root_path, true);
        if (device->Initialize() && vfs->RegisterDevice(std::move(device)) &&
            vfs->RegisterSymbolicLink("system:", mount)) {
          REXLOG_INFO("[acv-vfs] system: mounted as an empty read-only device from {}", root_path.string());
        } else {
          REXLOG_ERROR("[acv-vfs] system: mount failed for {}", root_path.string());
        }
      }
    }
    REXLOG_INFO("[acv-build] Host source revision {}", ACV_HOST_REVISION);
    // Menus and text dialogs retain the cursor until the user opts into look.
    rex::input::mnk::SetMouseLookActive(false);
    REXLOG_INFO("[acv-input] Keyboard/mouse profile: F8 toggles mouse look; Escape releases it");
    struct ProbeKey { unsigned seconds; SDL_Scancode scancode; };
    std::vector<ProbeKey> keys;
    const auto seconds = REXCVAR_GET(acv_probe_start_after);
    if (seconds > 0 && seconds <= 60) keys.push_back({seconds, SDL_SCANCODE_RETURN});
    const auto sequence = REXCVAR_GET(acv_probe_keys);
    if (!sequence.empty()) {
      keys.clear();
      std::istringstream input(sequence);
      std::string token;
      while (std::getline(input, token, ',')) {
        const auto colon = token.find(':');
        unsigned at = 0;
        const auto parsed = std::from_chars(token.data(), token.data() +
            (colon == std::string::npos ? token.size() : colon), at);
        const auto code = colon == std::string::npos ? SDL_SCANCODE_UNKNOWN :
            SDL_GetScancodeFromName(token.c_str() + colon + 1);
        if (colon == std::string::npos || parsed.ec != std::errc{} ||
            parsed.ptr != token.data() + colon || at < 1 || at > 300 ||
            (!keys.empty() && at <= keys.back().seconds) || code == SDL_SCANCODE_UNKNOWN) {
          REXLOG_WARN("[acv-input] Invalid diagnostic sequence; no keys scheduled");
          keys.clear();
          break;
        }
        keys.push_back({at, code});
      }
    }
    if (!keys.empty()) {
      input_probe_ = std::jthread([this, keys = std::move(keys)](std::stop_token stop) {
        const auto started = std::chrono::steady_clock::now();
        auto wait_until = [&stop](auto target) {
          while (std::chrono::steady_clock::now() < target) {
            if (stop.stop_requested()) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
          }
          return !stop.stop_requested();
        };
        for (const auto& key : keys) {
          if (!wait_until(started + std::chrono::seconds(key.seconds))) return;
          app_context().CallInUIThreadDeferred([this, key] { InjectDiagnosticKey(key.scancode, true); });
          if (!wait_until(started + std::chrono::seconds(key.seconds) + std::chrono::milliseconds(500))) return;
          app_context().CallInUIThreadDeferred([this, key] { InjectDiagnosticKey(key.scancode, false); });
        }
      });
    }
  }

  void OnKeyDown(rex::ui::KeyEvent& event) override {
    if (REXCVAR_GET(acv_input_trace) &&
        event.virtual_key() == rex::ui::VirtualKey::kReturn && !event.prev_state()) {
      REXLOG_INFO("[acv-input] Host Start key received");
      app_context().CallInUIThreadDeferred([this] {
        rex::input::X_INPUT_STATE state{};
        auto* input = static_cast<rex::input::InputSystem*>(runtime()->input_system());
        const auto result = input->GetState(0, &state);
        REXLOG_INFO("[acv-input] Deferred user0 status={:08X} buttons={:04X}",
                    result, static_cast<uint16_t>(state.gamepad.buttons));
      });
    }
    if (event.virtual_key() == rex::ui::VirtualKey::kF8) {
      if (!event.prev_state()) {
        const bool enabled = !rex::input::mnk::IsMouseLookActive();
        rex::input::mnk::SetMouseLookActive(enabled);
        REXLOG_INFO("[acv-input] Mouse look {}", enabled ? "enabled" : "released");
      }
      event.set_handled(true);
      return;
    }
    if (event.virtual_key() == rex::ui::VirtualKey::kEscape &&
        rex::input::mnk::IsMouseLookActive()) {
      rex::input::mnk::SetMouseLookActive(false);
      REXLOG_INFO("[acv-input] Mouse look released by Escape");
      event.set_handled(true);
      return;
    }
    // Same SDK overlay processing as ReXApp::OnKeyDown (private base method).
    rex::ui::ProcessKeyEvent(event);
  }

  void OnShutdown() override {
    AcvStopTextureWriterProbe();
    input_probe_.request_stop();
    if (input_probe_.joinable()) input_probe_.join();
  }

 private:
  // Host diagnostic only. Uses the SDK SDL scancode/event path, leaving retail
  // code and controller state untouched. It does not verify OS key delivery.
  void InjectDiagnosticKey(SDL_Scancode scancode, bool pressed) {
    auto* target = dynamic_cast<rex::ui::WindowSDL*>(window());
    if (!target) return;
    SDL_Event event{};
    event.type = pressed ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = scancode;
    event.key.down = pressed;
    REXLOG_INFO("[acv-input] Diagnostic SDL scancode {} {}", static_cast<int>(scancode), pressed ? "down" : "up");
    target->HandleKeyEvent(event);
  }
  std::jthread input_probe_;
};
