# Launcher settings and MangoHud — 2026-10-30

The updated local package is `../../ACV-Linux-release-1.0/`, patch level
`2026-10-30-settings-review`. The previous answer omitted `DECOMP` from its
absolute path; the actual directory is `/home/art/DEV/DECOMP/ACV-Linux-release-1.0`.
Game/runtime/GPU/FFX and canonical config hashes match the package state before
this settings update. [Package update and protected hashes](logs/acv-settings-review-20261030/package-update.json).

## FPS source and actual check

MangoHud measures host Vulkan presents. SDK presentation can repeat a guest image
while the UI is repainting: [presenter.h:111](tools/rexglue-sdk/include/rex/ui/presenter.h).
The current SDK toast detaches when its queue empties
([achievement_toast.cpp:61](tools/rexglue-sdk/src/ui/overlay/achievement_toast.cpp));
otherwise active dialogs cause repeated UI paints
([imgui_drawer.cpp:439](tools/rexglue-sdk/src/ui/imgui_drawer.cpp)).
That runtime fix was already in the package before this update; this task does not replace its libraries.

The overlay now labels its native counter **Host FPS** and adds **Guest** from the
existing `[acv-display] pacing` records at the GPU command processor's XE_SWAP
boundary ([command_processor.cpp:1057](tools/rexglue-sdk/src/graphics/command_processor.cpp)).
Guest is the average of **300 intervals**, with mean milliseconds and sample age.
At 30 swaps/s the window is about 10 seconds; at 15 swaps/s it is about 20 seconds.
Before a sample it says waiting; stale data says no fresh sample. This is neither
unique-image FPS nor simulation/game-time speed. Host frametime/graph is disabled
in this integration to avoid presenting it as guest frametime.

MangoHud 0.8.4's `fps_text`, `read_cfg`, ordered `exec` and config precedence were
checked against [official overlay configuration code](https://github.com/flightlessmango/MangoHud/blob/v0.8.4/src/overlay_params.cpp)
and [official exec rendering](https://github.com/flightlessmango/MangoHud/blob/v0.8.4/src/hud_elements.cpp).
The exec row reads a per-session text file using a quoted environment variable;
the existing log follower updates it once per second and when a new batch arrives. No per-frame file
writes, new guest hook or polling of guest memory is added. User config files and
capture-related environment variables remain intact; ACV appends its counter/layout
options to the child environment.

A 112-second isolated startup run (`20261030T050831520363Z`) with cap 30, X11,
native/bilinear output and requested 2560×1080 completed with child 0, fatal 0.
The window manager constrained the window to **2560×1044**; this was not a display
mode or image-quality test. After 90 seconds: MangoHud CSV average **30.0 FPS**,
latest guest batch **30.000189 swaps/s**, mean **33.333123 ms**. MangoHud was mapped
in the owned game process and its debug log confirmed it consumed the Guest row.
The user's reported mission discrepancy **was not reproduced** in this startup scene.
No visual HUD, unique-image, gameplay or game-time acceptance follows from this.

[Runtime observations](logs/acv-settings-review-20261030/runtime-check.json) ·
[MangoHud CSV summary](logs/acv-settings-review-20261030/mango-csv/acv_linux_probe_2026-10-30_10-10-02_summary.csv) ·
[Launch report](logs/acv-native-linux/20261030T050831520363Z/launch.json).

## Presets

| Setting | Производительность | Графика | Плавность |
|---|---|---|---|
| Render scale / post-AA | 1× / off | 1× / off | 1× / off |
| Anisotropy | 2× | 2× | 4× |
| Output | Bilinear | FSR 1 | Bilinear |
| Dither, 8-bit | Off | On | On |
| Texture soft/hard MiB | 768 / 1536 | Adaptive; here 4096 / 4608 | 1536 / 2048 |
| Retention seconds | 120 | 800 | 300 |
| Archive read-ahead MiB | 16 | 64 | 32 |
| FPS limit / VSync | 60 / on | 60 / on | 60 / on |

Geometry, monitor, depth and wide-view choice are retained. Ten-bit requests force
dither off. Sharpness settings reset to SDK defaults. Quality enables the requested
experimental FSR 1 only if the selected runtime exposes FidelityFX; otherwise it
fails before changing the draft. Preset application is validated by the shared
backend and remains unsaved until Save or Play. CAS/FSR image effects were not tested.

On this machine NVIDIA reports 6144 MiB VRAM and the OS reports 48089 MiB RAM.
Quality reserves at least 1024 MiB and 25% VRAM for rendering/other use, also caps
the cache to one quarter of physical RAM, then rounds down to supported values.
The actual SDK caps are soft 4096/hard 8192 MiB
([cache.cpp:35](tools/rexglue-sdk/src/graphics/pipeline/texture/cache.cpp)).
The largest supported soft budget under the reserve is 4096; hard is 4608 here.
Multiple GPUs use the smallest reported capacity. If VRAM cannot be queried,
2048/4096 is an explicitly labeled fallback. These are budgets, not preallocation
or a guarantee that memory is currently free. Presets cannot guarantee better FPS.

## Settings/configuration audit

[Read-only audit](tools/native-rexglue/audit_launcher_settings.py) checked all
**19 settings / 137 allowed values**, six preset/depth combinations, emitted cvar
names against SDK declarations and selected runtime strings, numeric SDK bounds,
canonical `[sdk]` config keys and existing saved settings. Three YCbCr modes remain
explicitly unavailable. No user settings were rewritten by this audit.
[Full result](logs/acv-settings-review-20261030/audit.json).

Fixed two boundary errors: wide-view conversion emitted both true and false
`present_letterbox`; it now emits one value. Custom window sizes above the SDK's
8192-pixel bound now fail validation. Dithering has a persistent visible description
on the Color page, including its purpose and the 10-bit exclusion.

Checks passed: 21 settings tests, 6 streaming/preset tests, 6 MangoHud tests,
6 input-mode tests, 4 backend tests, actual App/ImGui interface regression,
Release launcher build and package hash/dependency verification (189 files).
The isolated package UI smoke and runtime checks do not establish full mission,
save/load, all adapter, physical 10-bit or output-effect correctness.
