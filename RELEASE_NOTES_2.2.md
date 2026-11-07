# ACV-Linux 2.2 — release notes

## English

**Build and install:** [Linux/Windows x64-v2 guide](https://github.com/GrayRats/ArmoredcoreV-Port/blob/main/Install.MD) · [Build tools package](https://github.com/GrayRats/ArmoredcoreV-Port/pkgs/container/acv-build-tools). The attached `BUILD_2.2.md` (inside `ACV-2.2-build-sources.tar.gz`) lists the exact SDK, probe and launcher build steps of this release.

Release 2.2 moves the project to ReXGlue SDK v0.10.1, replaces the 3.3 GB `dvdbnd.bdt` with 51 shards, adds a launcher setting for the texture readback mode and brings the Wine launcher to the same behaviour as the Linux launcher.

### What changed

- **ReXGlue SDK v0.10.1** with the ACV patches re-applied (patch against the upstream tag is attached). Linux and Windows/Wine builds of the runtime, GPU plugin and game probe are rebuilt from it. The Linux build links the prebuilt FidelityFX Vulkan library of the same pinned FidelityFX commit.
- **`dvdbnd.bdt` is no longer shipped.** The archive is split into 51 shards with an index (`tools/acv_shard_dvdbnd.py`); the SDK serves `bind\dvdbnd.bdt` from them (cvar `sharded_bdt`). The shards were verified byte for byte against the original archive and the virtual archive hash equals the verified sha256. Installing from an ISO converts the extracted archive to shards and removes it; the launchers no longer require `dvdbnd.bdt`. `bind/dvdbnd5.bhd` is still required.
- **`readback_resolve = full` is the default.** The delayed `fast` mode produced black mech parts and terrain on this build (confirmed in game). A launcher setting now switches `full`/`fast` and explains the trade-off.
- **Wine/Windows launcher** builds its command line and environment with the same code as Linux (`acv_launch_spec.py`): saved graphics settings, input layout (SDK default `mnk_mode` is off), pilot name and gamertag, DLC mounts, profile XUID. The ACV host shims are built for Windows. `start.cmd` and `wine/start-wine.sh` export the same ACV variables as `start.sh`.
- **Build speed:** the Windows probe links with lld (seconds instead of about 20 minutes); ccache is available with `-DACV_USE_CCACHE=ON`.

### Source and diagnostics

- `tools/native-rexglue/refresh_package_manifest.py`, `recover_adjusting_thunks.py`, a launcher `--autoplay` flag for smoke runs, and the knowledge base unpacked to `docs/` (`tools/acv-context/export_docs.py`).
- Assets of this release: `ACV-2.2-build-sources.tar.gz` (project sources at the commit in `BUILD-SOURCE-COMMIT.txt`, the SDK patch and new SDK files, the probe CMake project, the recovered adjusting thunks file, `BUILD_2.2.md`) and its SHA256SUMS.

### Verification status and limits

- Linux and Wine launchers start the game from the 2.2 package without `dvdbnd.bdt` (60–100 s runs, no runtime errors in the logs). Input, sound and gameplay were not verified beyond logs; the load-time gain of the shards was not measured.
- The game package (`ACV-Linux-release-2.2`, 4.2 GB, contains game data) is **not** attached to this release. Only sources and build files are public here.
- FPS depends on the scene and the readback mode: `full` is slower than `fast` in the earlier title-scene measurements (about 16–17 vs 26–27 fps; different runs, not a controlled A/B).

## Русский

**Сборка и установка:** [инструкция Linux/Windows x64-v2](https://github.com/GrayRats/ArmoredcoreV-Port/blob/main/Install.RU.MD) · [пакет инструментов](https://github.com/GrayRats/ArmoredcoreV-Port/pkgs/container/acv-build-tools). Приложенный `BUILD_2.2.md` (внутри `ACV-2.2-build-sources.tar.gz`) описывает точные шаги сборки SDK, пробы и лаунчеров этого релиза.

Релиз 2.2 переводит проект на ReXGlue SDK v0.10.1, заменяет `dvdbnd.bdt` (3,3 ГБ) на 51 шард, добавляет в лаунчер настройку режима возврата текстур и приводит Wine-лаунчер к поведению Linux-лаунчера.

### Что изменилось

- **ReXGlue SDK v0.10.1** с повторно применёнными ACV-патчами (патч к upstream-тегу приложен). Linux- и Windows/Wine-сборки runtime, GPU-плагина и пробы игры пересобраны из него. Linux-сборка использует готовую Vulkan-библиотеку FidelityFX того же закреплённого коммита FidelityFX.
- **`dvdbnd.bdt` больше не поставляется.** Архив разбит на 51 шард с индексом (`tools/acv_shard_dvdbnd.py`); SDK отдаёт `bind\dvdbnd.bdt` из шардов (cvar `sharded_bdt`). Шарды проверены побайтно с исходным архивом, хеш виртуального архива равен проверенному sha256. Установка из ISO сама превращает распакованный архив в шарды и удаляет его; лаунчеры больше не требуют `dvdbnd.bdt`. Файл `bind/dvdbnd5.bhd` по-прежнему нужен.
- **`readback_resolve = full` по умолчанию.** Отложенный режим `fast` давал чёрные детали мехов и территории на этой сборке (подтверждено в игре). В лаунчере появилась настройка `full`/`fast` с пояснением.
- **Wine/Windows-лаунчер** строит командную строку и окружение тем же кодом, что Linux (`acv_launch_spec.py`): сохранённые настройки графики, раскладка ввода (в SDK по умолчанию `mnk_mode` выключен), имя пилота и gamertag, подключение DLC, XUID профиля. ACV-шимы собираются и под Windows. `start.cmd` и `wine/start-wine.sh` задают те же переменные ACV, что `start.sh`.
- **Скорость сборки:** Windows-проба линкуется через lld (секунды вместо ~20 минут); ccache включается опцией `-DACV_USE_CCACHE=ON`.

### Исходники и диагностика

- `tools/native-rexglue/refresh_package_manifest.py`, `recover_adjusting_thunks.py`, флаг лаунчера `--autoplay` для смоук-запусков и база знаний, распакованная в `docs/` (`tools/acv-context/export_docs.py`).
- Файлы релиза: `ACV-2.2-build-sources.tar.gz` (исходники проекта на коммите из `BUILD-SOURCE-COMMIT.txt`, патч SDK и новые файлы SDK, CMake-проект пробы, восстановленный файл adjusting thunks, `BUILD_2.2.md`) и его SHA256SUMS.

### Статус проверки и ограничения

- Linux- и Wine-лаунчеры запускают игру из пакета 2.2 без `dvdbnd.bdt` (запуски по 60–100 с, ошибок в логах нет). Ввод, звук и геймплей проверены только по логам; выигрыш шардов по времени загрузки не измерялся.
- Игровой пакет (`ACV-Linux-release-2.2`, 4,2 ГБ, содержит игровые данные) **не** приложен к релизу. Здесь публикуются только исходники и файлы для сборки.
- FPS зависит от сцены и режима readback: `full` медленнее `fast` в прежних замерах загрузочной сцены (около 16–17 против 26–27 fps; разные запуски, не контролируемый A/B).
