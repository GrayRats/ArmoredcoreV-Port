# ACV-Windows 2.2 — release notes

## English

Native Windows 11 x64 package of ARMORED CORE V on ReXGlue SDK v0.10.1 with the ACV patches. The launcher and the game render through Vulkan; the game can also use DirectX 12 (a launcher setting). The package is cross-compiled with MinGW on Linux; it does not need Wine.

### What changed

- **Native Windows package** `ACV-Windows-release-2.2`: `start.cmd` runs `launcher\acv-launcher.exe` directly. The Wine-only layout (`wine/`, `start-wine.sh`) is not part of this package.
- **Vulkan-only launcher.** Windows builds link no OpenGL. The launcher window renders through Vulkan; `vulkan-1.dll` comes with the GPU driver.
- **Graphics API setting** (Graphics page → «Графический API игры»): `Vulkan` (default) or `DirectX 12`. The setting takes effect at the next game start. The plugin is built with both backends (`REXGLUE_USE_D3D12=ON`, `REXGLUE_USE_VULKAN=ON`).
- **Emblem and thumbnail assets:** the enemy and map thumbnails (`EneThumbnail/e1020, e2020, e0240, e2030`, `mapthumbnail/briefingmap00, briefingmap07`) that the game requests through `dlc_image:` are now in the package; they are read from the retail archive shards and verified by hash.
- Launch-time log levels: the GPU plugin load line, the placeholder frame skip and the expected missing optional files are logged as info, so the warnings that remain point to real problems.
- Japanese UI glyphs: `ACV_JP_FONT` can point to a CJK font; by default the system `msgothic.ttc` is used.

### Verification status and limits

- Run through Faugus Launcher's runner (`umu-run`, Proton-Wineland Latest), 90 s each, final binaries: Vulkan — no errors, NVIDIA GeForce GTX 1660 SUPER, 51 shards served, about 20 fps in the frame-pacing log; DirectX 12 — no errors, about 48 fps. These are single short runs, not a controlled benchmark.
- Earlier, plain Wine with the DirectX 12 backend failed to create compute and graphics pipelines; under Proton it did not. The cause in the plain-Wine case was not identified.
- **Not verified:** a real Windows 11 machine (everything above ran under Proton/Wine on Linux); gameplay, sound, input, saves; the emblem and thumbnail screens in game.
- **FidelityFX is not in the Windows build.** Saved CAS/FSR settings stop the launch with a message; switch the scaling to the default in the Graphics page.
- Expected warnings in the logs: `ExCreateThread` (guest creates a system thread) and `Texture fetch constant ... invalid type` (documented; the game binds a null texture there).
- Four optional sound banks (`acv_bgm_title.fev`, `acv_dlc_dlc_se.fev`, `dlc_se.fsb`, `ch09.fsb`) and `acv_talk_jp.fev` are not in the retail disc image; the game receives the normal "not found" status for them.
- The game data (`data/`, about 4 GB) is **not** part of the public release; the package is distributed only where the owner's data rights allow it.

### Build

- Host: Linux, clang 22 with the `x86_64-w64-mingw32` target; SDK `tools/rexglue-sdk` (branch `acv-windows-sdk`, commit `3808f0f`) on top of `v0.10.1`; launcher `tools/acv-trg-launcher` (CMake, `ACV_LAUNCHER_OPENGL=OFF` on Windows).
- Local archive `ACV-Windows-release-2.2.tar.zst` (3.4 GB), sha256 `becf9ad052ac52b636afc4af14ec67fa601826a039bd256920b44ccf6e8f9d3e`. It is not attached to the release.

## Русский

Нативный пакет ARMORED CORE V для Windows 11 x64 на ReXGlue SDK v0.10.1 с ACV-патчами. Лаунчер и игра рисуют через Vulkan; игра также может работать через DirectX 12 (настройка в лаунчере). Пакет собран кросс-компиляцией MinGW на Linux и не требует Wine.

### Что изменилось

- **Нативный пакет Windows** `ACV-Windows-release-2.2`: `start.cmd` запускает `launcher\acv-launcher.exe` напрямую. Раскладка только для Wine (`wine/`, `start-wine.sh`) в этот пакет не входит.
- **Лаунчер только на Vulkan.** Сборка для Windows не линкует OpenGL. Окно лаунчера рисуется через Vulkan; `vulkan-1.dll` поставляет драйвер видеокарты.
- **Настройка графического API** (страница «Графика» → «Графический API игры»): `Vulkan` (по умолчанию) или `DirectX 12`. Применяется при следующем запуске игры. Плагин собран с обоими бэкендами (`REXGLUE_USE_D3D12=ON`, `REXGLUE_USE_VULKAN=ON`).
- **Эмблемы и миниатюры:** миниатюры врагов и карт (`EneThumbnail/e1020, e2020, e0240, e2030`, `mapthumbnail/briefingmap00, briefingmap07`), которые игра запрашивает через `dlc_image:`, теперь входят в пакет. Они читаются из шардов retail-архива и проверены по хэшу.
- Уровни логирования: строка загрузки GPU-плагина, пропуск кадров в ожидании пайплайнов и ожидаемо отсутствующие необязательные файлы пишутся как info, поэтому оставшиеся предупреждения указывают на реальные проблемы.
- Японские символы интерфейса: `ACV_JP_FONT` может указывать на CJK-шрифт; по умолчанию используется системный `msgothic.ttc`.

### Статус проверки и ограничения

- Запуск через раннер Faugus Launcher (`umu-run`, Proton-Wineland Latest), по 90 с на каждый вариант, финальные бинарники: Vulkan — ошибок нет, NVIDIA GeForce GTX 1660 SUPER, 51 шард, около 20 FPS в журнале темпа кадров; DirectX 12 — ошибок нет, около 48 FPS. Это короткие одиночные прогоны, а не контролируемый бенчмарк.
- Ранее в «чистом» Wine с бэкендом DirectX 12 не создавались пайплайны вычислительных и графических шейдеров; под Proton — создаются. Причина в «чистом» Wine не установлена.
- **Не проверено:** реальная машина с Windows 11 (всё выше шло под Proton/Wine на Linux); геймплей, звук, управление, сохранения; экраны эмблем и миниатюр в игре.
- **В сборке Windows нет FidelityFX.** Сохранённые настройки CAS/FSR останавливают запуск с сообщением; переключите масштабирование на обычное на странице «Графика».
- Ожидаемые предупреждения в журналах: `ExCreateThread` (гость создаёт системный поток) и `Texture fetch constant ... invalid type` (задокументировано: игра привязывает пустую текстуру).
- Четыре необязательных звуковых банка (`acv_bgm_title.fev`, `acv_dlc_dlc_se.fev`, `dlc_se.fsb`, `ch09.fsb`) и `acv_talk_jp.fev` отсутствуют в retail-образе диска; игра получает обычный статус «файл не найден».
- Игровые данные (`data/`, около 4 ГБ) **не входят** в публичный релиз; пакет распространяется только там, где это разрешают права владельца данных.

### Сборка

- Среда: Linux, clang 22 с целью `x86_64-w64-mingw32`; SDK `tools/rexglue-sdk` (ветка `acv-windows-sdk`, коммит `3808f0f`) поверх `v0.10.1`; лаунчер `tools/acv-trg-launcher` (CMake, на Windows `ACV_LAUNCHER_OPENGL=OFF`).
- Локальный архив `ACV-Windows-release-2.2.tar.zst` (3.4 ГБ), sha256 `becf9ad052ac52b636afc4af14ec67fa601826a039bd256920b44ccf6e8f9d3e`. В релиз он не прикладывается.
