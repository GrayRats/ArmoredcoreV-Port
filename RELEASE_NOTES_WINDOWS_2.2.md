# ACV-Windows 2.2 — release notes

## English

Native Windows 11 x64 package of ARMORED CORE V on ReXGlue SDK v0.10.1 with the ACV patches. The launcher and the game render through Vulkan; the game can also use DirectX 12 (a launcher setting). The package is cross-compiled with MinGW on Linux; it does not need Wine.

### What changed

- **Native Windows package** `ACV-Windows-release-2.2`: `start.cmd` runs `launcher\acv-launcher.exe` directly. The Wine-only layout (`wine/`, `start-wine.sh`) is not part of this package.
- **Vulkan-only launcher.** Windows builds link no OpenGL. The launcher window renders through Vulkan; `vulkan-1.dll` comes with the GPU driver.
- **Graphics API setting** (Graphics page → «Графический API игры»): `Vulkan` (default) or `DirectX 12`. The setting takes effect at the next game start. The plugin is built with both backends (`REXGLUE_USE_D3D12=ON`, `REXGLUE_USE_VULKAN=ON`).
- **Shader cache setting** (Graphics page → «Кэш шейдеров»): `Вкл.` (default) keeps translated shaders, pipeline descriptions and the Vulkan driver cache between runs; `Выкл.` reads and writes none of them. The setting takes effect at the next game start. The driver cache is enabled by `start.cmd`, not by running `launcher\acv-launcher.exe` directly.
- **Shorter pipeline-creation stalls:** new SDK cvar `vulkan_pipeline_processor_assist`; ACV passes `--vulkan_pipeline_processor_assist=false`, so the processor thread no longer waits for worker-created pipelines at the end of each submission. A/B run on the same cache state: largest frame 559 ms → 291 ms. The sustained 15–16 FPS in the heaviest scene is not solved. Frames that need a pending pipeline use the placeholder path.
- **Emblem and thumbnail assets:** the enemy and map thumbnails (`EneThumbnail/e1020, e2020, e0240, e2030`, `mapthumbnail/briefingmap00, briefingmap07`) that the game requests through `dlc_image:` are now in the package; they are read from the retail archive shards and verified by hash.
- Launch-time log levels: the GPU plugin load line, the placeholder frame skip and the expected missing optional files are logged as info, so the warnings that remain point to real problems.
- Japanese UI glyphs: `ACV_JP_FONT` can point to a CJK font; by default the system `msgothic.ttc` is used.

### Update 2026-11-10 (first runs on Windows 11, GTX 1660 SUPER)

- **DirectX 12 works with the `rov` render-target path and readback `fast`.** Tested: `rtv`+`fast` black gameplay; `rtv`+`full` black gameplay; `rov`+`full` loading never finishes; `rov`+`fast` normal picture. For DirectX 12 the launcher passes `--render_target_path_d3d12=rov` and replaces the synchronous readback `full` by `fast` (with a warning). Vulkan keeps `full`. The cause of the black `rtv` picture is not found (same symptom as the rejected Vulkan FBO path); an explicit `render_target_path_d3d12` in `acv.config [sdk]` overrides the launcher.
- **`d3d12_pipeline_processor_assist`** (patch `tools/native-rexglue/patches/d3d12-pipeline-processor-assist.patch`, built natively into `rexgpu-xenos.dll`): the launcher passes `--d3d12_pipeline_processor_assist=false` when the plugin has the cvar. One 3.5-minute DirectX 12 run after the loading phase: 30 FPS, median p99 37 ms, largest frame 160 ms; the comparable run without it had p99 up to 412 ms and a 563 ms frame. A single short run, not a benchmark. The same patch fixes the preload loop that never started its extra creation threads.
- **Launcher:** an option the build cannot honour (DirectX 12, 10-bit, CAS/FSR, frame limit, wide view) now degrades with a warning instead of blocking Save, Play and presets; the saved build path follows the package (a stale path to another folder is replaced and reported); optional `acv-capabilities.json` with a scan fallback; a Diagnostics button (clipboard and `logs\diagnostics-*.txt`); a session-log folder row; MangoHud and controller rows are hidden on Windows; Play starts the build shown on the settings page. `start.cmd` installs `launcher\acv-launcher-new.exe` over `acv-launcher.exe` on start.
- **Tools:** `tools/native-rexglue/pacing_report.py` compares `[acv-display] pacing` between runs; `build_win_gpu_plugin.sh` builds `rexgpu-xenos.dll` natively in MSYS2 UCRT64 with clang (CLANG64/libc++ does not compile the SDK and does not match the shipped libstdc++), with backups and an import-symbol check.
- **Unchanged:** `librexruntime.dll`. Windows asset read-ahead, a waitable swapchain, DRED enablement and timer resolution were evaluated; no problem in the logs justified replacing the runtime.

### Verification status and limits

