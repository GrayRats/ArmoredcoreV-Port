// Host integration probe of the installed SDK's event -> XInput MnK driver.
// No claim about ACV camera sensitivity or gameplay parity.
#include <rex/cvar.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/ui/ui_event.h>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
  rex::cvar::Init(argc, argv);
  using namespace rex::input;
  using Key = rex::ui::VirtualKey;
  int checks = 0;
  auto check = [&](bool value, const char* label) {
    if (!value) { std::cerr << "FAILED: " << label << '\n'; std::exit(1); }
    ++checks;
  };
  mnk::MnkInputDriver driver(nullptr, 0);
  check(driver.Setup() == 0, "setup");
  std::vector<DeviceInfo> devices;
  driver.EnumerateDevices(devices);
  check(devices.size() == 1 && devices[0].synthetic, "synthetic controller enumerated");
  const auto device = devices[0].id;
  rex::ui::UISetupEvent focus(nullptr);
  driver.OnGotFocus(focus);
  auto state = [&] {
    X_INPUT_STATE out{};
    check(driver.GetDeviceState(device, &out) == 0, "poll connected controller");
    return out.gamepad;
  };
  auto key = [&](Key k, bool down) {
    rex::ui::KeyEvent event(nullptr, k, 1, false, false, false, false, false);
    if (down) driver.OnKeyDown(event); else driver.OnKeyUp(event);
  };
  for (auto [k, button] : {std::pair{Key::kSpace, X_INPUT_GAMEPAD_A},
                           {Key::kBack, X_INPUT_GAMEPAD_B},
                           {Key::kR, X_INPUT_GAMEPAD_X}, {Key::kF, X_INPUT_GAMEPAD_Y},
                           {Key::kQ, X_INPUT_GAMEPAD_LEFT_SHOULDER},
                           {Key::kE, X_INPUT_GAMEPAD_RIGHT_SHOULDER},
                           {Key::kReturn, X_INPUT_GAMEPAD_START},
                           {Key::kTab, X_INPUT_GAMEPAD_BACK},
                           {Key::kUp, X_INPUT_GAMEPAD_DPAD_UP}}) {
    key(k, true); check((state().buttons & button) != 0, "keyboard button pressed");
    key(k, false); check((state().buttons & button) == 0, "keyboard button released");
  }
  key(Key::kW, true); check(state().thumb_ly > 0, "W moves forward");
  key(Key::kW, false); check(state().thumb_ly == 0, "movement released");
  key(Key::kA, true); check(state().thumb_lx < 0, "A moves left");
  key(Key::kA, false);
  rex::ui::MouseEvent left(nullptr, rex::ui::MouseEvent::Button::kLeft, 0, 0);
  driver.OnMouseDown(left); check(state().right_trigger == 255, "left mouse is RT");
  driver.OnMouseUp(left); check(state().right_trigger == 0, "mouse trigger released");
  rex::ui::MouseEvent right(nullptr, rex::ui::MouseEvent::Button::kRight, 0, 0);
  driver.OnMouseDown(right); check(state().left_trigger == 255, "right mouse is LT");
  driver.OnMouseUp(right);
  mnk::SetMouseLookActive(false);
  rex::ui::MouseEvent move(nullptr, rex::ui::MouseEvent::Button::kNone, 12, 7);
  driver.OnMouseMove(move); auto idle = state();
  check(idle.thumb_rx == 0 && idle.thumb_ry == 0, "mouse release gate");
  mnk::SetMouseLookActive(true);
  rex::ui::MouseEvent move2(nullptr, rex::ui::MouseEvent::Button::kNone, 24, 14);
  driver.OnMouseMove(move2); auto look = state();
  check(look.thumb_rx > 0 && look.thumb_ry < 0, "mouse axes and orientation");
  auto drained = state(); check(drained.thumb_rx == 0 && drained.thumb_ry == 0, "mouse delta drained");
  key(Key::kW, true); driver.OnLostFocus(focus);
  auto lost = state(); check(lost.buttons == 0 && lost.thumb_ly == 0, "focus loss clears input");
  driver.OnGotFocus(focus); check(state().thumb_ly == 0, "no stuck key after focus return");
  rex::cvar::SetFlagByName("mnk_mode", "false");
  devices.clear(); driver.EnumerateDevices(devices);
  check(devices.empty(), "gamepad-only mode hides synthetic controller");
  std::cout << "{\"checks\":" << checks << ",\"status\":\"passed\",\"scope\":\"installed SDK event-to-XInput integration\"}\n";
}
