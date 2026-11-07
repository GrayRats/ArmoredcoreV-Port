# Windows Vulkan support plan — ACV launcher and game

## Status and scope

The Windows port is now requested. This document is the implementation queue
for Windows Vulkan support in the ACV launcher and game, with Linux and macOS
regression coverage. The current host is Linux x86_64 with MinGW-w64, Clang/Clang-CL, Wine and
LunarG Vulkan SDK 1.4.363.1. Its headers report VK_HEADER_VERSION 363. The
Windows SDK was installed in copy-only mode under tmp/; the Linux SDK is under
tmp/lunarg-vulkan/linux-sdk/1.4.363.1/x86_64. Neither was installed
system-wide. The launcher requests Vulkan API 1.4 and rejects older loaders or
devices. The Windows loader/import-library filenames remain vulkan-1.dll and
vulkan-1.lib; those stable ABI names do not mean the application requests
Vulkan 1.0. This host has no native Windows desktop or GPU; macOS cannot be
built or run here, so native driver, WSI and gameplay acceptance need matching
OS/GPU machines.

The SDK has a Windows x64 CMake preset and Win32 Vulkan surface code, but Vulkan
is disabled by default on Windows. The ACV launcher now has optional SDL2/ImGui
Vulkan and OpenGL renderers. A Win32 process/path/settings subset and Windows
launcher `.exe` cross-build are in place. The ACV game application remains
blocked by unported compatibility shims, Linux-only session/input/package code,
and the missing Windows game runtime bridge, as tracked in
[Install.MD](Install.MD#5-windows-x64-sdk-build-and-application-port-status).
An SDK DLL build alone does not produce a working ACV Windows package.

### Implementation checkpoint — 2026-11-06

- ACVLooseLoader (MIT, commit 24fa8bcb) собран для `win-x64` (framework-dependent, .NET 8).
  Не запускался на Windows. В `PathHelper.CleanPath` и `ConsoleModeManager.LockConsole` добавлены
  ветки `OperatingSystem.IsWindows()`; Windows-ветки не проверены. Linux-правки:
  `tools/acv-loose-loader-linux.patch`.
- Текст переключателя MangoHud в `tools/acv-trg-launcher/main.cpp` изменён: «Host FPS» убран
  (счётчик отключён, см. AGENTS.md). Windows-launcher `.exe` после этого не пересобирался;
  результат из checkpoint 2026-11-05 относится к старому тексту.
- `tools/acv-context` (stdlib, SQLite FTS5) и `tools/llmignore.py` (вызывает `git`) на Windows
  не проверялись. Хуки и `.mcp.json` вызывают `python3`, которого на Windows может не быть
  (`py`/`python`); `git` должен быть в PATH; путь индекса по умолчанию — `CLAUDE_PLUGIN_DATA` или `tmp/`.
- Среда разработки на Linux: NVIDIA GeForce GTX 1660 SUPER. Матрица драйверов Windows
  (NVIDIA, AMD, Intel) не заполнена.
- Статус нативного Windows не изменился: нет проверки UI, WSI, игрового runtime, геймплея и пакета.

### Implementation checkpoint — 2026-11-05

ReXGlue's game Vulkan backend now targets `VK_API_VERSION_1_4`, checks the
`vkEnumerateInstanceVersion` result, rejects loaders below 1.4 and skips
physical devices below 1.4. The Linux Release `rexui` target and the Windows
MinGW-targeted Clang `rexui` target both build successfully. The full Windows
SDK build remains blocked before game linking by the vendored FFmpeg MSVC CRT
configuration (`crtversion.h` is unavailable to this Linux cross toolchain).
No Windows game executable/runtime package was produced, and this code change
has not had a Wine or native gameplay run.

Follow-up on the same date: the `tmp/rexglue-win-vulkan-clang` configuration
(`clang++ --target=x86_64-w64-mingw32`) now builds to completion (`ninja -k 0`,
exit 0). The remaining source fixes were: `WinSock2.h`/`WS2tcpip.h` include
casing in `src/system/xsocket.cpp`, and explicit `reinterpret_cast<void*>` for
`FARPROC` assignments in `src/input/xinput/xinput_input_driver.cpp`. The earlier
`threading_win.cpp`, `socket_win.cpp` and `fiber_win32.cpp` errors no longer
reproduce. Produced under `tools/rexglue-sdk/out/win-amd64/`: `rexglue.exe`
(PE32+, console, SHA-256
018b85bf4867615ae9b2709196a92da6c3b6e0dd6564a6aae1b1497a00cc1645),
`librexgpu-xenos.dll` and `librexruntime.dll`. This is a compile/link result
only; the Wine smoke and native gameplay runs are still open. The Windows game
executable/runtime package is still not produced.

The ReXGlue source tree remains pinned to Vulkan-Headers/SDK 1.4.357.0; these
headers expose Vulkan API 1.4. The local LunarG 1.4.363.1 SDK/runtime remains
the launcher link/runtime source. Windows `vulkan-1.dll` and `vulkan-1.lib`
remain the loader ABI filenames while the application requests API 1.4.

SQLite operation IDs: Linux `rexui` Release build
`64e3c1f5-6f21-45ec-a0be-26bf6b8bb773`; focused Windows Vulkan object build
`86dba48a-e0ec-4b61-b416-bb6642024abd`; Windows `rexui` build
`e868e3c0-f481-422c-a7fc-9a8e42d45108`.

### Implementation checkpoint — 2026-11-04

| Boundary | Implemented and checked | Still open |
|---|---|---|
| ACV launcher UI | Optional SDL2/ImGui Vulkan renderer with OpenGL fallback. Windows Vulkan .exe fully cross-linked against LunarG SDK 1.4.363.1. Linux Vulkan and OpenGL each passed an 80-frame smoke including resize; launcher CTest passed 3/3. | Wine with app-local LunarG Runtime Components loaded LunarG loader 1.4.363, but that loader could not load Wine's ICD bridge. A separate Wine run using Wine's built-in loader completed the 80-frame Vulkan renderer loop (renderer=vulkan); it ended backend_ready=0 because the Windows Python backend is absent. Native Windows WSI/presentation and gameplay remain untested. |
| Launcher process and paths | Added Win32 process supervision, UTF-8 argument/path conversion, Windows staging-directory creation, Windows-local Python settings paths and byte locking, and explicit unsupported Windows game-launch status. Windows launcher C++ target and 39 Python backend tests pass on Linux. | Run the process/profile/install boundary on native Windows. The launcher deliberately blocks Play because the game runtime is not ported. |
| ReXGlue Windows Vulkan | Windows Vulkan CMake configure passed; selected Vulkan renderer, command processor, render-target, pipeline-cache and Win32 WSI translation units compile with MinGW. Persistent pipeline cache has a Win32 file-lock/atomic-replace implementation; POSIX branch remains intact. | Full SDK build stopped in vendored FFmpeg's MSVC CRT configuration (crtversion.h missing in this Linux cross environment). No Windows game runtime/DLL set or native game build was produced. |
| Linux maintained path | Built against LunarG SDK 1.4.363.1 headers and loader; CTest 3/3, Vulkan and OpenGL 80-frame smokes passed. SDK vulkaninfo reported loader 1.4.363 and NVIDIA device API 1.4.351. | Re-run package/gameplay acceptance only when the Linux runtime/package is changed; this work did not alter or promote a game release. |
| macOS | SDL renderer includes the portability-enumeration and portability-subset path needed by a MoltenVK-capable device. | No macOS compiler, SDK, MoltenVK runtime or native presentation test on this host. |

Windows Vulkan launcher artifact: tmp/acv-trg-win-vulkan-build/acv-launcher.exe,
PE32+ x86-64, SHA-256
37daccbe972454a65b135026747b28dc3e659a52702eb3979aa92e972a40e19b.
The import table references the stable vulkan-1.dll loader ABI. It was linked
against LunarG SDK 1.4.363.1 vulkan-1.lib. Windows SDK installer SHA-256
c02932bd9a968b22d404536108adaacac908c081cd6b0113f5bfefc523dd0a6a;
Linux SDK archive SHA-256
b7867d8ca658563813af666f34b93db87f1b1c7d9d69d97e924c210c7ba33eb8.
LunarG Runtime Components 1.4.363.0 archive SHA-256
a25a927aa8b9f0371048f1861cf88ac3b9bc9b1fb332c42d897c8ab32695769a;
extracted vulkan-1.dll SHA-256
e1fcfc9489beefa6a6d9d11c6d7517f6a2b7f16e0d3fb8f4103bee0210f314a3.

SQLite operation IDs: Windows LunarG Vulkan configure
227409c5-b770-4550-9bff-7e36e414f16f, Windows Vulkan link
a7eba798-5595-4ba6-8feb-f4c1f04375a5, Wine using LunarG loader
200ffe2f-6a93-4c1d-a139-a7c2871d5220 (loader 1.4.363 identified; Wine ICD bridge load failed), Wine built-in loader smoke
9a4b09d7-a9d2-4d3e-9e9f-960725b652a8, Linux SDK launcher configure
064c7940-cacb-4c64-9ec1-994cd5e8c7f4, build
90976c61-666f-410e-926e-688a7a8d53ba, CTest
1fca97d5-fe9a-40cd-a4e4-1609fbdaa355, Vulkan smoke
824b69f4-e39a-4d96-9749-e00255decd32, OpenGL smoke
708104ad-f791-4d83-8871-f4f7d210237f.

These results establish source and cross-compile progress, not Windows or macOS
product support. No cross-OS FPS comparison has been collected; performance
improvements must be measured on matched scenes and actual drivers per OS. The
Linux launcher smoke checks establish rendering/resize/shutdown only; this host
has no `perf`, Valgrind/Callgrind or Hotspot profiler and the project has no
committed launcher frame-time budget, so CPU/GPU bottlenecks are not assessed.

## Goals

- Build the ReXGlue game runtime with the Vulkan backend for Windows x64.
- Give the ACV launcher a Vulkan 1.4 UI renderer while retaining a tested
  OpenGL fallback for machines without a usable Vulkan 1.4 driver.

## Source-backed starting point

| Area | Current source state | Required work |
|---|---|---|
| ReXGlue graphics options | [CMakeLists.txt](tools/rexglue-sdk/CMakeLists.txt#L35) defaults Windows to D3D12 on and Vulkan off. Windows Vulkan configure passed in a separate cross-build tree. | Complete an SDK build with the real Windows SDK/MSVC CRT and package dependencies; then validate Vulkan-only and, if needed, dual-backend builds. |
| Win32 Vulkan WSI | `api.h` defines `VK_USE_PLATFORM_WIN32_KHR`; [vulkan_instance.cpp](tools/rexglue-sdk/src/ui/vulkan/vulkan_instance.cpp#L431) loads Win32 surface functions and [vulkan_presenter.cpp](tools/rexglue-sdk/src/ui/vulkan/vulkan_presenter.cpp#L916) creates `VkWin32SurfaceKHR`. Selected Win32 WSI units compile for MinGW. | Validate `HINSTANCE`/`HWND`, present-queue selection, swapchain resize/out-of-date handling, teardown and device loss on real Windows drivers. |
| ACV launcher renderer | main.cpp offers optional SDL2/ImGui Vulkan and OpenGL modes. The renderer requests API 1.4 and rejects older loaders/devices. Linux modes pass smoke; the Windows Vulkan target links against LunarG SDK 1.4.363.1. | Validate native Windows window/surface creation, presentation, resize, fallback and shutdown. Wine is limited to renderer startup/swap smoke and does not establish native WSI or gameplay. |
| ACV process launch | [process.h](tools/acv-trg-launcher/process.h) selects POSIX or Win32 supervision; Windows uses `CreateProcessW`, pipes, job ownership, timeout and UTF-8 arguments. Windows launcher cross-build passed. | Native Windows tests for quoting, cancellation, exit code, pipe limits, logging, and executable-relative paths. |
| Settings and profiles | `acv_paths.py` uses LocalAppData on Windows; `acv_profiles.py` has `msvcrt` locking and atomic backup renames; Python portability suite passes on Linux. | Validate Windows byte locks and crash-safe profile/install paths on NTFS. Session/input tooling still contains Linux-only `/proc` paths. |
| Game compatibility shims | `acv_compat_shims.cpp` currently includes POSIX dynamic loading, file and process headers | Implement and review Windows equivalents without changing guest ABI or hiding invalid guest calls |
| Package | Linux packaging stages ELF/`.so`, `ldd` and `start.sh` | Add a separate Windows `.exe`/`.dll` package, dependency manifest, licenses, profile path and install/update validation; never rename Linux libraries to DLLs |

## Implementation sequence

### 0. Establish reproducible baselines

1. Record the exact ReXGlue, SDL, Dear ImGui, Vulkan-Headers and shader-tool
   revisions used by each target. Keep the existing Vulkan references pinned at
   Khronos Vulkan-Docs `090f1b190d60ced4a1d198fd3747d071cc271b1c` until an
   intentional reference update is reviewed.
2. Capture Linux launcher/backend checks and the current Vulkan game build before
   platform edits. Preserve the accepted package and user settings.
3. Use a fresh Windows SDK build directory. Check the installed Windows SDK,
   compiler ABI, Vulkan headers/loader import library, shader compiler and all
   linked DLLs. MinGW/Wine is a compile and smoke aid; it does not replace a
   native Windows driver run.

### 1. Build the Windows Vulkan SDK target — configure/selected TUs complete; full build pending

1. Configure the pinned `win-amd64` preset with `REXGLUE_USE_VULKAN=ON` and
   `REXGLUE_USE_D3D12=OFF`. Use the checked-in SDK's supported Clang/MSVC
   environment, not Linux `-march` flags or a guessed compiler ABI.
2. Compile/install `rexruntime`, `rexui` and `rexgpu-xenos`; verify the Vulkan
   platform macro is consistent across headers, plugins and DLLs.
3. Fix only verified Windows compile/runtime blockers. Preserve checked
   `VkResult` handling, reverse-order cleanup, surface/device ownership and
   explicit synchronization from existing code.
4. Add a second configuration with both graphics backends only if required by
   the ACV runtime. Do not assume that a successful D3D12 SDK build proves the
   ACV FSI/full Vulkan game path works.

### 2. Add a Vulkan 1.4 renderer to the ACV launcher — Linux tested; Windows cross-link and Wine renderer smoke complete, native validation pending

1. Separate ImGui frame production from the renderer so the ACV UI has one
   shared presentation path. Avoid maintaining separate SDL/Vulkan setup in the
   ACV executable and TRG standalone wrapper.
2. Add a `vulkan`/`opengl` renderer selection. The initial default stays OpenGL
   until Vulkan UI startup, resize and shutdown pass on Windows; make the
   selected mode visible in diagnostics.
3. Require VK_API_VERSION_1_4 from the loader and selected physical device.
   Obtain SDL's required instance extensions, create the SDL Vulkan surface,
   choose graphics/present queues, check VK_KHR_swapchain, create the swapchain
   and ImGui renderer, then present frames with bounded in-flight synchronization.

### 3. Port the ACV Windows application boundary — launcher subset implemented; game bridge pending

1. Split process supervision into POSIX and Win32 implementations behind one
   interface. Preserve stdout/stderr capture, bounded wait, graceful stop,
   child ownership and cleanup; do not terminate unrelated processes.
2. Replace `/proc/self/exe`, `fcntl`, `flock`, POSIX `spawn`, signals and
   XDG-only paths with platform-specific implementations. Keep settings fields,
   validation, backups and UUID/XUID stable across OSes.
3. Port dynamic-library loading and file operations in `acv_compat_shims.cpp`
   and related application code. Retain clear failure paths when a module or
   required file is missing.
4. Add native Windows process, path, lock, settings and install tests before
   connecting the launcher to the game runtime.
5. Check the Python tooling a Windows developer or package would use
   (`tools/acv-context`, `tools/llmignore.py`, `tools/acv_loose_unpack.py`): interpreter
   name (`python`/`py`), `git` on PATH, `CLAUDE_PLUGIN_DATA` location, and `.llmignore`
   matching with `\` path separators. Run the portable tests there: `test_ctxlib.py`, `test_llmignore.py`.
   Verify the `ACVLooseLoader` Windows branches in `PathHelper` and `ConsoleModeManager`.

### 4. Validate game Vulkan on Windows

1. Build the actual ACV host executable and runtime for Windows x64 after the
   application boundary is portable. Stage only the matching `.exe` and `.dll`
   files; keep game data and user profiles outside the source/build tree.
2. Verify loader and ICD discovery, instance extensions, Win32 surface creation,
   surface present support, swapchain formats/modes, queue selection and
   recreation after resize or display changes.
3. Use a required Vulkan baseline derived from the existing code. Probe optional
   features and extensions at runtime; gate synchronization2, timeline
   semaphores, graphics pipeline libraries, dynamic state and other advanced
   paths individually with a correct fallback.
4. Start with one stable menu scene, then test a repeatable mission route,
   rendering, input, pacing, save/load, suspend/resume and clean shutdown.
   Enable the Vulkan validation layer when installed; its absence is recorded,
   not reported as a clean validation run.

### 5. Package and support the launcher/game together

1. Build a versioned Windows package in a new output directory with a hash
   manifest, dependency list, license files and install/update/rollback path.
2. Detect missing loader/driver support before enabling Vulkan rendering and
   show an actionable launcher error. Do not ship a vendor ICD or GPU driver.
   Decide loader redistribution only after checking the selected loader's
   official distribution and license requirements.
3. Keep per-user settings in Windows Known Folders, make launch work from paths
   containing spaces and non-ASCII characters, and ensure package replacement
   preserves profiles and saves.
4. Verify the package on a clean Windows account without the development SDK.

### 6. Cross-OS performance and regression gate

1. Run the Linux launcher, settings, package and Vulkan game checks after shared
   code changes. Build the macOS SDK Vulkan/MoltenVK target on a native macOS
   runner where available; ACV macOS packaging remains a separate capability
   gate until its host application path is verified.
2. Compare launcher idle CPU/GPU use and game frametime p50/p95/p99, CPU/GPU
   utilization, pipeline creation stalls and readback/transfer cost on matched
   scenes and settings per OS.
3. Test at least one available NVIDIA, AMD and Intel driver; record unavailable
   vendors explicitly. Do not ship a default optimization based on one driver
   or one OS. Keep each optional Vulkan optimization independently switchable
   until correctness and repeatable measurements pass.
4. Promote a Windows package only after native Windows UI, game rendering,
   settings, input, save/load and clean-machine package acceptance pass. Update
   [Install.MD](Install.MD#5-windows-x64-sdk-build-and-application-port-status),
   `kb:PLAN`, `kb:PLAN_RU` and `kb:PROGRESS` with the exact
   artifacts, hashes, OS/driver matrix and untested cases.
5. Measure performance with the guest swap cadence (`[acv-display] pacing`) and record
   the GPU vendor and driver with every sample. Host-side MangoHud FPS is not a valid
   metric: it counts window presents. A `readback_resolve` mode other than the accepted
   one can change output, so compare its speed only after an image check on that OS.

## Completion criteria

- A reproducible native Windows x64 Vulkan SDK and ACV game build exists, using
  Vulkan API 1.4; no POSIX-only application dependency remains on the Windows launch path.

## Risks and guardrails

- The pinned Windows SDK route currently configures D3D12 by default and was not
  executed natively for the current package. Vulkan-on build results must be
  recorded separately from existing SDK Windows build notes.
- Win32 Vulkan surface functions exist in the source behind platform guards;
  this is not proof that the complete ACV app builds or presents correctly.
- The launcher has separate OpenGL and Vulkan UI backends; the game uses
  ReXGlue's Vulkan renderer. These paths have distinct device, WSI and failure
  domains.
- MinGW and Wine can expose compiler/linker and basic startup issues. They do
  not validate Windows driver discovery, WSI presentation, performance or
  gameplay.
- Preserve Linux XDG paths and behavior, macOS MoltenVK loader rules, guest ABI,
  user settings and existing package checkpoints while isolating OS adapters.
- Platform branches added to shared code (`PathHelper`, `ConsoleModeManager`, launcher
  text) were checked only on Linux. Their Windows behavior is unverified until the native
  tests in section 3 cover them.

## References

- [LunarG Vulkan SDK downloads](https://vulkan.lunarg.com/sdk/home) and [official release notes](https://vulkan.lunarg.com/doc/view/latest/windows/release_notes.html)
- [LunarG Windows getting started](https://vulkan.lunarg.com/doc/view/latest/windows/getting_started.html)
- [Khronos Vulkan Loader driver discovery and VK_DRIVER_FILES](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderDriverInterface.md)

- Windows SDK/app blockers and supported preset: [Install.MD](Install.MD#5-windows-x64-sdk-build-and-application-port-status)
- ReXGlue Windows Vulkan option and Win32 surface implementation: `tools/rexglue-sdk/CMakeLists.txt`, `tools/rexglue-sdk/include/rex/ui/vulkan/api.h`, `tools/rexglue-sdk/src/ui/vulkan/vulkan_instance.cpp`, `tools/rexglue-sdk/src/ui/vulkan/vulkan_presenter.cpp`
- Launcher OpenGL backend and POSIX process boundary: `tools/acv-trg-launcher/main.cpp`, `tools/acv-trg-launcher/process.h`, `tools/acv-trg-launcher/CMakeLists.txt`
- Khronos Vulkan-Docs pinned snapshot: `090f1b190d60ced4a1d198fd3747d071cc271b1c`, `appendices/VK_KHR_win32_surface.adoc`, `chapters/VK_KHR_surface/wsi.adoc`
- Win32 Vulkan declarations: `references/Vulkan-Headers/include/vulkan/vulkan_win32.h`