- Run through Faugus Launcher's runner (`umu-run`, Proton-Wineland Latest), 90 s each, final binaries: Vulkan — no errors, NVIDIA GeForce GTX 1660 SUPER, 51 shards served, about 20 fps in the frame-pacing log; DirectX 12 — no errors, about 48 fps. These are single short runs, not a controlled benchmark.
- Earlier, plain Wine with the DirectX 12 backend failed to create compute and graphics pipelines; under Proton it did not. The cause in the plain-Wine case was not identified.
- **Not verified:** a real Windows 11 machine (everything above ran under Proton/Wine on Linux); gameplay, sound, input, saves; the emblem and thumbnail screens in game; the visual result of placeholder frames with the processor-assist flag off.
- **FidelityFX is not in the Windows build.** Saved CAS/FSR settings fall back to the default scaling with a warning (since the 2026-11-10 update; before it they stopped the launch).
- Expected warnings in the logs: `ExCreateThread` (guest creates a system thread) and `Texture fetch constant ... invalid type` (documented; the game binds a null texture there).
- Four optional sound banks (`acv_bgm_title.fev`, `acv_dlc_dlc_se.fev`, `dlc_se.fsb`, `ch09.fsb`) and `acv_talk_jp.fev` are not in the retail disc image; the game receives the normal "not found" status for them.
- The game data (`data/`, about 4 GB) is **not** part of the public release; the package is distributed only where the owner's data rights allow it.

### Build

- Host: Linux, clang 22 with the `x86_64-w64-mingw32` target; SDK `tools/rexglue-sdk` (branch `acv-windows-sdk`, commits `3808f0f` and `6b32cc5`) on top of `v0.10.1`; launcher `tools/acv-trg-launcher` (CMake, `ACV_LAUNCHER_OPENGL=OFF` on Windows).
- Local archive `ACV-Windows-release-2.2.tar.zst` (3.4 GB), sha256 `becf9ad052ac52b636afc4af14ec67fa601826a039bd256920b44ccf6e8f9d3e`. It is not attached to the release. The hash belongs to the earlier archive; the folder was updated after it and the archive has not been rebuilt.

## Русский

Нативный пакет ARMORED CORE V для Windows 11 x64 на ReXGlue SDK v0.10.1 с ACV-патчами. Лаунчер и игра рисуют через Vulkan; игра также может работать через DirectX 12 (настройка в лаунчере). Пакет собран кросс-компиляцией MinGW на Linux и не требует Wine.

### Что изменилось

- **Нативный пакет Windows** `ACV-Windows-release-2.2`: `start.cmd` запускает `launcher\acv-launcher.exe` напрямую. Раскладка только для Wine (`wine/`, `start-wine.sh`) в этот пакет не входит.
- **Лаунчер только на Vulkan.** Сборка для Windows не линкует OpenGL. Окно лаунчера рисуется через Vulkan; `vulkan-1.dll` поставляет драйвер видеокарты.
- **Настройка графического API** (страница «Графика» → «Графический API игры»): `Vulkan` (по умолчанию) или `DirectX 12`. Применяется при следующем запуске игры. Плагин собран с обоими бэкендами (`REXGLUE_USE_D3D12=ON`, `REXGLUE_USE_VULKAN=ON`).
- **Настройка «Кэш шейдеров»** (страница «Графика» → «Кэш шейдеров»): `Вкл.` (по умолчанию) сохраняет между запусками переведённые шейдеры, описания pipeline и кэш драйвера Vulkan; `Выкл.` не читает и не пишет ни один из них. Применяется при следующем запуске игры. Кэш драйвера подключает `start.cmd`, а не прямой запуск `launcher\acv-launcher.exe`.
- **Меньше подвисаний при создании pipeline:** новый cvar SDK `vulkan_pipeline_processor_assist`; ACV передаёт `--vulkan_pipeline_processor_assist=false`, поэтому поток процессора больше не ждёт pipeline, которые создают рабочие потоки, в конце каждой отправки. A/B-прогон на одинаковом состоянии кэша: самый долгий кадр 559 мс → 291 мс. Устойчивые 15–16 FPS в самой тяжёлой сцене не устранены. Кадры, которым нужен ещё не созданный pipeline, используют заглушку.
- **Эмблемы и миниатюры:** миниатюры врагов и карт (`EneThumbnail/e1020, e2020, e0240, e2030`, `mapthumbnail/briefingmap00, briefingmap07`), которые игра запрашивает через `dlc_image:`, теперь входят в пакет. Они читаются из шардов retail-архива и проверены по хэшу.
- Уровни логирования: строка загрузки GPU-плагина, пропуск кадров в ожидании пайплайнов и ожидаемо отсутствующие необязательные файлы пишутся как info, поэтому оставшиеся предупреждения указывают на реальные проблемы.
- Японские символы интерфейса: `ACV_JP_FONT` может указывать на CJK-шрифт; по умолчанию используется системный `msgothic.ttc`.

### Обновление 2026-11-10 (первые запуски на Windows 11, GTX 1660 SUPER)

