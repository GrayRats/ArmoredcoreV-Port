# Linux keyboard and mouse

The Linux probe now enables the installed ReXGlue SDK's synthetic player-one
controller by default. This is host input support, not confirmation of correct
ACV gameplay or camera behavior. A physical gamepad can still be used.

Launch from the project directory through the SQLite logger:

```bash
node tools/decomp-logging/cli.mjs run --stage linux_play --timeout-ms 130000 -- python3 tools/native-rexglue/probe_linux.py --seconds 110 --video-driver x11 --user-root tmp/acv-mnk-profile
```

The probe deliberately stops after the requested duration (maximum 300 seconds).
Use `--controls gamepad` to disable the synthetic controller. The optional
`--user-root` preserves setup/profile/cache between probes. Controls are stored
in `tools/native-rexglue/input-profile.json`; additional SDK cvars passed with
`--extra=--mnk_sensitivity=0.5` override the profile for one run.

| Input | Controller input |
|---|---|
| WASD | Left stick |
| Mouse, or I/J/K/L | Right stick |
| F8 | Enable/release mouse look; initially released |
| Escape | Release mouse look first; otherwise B |
| Left/right mouse | RT/LT |
| Q/E | LB/RB |
| Space / Backspace | A/B |
| R/F | X/Y |
| Enter / Tab | Start/Back |
| Arrow keys or 1/2/3/4 | D-pad up/down/left/right |
| V / middle mouse or C | Left/right stick press |

These are controller mappings, not inferred ACV action names. The SDK suppresses
mouse look when its overlay captures the mouse. Focus loss clears held inputs.

## Verification — 2026-10-21

- **Confirmed, host integration:** `tools/native-rexglue/check_input.cpp` runs
  against installed SDK 0.10.0 and passes 61 checks for event-to-XInput mappings,
  mouse deltas, release gating, focus loss, and gamepad-only mode.
  Evidence: `logs/acv-native-linux/input-probe.json`, SQLite operation
  `7b9b0e5b-6e18-49c6-be3b-d153d13e9306`.
- **Confirmed, actual game window:** F8 and Escape produce the expected
  enable/release messages in `logs/acv-native-linux/20261021T054533198224Z/`;
  checked by SQLite operation `54777d79-d5df-4a12-9785-128bdb841c12`.
- **Not verified:** gameplay camera response, sensitivity parity and complete
  controller action coverage in a playable scene. Frontend/setup and runtime
  blockers in [PLAN_B.md](PLAN.md) remain.
- This run also records repeated guest NULL writes on thread `0xF800152C`;
  no guest PC or causal connection to input has been established. Earlier probes
  observed the same message during shutdown. SDK source of the diagnostic:
  `src/system/xmemory.cpp:547` (host source location, not a retail address).

The launch wrapper now stops its own child after eight unhandled guest access
violations, with a two-second forced-exit fallback. It counts errors, retains
at most 32 sample lines and 128 distinct message keys plus overflow, and forwards
only the first eight repeats of each error into SQLite output. The SDK's own
runtime file remains untouched, including its existing rotation behavior.
A synthetic fault-loop check counted 2016 errors, retained the bounded sample,
verified child termination and preserved its raw input log:
SQLite operation `ff9c4f7d-e89d-4728-b52f-92a9e6863de4`.

## Open: real Start delivery

The 125-second launch `logs/acv-native-linux/20261021T055158059899Z/` and its four
RenderDoc captures remain on PRESS START after both window-directed and focused
XTest Enter attempts. This does not establish working game navigation. An opt-in
`--extra=--acv_input_trace=true` host hook logs received Start keys and deferred
player-one XInput state; investigate this route before claiming end-to-end
keyboard support. SDK driver tests and F8/Escape checks remain valid in their
stated scope. No UTF-8 exception or NULL-write loop was reproduced in this run;
known VFS/protection diagnostics still make its wrapper exit with error.

## SDL-to-menu validation — 2026-10-21, 15:23 UTC

The opt-in `--extra=--acv_probe_start_after=15` diagnostic emits one SDL
Return scancode through the SDK window event handler (not an OS keyboard event).
The actual InputSystem reports success and Start `0x0010`; the game leaves
PRESS START and renders its Xbox LIVE offline notice. This confirms the inner
SDL → XInput → guest-menu route, not desktop delivery or gameplay camera parity.
Evidence: `logs/acv-native-linux/20261021T152310773397Z/input-validation.json`
and `logs/acv-renderdoc/20261021T152310773397Z/acv_frame17834.png`.
Known VFS/protection diagnostics remain, so the overall launch is still marked
failed despite the input-specific result. Diagnostic injection is off by default.
`--extra=--cvar=VALUE` is now repeatable for multiple SDK options.

## ACV graphics compatibility defaults

The bounded probe now defaults to `--graphics-profile=acv-fsi`, which passes
`--protect_zero=false --render_target_path_vulkan=fsi`. These settings produced
visible gameplay HUD/effects in the recorded Linux experiment; dark geometry,
UTF-8 failures and full gameplay parity remain open. Use `--graphics-profile=sdk`
for SDK defaults during comparisons. Explicit `--extra=--cvar=value` options come
last and override the profile. Readback experiments are not defaults.
Evidence: `logs/acv-native-linux/fsi-utf8-backtrace/acv_frame46308.png`.

## Canonical launch configuration — 2026-10-22

User selected `tools/native-rexglue/acv.config` as the single launch configuration.
Current saved values: pilot_name=ARTEM, gamertag=ARTEM, headless=true.
probe_linux.py reads this file by default and passes profile names to the existing
XamShowKeyboardUI / XamUserGetName / XamUserGetGamerTag shims. A missing config
now fails explicitly instead of silently using SDK defaults. Preserve these user
settings in subsequent experiments. No new game launch or gameplay fix is claimed.