- **DirectX 12 работает с путём рендера `rov` и режимом чтения `fast`.** Проверено: `rtv`+`fast` чёрный gameplay; `rtv`+`full` чёрный gameplay; `rov`+`full` загрузка не завершается; `rov`+`fast` нормальная картинка. Для DirectX 12 лаунчер передаёт `--render_target_path_d3d12=rov` и заменяет синхронное чтение `full` на `fast` (с предупреждением). Для Vulkan остаётся `full`. Причина чёрной картинки на `rtv` не найдена (тот же симптом, что у отклонённого пути FBO в Vulkan); явный `render_target_path_d3d12` в `acv.config [sdk]` перекрывает выбор лаунчера.
- **`d3d12_pipeline_processor_assist`** (патч `tools/native-rexglue/patches/d3d12-pipeline-processor-assist.patch`, собран здесь в `rexgpu-xenos.dll`): лаунчер передаёт `--d3d12_pipeline_processor_assist=false`, если в плагине есть этот cvar. Один 3,5-минутный запуск DirectX 12 после загрузки: 30 FPS, медианный p99 37 мс, самый долгий кадр 160 мс; сопоставимый запуск без него давал p99 до 412 мс и кадр 563 мс. Это один короткий запуск, а не бенчмарк. Тот же патч исправляет цикл предзагрузки, который не запускал дополнительные потоки создания.
- **Лаунчер:** вариант, который сборка не поддерживает (DirectX 12, 10 бит, CAS/FSR, лимит кадров, wide view), теперь откатывается с предупреждением, а не блокирует сохранение, запуск и наборы; сохранённый путь сборки следует за пакетом (устаревший путь в другую папку заменяется и сообщается); необязательный `acv-capabilities.json` с откатом на сканирование; кнопка «Диагностика» (буфер обмена и `logs\diagnostics-*.txt`); строка с папкой журналов сессий; строки MangoHud и контроллера скрыты на Windows; «Играть» запускает сборку, показанную на странице настроек. `start.cmd` ставит `launcher\acv-launcher-new.exe` поверх `acv-launcher.exe` при запуске.
- **Инструменты:** `tools/native-rexglue/pacing_report.py` сравнивает `[acv-display] pacing` между запусками; `build_win_gpu_plugin.sh` собирает `rexgpu-xenos.dll` нативно в MSYS2 UCRT64 через clang (CLANG64/libc++ не компилирует SDK и не совпадает с поставляемым libstdc++), с резервными копиями и проверкой импортируемых символов.
- **Без изменений:** `librexruntime.dll`. Подгрузка архивов на Windows, waitable swapchain, включение DRED и разрешение таймера рассматривались; проблем в логах, оправдывающих замену runtime, нет.

### Статус проверки и ограничения

- Запуск через раннер Faugus Launcher (`umu-run`, Proton-Wineland Latest), по 90 с на каждый вариант, финальные бинарники: Vulkan — ошибок нет, NVIDIA GeForce GTX 1660 SUPER, 51 шард, около 20 FPS в журнале темпа кадров; DirectX 12 — ошибок нет, около 48 FPS. Это короткие одиночные прогоны, а не контролируемый бенчмарк.
- Ранее в «чистом» Wine с бэкендом DirectX 12 не создавались пайплайны вычислительных и графических шейдеров; под Proton — создаются. Причина в «чистом» Wine не установлена.
- **Не проверено:** реальная машина с Windows 11 (всё выше шло под Proton/Wine на Linux); геймплей, звук, управление, сохранения; экраны эмблем и миниатюр в игре; визуальный результат кадров-заглушек при выключенном флаге `vulkan_pipeline_processor_assist`.
- **В сборке Windows нет FidelityFX.** Сохранённые настройки CAS/FSR откатываются к обычному масштабированию с предупреждением (после обновления 2026-11-10; раньше они останавливали запуск).
- Ожидаемые предупреждения в журналах: `ExCreateThread` (гость создаёт системный поток) и `Texture fetch constant ... invalid type` (задокументировано: игра привязывает пустую текстуру).
- Четыре необязательных звуковых банка (`acv_bgm_title.fev`, `acv_dlc_dlc_se.fev`, `dlc_se.fsb`, `ch09.fsb`) и `acv_talk_jp.fev` отсутствуют в retail-образе диска; игра получает обычный статус «файл не найден».
- Игровые данные (`data/`, около 4 ГБ) **не входят** в публичный релиз; пакет распространяется только там, где это разрешают права владельца данных.

### Сборка

- Среда: Linux, clang 22 с целью `x86_64-w64-mingw32`; SDK `tools/rexglue-sdk` (ветка `acv-windows-sdk`, коммиты `3808f0f` и `6b32cc5`) поверх `v0.10.1`; лаунчер `tools/acv-trg-launcher` (CMake, на Windows `ACV_LAUNCHER_OPENGL=OFF`).
- Локальный архив `ACV-Windows-release-2.2.tar.zst` (3.4 ГБ), sha256 `becf9ad052ac52b636afc4af14ec67fa601826a039bd256920b44ccf6e8f9d3e`. В релиз он не прикладывается. Хэш относится к предыдущему архиву: папка обновлена после него, архив не пересобирался.
