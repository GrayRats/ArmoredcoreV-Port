# Сборка и установка ACV

[English](Install.MD) · [Русский](Install.RU.MD) · [Релизы](https://github.com/GrayRats/ArmoredcoreV-Port/releases) · [Исходники](https://github.com/GrayRats/ArmoredcoreV-Port)

> **Платформа:** поддерживаемая цель приложения — Linux x86-64-v2. SDK также имеет пресет Windows x64. В ACV-приложении, Python-backend и лаунчере остаются POSIX-зависимости; готовой нативной Windows-сборки игры в этом дереве пока нет.

## Содержание

1. [Выберите способ установки](#1-выберите-способ-установки)
2. [Инструменты, процессор и ABI](#2-инструменты-процессор-и-abi)
3. [Подготовьте исходники](#3-подготовьте-исходники)
4. [Сборка в Linux](#4-сборка-в-linux)
5. [Windows x64: SDK и состояние порта приложения](#5-windows-x64-sdk-и-состояние-порта-приложения)
6. [Создание Linux-пакета](#6-создание-linux-пакета)
7. [GitHub Packages и окружение сборки](#7-github-packages-и-окружение-сборки)
8. [Решение проблем](#8-решение-проблем)

## 1. Выберите способ установки

| Способ | Что входит | Для чего подходит |
|---|---|---|
| Готовый Linux-пакет | Лаунчер, runtime, игровой ELF, данные, лицензии и манифест | Запуск в совместимой Linux-системе |
| Репозиторий/архив исходников | Шимы, инструменты, CMake-шаблоны, примеры конфигурации, документация | Сборка лаунчера и SDK, разработка порта |
| Полное рабочее дерево ACV | Всё выше **плюс** generated translation, entrypoint-проект, adjusting thunks и согласованный SDK с патчами | Пересборка игры по процедуре сопровождающего |
| Образ GitHub Packages | Linux-компилятор и зависимости | Компиляция в отдельном окружении |

В исходный GitHub ZIP/tarball не входят игнорируемый `tmp/`, generated translation, внешние checkout и бинарные результаты SDK. Одна распаковка исходников не восстанавливает принятую игровую сборку.

### Установка готового Linux-пакета

Скачайте из [релиза 1.1](https://github.com/GrayRats/ArmoredcoreV-Port/releases/tag/v1.1.0):

- `ACV-Linux-release-1.0.tar.zst.part-00`
- `ACV-Linux-release-1.0.tar.zst.part-01`
- `ACV-Linux-release-1.0.parts.sha256`
- `ACV-Linux-release-1.0.tar.zst.sha256`
- `ACV-Linux-release-1.0-PROVENANCE.md`

В каталоге загрузки выполните:

```bash
sha256sum -c ACV-Linux-release-1.0.parts.sha256
cat ACV-Linux-release-1.0.tar.zst.part-* > ACV-Linux-release-1.0.tar.zst
sha256sum -c ACV-Linux-release-1.0.tar.zst.sha256
zstd -t ACV-Linux-release-1.0.tar.zst
tar --zstd -xf ACV-Linux-release-1.0.tar.zst
./ACV-Linux-release-1.0/start.sh
```

Сам бинарный пакет обозначен как **1.0**. Релиз **1.1** также содержит исходники и документацию проекта. Эта упаковка не доказывает новый результат FPS.

![Существующая игровая сессия Linux-порта](docs/images/gameplay-vulkan.jpg)

*Существующий игровой кадр Linux-порта. Это не подтверждение Windows-сборки или результат замера производительности.*

## 2. Инструменты, процессор и ABI

| Компонент | Требование |
|---|---|
| CPU | x86-64-v2; флаг компиляции `-march=x86-64-v2` |
| Компилятор | LLVM Clang **18+** с C++23; SDK отклоняет GCC |
| Сборка | CMake **3.25+**, Ninja, Git; самому лаунчеру достаточно CMake 3.24+ |
| Скрипты | Python **3.11+**, Node.js **22+**, npm |
| Linux-лаунчер | SDL2, OpenGL 3.3, X11/XWayland; SDL2/ImGui загружаются по закреплённым ревизиям |
| Рендер игры | Vulkan-драйвер с возможностями, необходимыми выбранному GPU-plugin SDK |
| Windows SDK | Visual Studio 2022 C++ Build Tools, Windows SDK, LLVM Clang 18+, CMake, Ninja |

`x86-64-v2` описывает инструкции процессора, а не совместимость glibc или Windows. Для распространяемой v2-сборки не используйте `-march=native`. Оптимизацию задаёт Release; добавлять `-ffast-math` не требуется.

> **ABI готовых файлов, проверено 29.10.2026:** `librexruntime.so` требует символы до `GLIBC_2.44` и `GLIBCXX_3.4.36`; игровой ELF — `GLIBC_2.43`/`GLIBCXX_3.4.35`; лаунчер — `GLIBC_2.43`/`GLIBCXX_3.4.32`. На более старом дистрибутиве может потребоваться пересборка всего согласованного набора. Не заменяйте системную libc скопированной библиотекой.

Проверка на целевой машине:

```bash
lscpu
getconf GNU_LIBC_VERSION
readelf --version-info ACV-Linux-release-1.0/bin/game/librexruntime.so
ldd ACV-Linux-release-1.0/bin/game/acv_linux_probe
```

### Зависимости сборки Ubuntu 24.04

Это установка **окружения сборки**. Она не делает готовый игровой ELF, собранный с более новой glibc, совместимым с Ubuntu 24.04.

```bash
sudo apt-get update
sudo apt-get install git git-lfs build-essential clang-18 lld-18 cmake ninja-build \
  python3 pkg-config libvulkan-dev libgl1-mesa-dev libegl1-mesa-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev \
  libwayland-dev libxkbcommon-dev libasound2-dev libpulse-dev libudev-dev \
  fonts-dejavu-core zstd patchelf
```

Node 22+ установите отдельно с [Node.js](https://nodejs.org/en/download) или используйте [контейнер сборки](#7-github-packages-и-окружение-сборки). Проверьте `node --version`: штатный Node из репозитория Ubuntu может быть слишком старым для логгера.

### Зависимости Arch/CachyOS

```bash
sudo pacman -S --needed base-devel clang lld cmake ninja git python nodejs npm \
  pkgconf vulkan-headers vulkan-icd-loader mesa libglvnd libx11 libxrandr \
  libxcursor libxi libxinerama wayland libxkbcommon alsa-lib libpulse \
  systemd-libs zstd patchelf ttf-dejavu
```

Драйвер GPU установите отдельно под своё оборудование. Эти списки зависимостей не заменяют работающий Vulkan-драйвер.

## 3. Подготовьте исходники

```bash
git clone https://github.com/GrayRats/ArmoredcoreV-Port.git ACV
cd ACV
git rev-parse HEAD
node --version
python3 --version
clang++ --version
cmake --version
ninja --version
npm ci --prefix tools/decomp-logging
```

`npm ci` устанавливает зависимости логгера. Последующие анализ, сборку и упаковку выполняйте через него. В существующем рабочем дереве сохраняйте текущую ветку и локальные изменения.

Получите внешние зависимости на используемых проектом ревизиях:

```bash
git clone https://github.com/TekRantGaming/trg-launcher.git tools/trg-launcher
git -C tools/trg-launcher checkout --detach b384a28c712c9320eea8f9d50639dec49981b6c1
git clone https://github.com/rexglue/rexglue-sdk.git tools/rexglue-sdk
git -C tools/rexglue-sdk checkout --detach c94f5ebdcb3c9d1a460ca48e04f9758448f8d518
git -C tools/rexglue-sdk submodule update --init --recursive
```

Клонирование выполняйте только для отсутствующих каталогов. Указанная ревизия SDK — **основа** ACV-модификаций, а не готовое воспроизведение принятого runtime с патчами. Ревизии SDL2/ImGui записаны в [dependencies.json](tools/acv-trg-launcher/dependencies.json); CMake скачивает и проверяет их. Внешние проекты сохраняют собственные лицензии.

Создайте конфигурацию только при её отсутствии:

```bash
test -f tools/native-rexglue/acv.config || \
  cp tools/native-rexglue/acv.config.example tools/native-rexglue/acv.config
```

Укажите имя пилота и gamertag в `[profile]`. При обновлении сохраняйте существующие настройки. Пример не заменяет личный конфиг и профиль запуска ACV-FSI.

## 4. Сборка в Linux

### 4.1 SDK: Release и x86-64-v2

Из корня репозитория:

```bash
node tools/decomp-logging/cli.mjs run --stage sdk-configure --timeout-ms 600000 -- \
  cmake --preset linux-amd64 -S tools/rexglue-sdk \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 \
  -DREXGLUE_USE_VULKAN=ON -DREXGLUE_USE_D3D12=OFF \
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF \
  -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_FIDELITYFX=OFF
node tools/decomp-logging/cli.mjs run --stage sdk-build --timeout-ms 1800000 -- \
  cmake --build tools/rexglue-sdk/out/build/linux-amd64 --config Release --parallel 4
node tools/decomp-logging/cli.mjs run --stage sdk-install --timeout-ms 600000 -- \
  cmake --install tools/rexglue-sdk/out/build/linux-amd64 --config Release \
  --prefix "$PWD/tmp/rex-sdk-install"
```

SDK использует multi-config preset с Debug по умолчанию. **Сохраняйте `--config Release` при сборке и установке.** Выше собирается базовый SDK без экспериментального FidelityFX; ACV-патчи этими командами не восстанавливаются. Отдельный [FFX-helper](tools/native-rexglue/build_sdk_ffx.py) — Linux-путь разработки, а не принятый кроссплатформенный рецепт.

### 4.2 TRG-лаунчер

Для лаунчера нужны закреплённый TRG checkout и заголовки `thirdparty/inja` из SDK. Generated translation игры не требуется:

```bash
node tools/decomp-logging/cli.mjs run --stage trg-configure --timeout-ms 600000 -- \
  cmake -S tools/acv-trg-launcher -B tmp/acv-trg-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2
node tools/decomp-logging/cli.mjs run --stage trg-build --timeout-ms 600000 -- \
  cmake --build tmp/acv-trg-build --target acv-launcher --parallel 4
```

В Ubuntu укажите `clang-18`/`clang++-18`, если имена без версии выбирают старый компилятор. Результат: `tmp/acv-trg-build/acv-launcher`. Интерфейс может открываться без игрового executable, но PLAY требует корректный runtime-пакет.

![Лаунчер, собранный из исходников в отдельной Linux-среде](docs/images/install-launcher-linux.jpg)

*Настоящая Linux-сборка из исходников: Xvfb, программный OpenGL и отдельный профиль. Игра для этого скриншота не запускалась.*

Компиляция из чистого checkout и этот снимок прошли в [workflow 40919963102](https://github.com/GrayRats/ArmoredcoreV-Port/actions/runs/40919963102): Ubuntu24.04/Clang18, Release, `-march=x86-64-v2`.

### 4.3 Пересборка игры: дополнительные входные файлы

Текущему incremental build нужны:

```text
tmp/acv-rexglue-linux-indexed/
  CMakeLists.txt
  acv_linux_probe_manifest.toml
  src/main.cpp
  generated/rexglue.cmake
  generated/conditional-fragments/...
tmp/acv-native-adjusting-thunks.cpp
<согласованные заголовки SDK с патчами, установленный CMake-package и runtime-библиотеки>
```

| Отсутствующий вход | Почему он нужен |
|---|---|
| Generated ACV C++, регистрация функций и PCH | В публичный архив не входит generated output |
| Manifest и entrypoint-проект | `build_linux.py` ожидает готовое дерево проекта |
| Adjusting thunks | Генератор также читает приватный registry и результаты сканирования; чистого clone недостаточно |
| Согласованный SDK с патчами | Отдельные патчи — ресурсы ремонта и свидетельства; единой принятой команды воспроизведения runtime из чистого clone пока нет |

Перед продолжением восстановите полное согласованное рабочее дерево. Не создавайте пустые заглушки и не подставляйте stock SDK молча. Входы описаны в [native-документе](docs/NATIVE_REXGLUE_LINUX.md), [baseline-хешах](tools/native-rexglue/RUNTIME_BASELINE.json) и [карте инструментов](TOOLS.MD).

Для **восстановленного рабочего дерева**:

```bash
cp tools/native-rexglue/CMakeLists.linux-probe.txt tmp/acv-rexglue-linux-indexed/CMakeLists.txt
node tools/decomp-logging/cli.mjs run --stage acv-configure --timeout-ms 600000 -- \
  cmake -S tmp/acv-rexglue-linux-indexed \
  -B tmp/acv-rexglue-linux-indexed/out/build/linux-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS=-march=x86-64-v2 \
  -DACV_PROJECT_ROOT="$PWD" -DCMAKE_PREFIX_PATH=/absolute/path/to/matching/patched/sdk
node tools/decomp-logging/cli.mjs run --stage acv-build --timeout-ms 1800000 -- \
  python3 tools/native-rexglue/build_linux.py --jobs 4 \
  --runtime-artifact tmp/runtime-inputs/librexruntime.so \
  --gpu-plugin-artifact tmp/runtime-inputs/librexgpu-xenos.so
```

Сначала положите проверенную пару библиотек в `tmp/runtime-inputs/`. Если runtime использует `libamd_fidelityfx_vk.so`, сохраните согласованный файл рядом с выходным ELF. Явные artifact-параметры принимают входы из workspace `tmp/` и проверяют ELF/хеши. Переносимый CMake-шаблон также принимает `ACV_ADJUSTING_THUNKS`, если файл проверенных thunks находится в другом месте. Пересобранная игра остаётся кандидатом до проверки.

<details>
<summary>Новый ReXGlue-проект: исследовательский пример, не принятая сборка ACV</summary>

У закреплённого CLI проверены следующие параметры:

```bash
node tools/decomp-logging/cli.mjs run --stage acv-init --timeout-ms 120000 -- \
  tmp/rex-sdk-install/bin/rexglue init --project-name acv_linux_probe \
  --xex-path /absolute/path/to/owned/default.xex \
  --game-root /absolute/path/to/owned/game --project-root tmp/acv-new-port
node tools/decomp-logging/cli.mjs run --stage acv-codegen --timeout-ms 1800000 -- \
  tmp/rex-sdk-install/bin/rexglue codegen tmp/acv-new-port/acv_linux_probe_manifest.toml
```

Проверьте фактическое имя manifest, созданное `init`. Для справки: схема manifest рабочего дерева ACV имеет следующий вид; замените пути своими:

```toml
[project]
name = "acv_linux_probe"
sdk_version = "0.10.0"
game_root = "/absolute/path/to/owned/game"

[entrypoint]
file_path = "/absolute/path/to/owned/default.xex"
out_directory_path = "generated/conditional-fragments"
includes = [
  "/absolute/path/to/ACV/tools/native-rexglue/acv-boundaries.toml",
  "/absolute/path/to/ACV/tools/native-rexglue/acv-conditional-targets.toml",
  "/absolute/path/to/ACV/tools/native-rexglue/acv-data-referenced-functions.toml",
]
```

Это пример схемы, а не полное воспроизведение принятого ACV-проекта. Анализу нужны эти boundary/config входы и дальнейшая работа над проектом. Обычный init/codegen не восстанавливает рабочее дерево. Не обходите validation вымышленными функциями и не делайте generated shards главным слоем ремонта.

</details>

## 5. Windows x64: SDK и состояние порта приложения

### 5.1 Developer PowerShell

Установите Visual Studio 2022 Build Tools с **Desktop development with C++** и Windows SDK, LLVM Clang18+, CMake3.25+, Ninja, Git, Python3.11+ и Node22+. Откройте **Developer PowerShell for VS2022** с окружением x64.

```powershell
git clone https://github.com/GrayRats/ArmoredcoreV-Port.git ACV
Set-Location ACV
clang++ --version
cmake --version
ninja --version
python --version
node --version
npm ci --prefix tools/decomp-logging
git clone https://github.com/rexglue/rexglue-sdk.git tools/rexglue-sdk
git -C tools/rexglue-sdk checkout --detach c94f5ebdcb3c9d1a460ca48e04f9758448f8d518
git -C tools/rexglue-sdk submodule update --init --recursive
```

Закреплённый preset использует драйвер **`clang`/`clang++`**. Это не предполагаемая конфигурация `clang-cl`. Окружение библиотек/заголовков Visual Studio должно оставаться доступным этому драйверу.

### 5.2 Компиляция и установка SDK

```powershell
node tools/decomp-logging/cli.mjs run --stage win-sdk-configure --timeout-ms 600000 -- cmake --preset win-amd64 -S tools/rexglue-sdk -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 -DREXGLUE_USE_D3D12=ON -DREXGLUE_USE_VULKAN=OFF -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_FIDELITYFX=OFF
node tools/decomp-logging/cli.mjs run --stage win-sdk-build --timeout-ms 1800000 -- cmake --build tools/rexglue-sdk/out/build/win-amd64 --config Release --parallel 4
node tools/decomp-logging/cli.mjs run --stage win-sdk-install --timeout-ms 600000 -- cmake --install tools/rexglue-sdk/out/build/win-amd64 --config Release --prefix "$PWD/tmp/rex-sdk-install-win"
Get-ChildItem tools/rexglue-sdk/out -Recurse -Filter rexruntime.dll
Get-ChildItem tmp/rex-sdk-install-win -Recurse -Filter *.dll
```

Это нативный Windows-путь SDK из закреплённого preset; для этого выпуска он на Windows не выполнялся. Vulkan-вариант выбирается через `REXGLUE_USE_VULKAN=ON` и требует соответствующих зависимостей. Рабочий профиль ACV FSI/full относится к Vulkan; возможность собрать D3D12-backend не доказывает корректный рендер ACV через него.

### 5.3 Что мешает нативному Windows-пакету ACV

| Текущий файл/слой | Что требуется до сборки Windows-игры |
|---|---|
| `acv_compat_shims.cpp` | Реализовать Windows-пути вместо POSIX `dlfcn.h`, `fcntl.h`, `unistd.h` |
| `tools/acv-trg-launcher/process.h` | Портировать `posix_spawn`, `poll`, группы процессов и сигналы |
| `acv_profiles.py` и Linux sessions | Портировать `fcntl.flock`, `/proc`, XDG и владение сессией |
| `standalone.py` | Заменить упаковку ELF/`.so` через `ldd` и Linux `start.sh` |
| Generated-проект и SDK с патчами | Восстановить те же дополнительные входы, что нужны для Linux-игры |

Проверенной команды сборки нативного ACV `.exe` и Windows-скриншота пока нет. WSL2 позволяет выполнить Linux-**сборку** с учётом CPU, драйвера и glibc; готовый Linux-пакет от этого не становится нативным Windows-приложением.

<details>
<summary>Пример упаковки Windows после реализации и независимой проверки нативного порта</summary>

После переноса POSIX-слоя и получения работающего executable соберите его реальные зависимости. Ниже шаблон, а не текущая сборка ACV:

```powershell
$VerifiedExe = "C:/verified-build/acv.exe"
$VerifiedDllDir = "C:/verified-build/runtime"
$PackageDir = "releases/ACV-Windows-candidate"
if (Test-Path $PackageDir) { throw "Выберите новый каталог вывода" }
New-Item -ItemType Directory -Path $PackageDir | Out-Null
Copy-Item $VerifiedExe $PackageDir
Copy-Item "$VerifiedDllDir/*.dll" $PackageDir
dumpbin /DEPENDENTS "$PackageDir/acv.exe"
Get-ChildItem $PackageDir -File | Get-FileHash -Algorithm SHA256 | Format-Table -AutoSize | Out-String | Set-Content "$PackageDir/SHA256.txt"
Compress-Archive -Path "$PackageDir/*" -DestinationPath "$PackageDir.zip"
```

Включите лицензии зависимостей, реализованный backend настроек/профилей, разрешённый к распространению VC runtime при необходимости и проверенный путь установки данных. Переименование Linux `.so` в DLL не переносит библиотеку. До объявления поддержки проверьте пакет на чистой Windows-системе.

</details>

## 6. Создание Linux-пакета

Нужны собранный TRG-лаунчер, **согласованный проверенный** набор `acv_linux_probe`, `librexruntime.so`, `librexgpu-xenos.so` и, при необходимости, `libamd_fidelityfx_vk.so`, лицензии зависимостей и canonical `acv.config`.

Из корня репозитория, пример Ubuntu:

```bash
node tools/decomp-logging/cli.mjs run --stage acv-package --timeout-ms 600000 -- \
  python3 tools/acv-launcher/standalone.py --frontend trg \
  --launcher tmp/acv-trg-build/acv-launcher \
  --runtime-dir /absolute/path/to/verified/runtime \
  --font-dir /usr/share/fonts/truetype/dejavu \
  --font-license /usr/share/doc/fonts-dejavu-core/copyright \
  --skip-settings-tests --out tmp/ACV-Linux-candidate
node tools/decomp-logging/cli.mjs run --stage acv-package-verify --timeout-ms 600000 -- \
  python3 tools/acv-trg-launcher/verify_package.py tmp/ACV-Linux-candidate
```

Для Arch используются `/usr/share/fonts/TTF` и `/usr/share/licenses/ttf-dejavu/LICENSE`; эти значения сохранены по умолчанию. `--skip-settings-tests` пропускает settings test file, включая effect checks, и записывает пропуск. Остальные проверки упаковки продолжают выполняться. Приёмки эффектов CAS/FSR в проекте пока нет.

Каталог назначения должен быть новым. Упаковка выполняется через staging с проверкой хешей и библиотек перед публикацией. Существующий вывод не удаляется. Runtime с отличающимися от `RUNTIME_BASELINE.json` хешами отмечается кандидатом.

### Включение локально извлечённых данных

Флаг `--include-game-data` копирует `tmp/iso_root/` в `data/game/`; необязательные `tmp/acv-dlc-sound/` и `tmp/acv-dlc-image/` включаются при наличии. Подготовьте эти каталоги из собственных входных файлов и сверьте SHA-256 XEX, требуемый упаковщиком. Для runtime-пакета меньшего размера не передавайте флаг и используйте существующий backend установки данных отдельно.

Оригинальные ISO/XEX сохраняйте. Для артефактов с игровыми данными храните provenance и применимое основание распространения; внешние ресурсы сохраняют свои лицензии.

### Архивирование и деление на части

```bash
tar --exclude='*/__pycache__' --exclude='*.pyc' --zstd -cf \
  tmp/ACV-Linux-candidate.tar.zst -C tmp ACV-Linux-candidate
zstd -t tmp/ACV-Linux-candidate.tar.zst
split -b 1700M -d -a 2 tmp/ACV-Linux-candidate.tar.zst tmp/ACV-Linux-candidate.tar.zst.part-
cd tmp
sha256sum ACV-Linux-candidate.tar.zst.part-* > ACV-Linux-candidate.parts.sha256
```

[GitHub Releases ограничивает один загружаемый файл размером менее2ГиБ](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases). Прикладывайте части, хеши, манифест/provenance и инструкцию объединения. Успех проверки пакета подтверждает выполненные проверки, а не gameplay/FPS parity.

## 7. GitHub Packages и окружение сборки

[Workflow Build tools package](.github/workflows/build-tools.yml) собирает образ Linux/amd64, компилирует лаунчер с x86-64-v2, сохраняет его окно в Xvfb и публикует окружение в **GitHub Container Registry**:

```bash
docker pull ghcr.io/grayrats/acv-build-tools:latest
docker run --rm -it --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
  ghcr.io/grayrats/acv-build-tools:latest bash
```

Для воспроизводимости используйте image digest из workflow log вместо изменяемого тега `latest`. В образе есть инструменты сборки, но нет игровых данных ACV, generated translation игры и драйверов GPU. Для приватного package нужна авторизация GHCR; публичная видимость задаётся отдельно владельцем.

Публикация и анонимное получение проверены 29.10.2026. Образ указанного workflow:

```text
ghcr.io/grayrats/acv-build-tools@sha256:e3b8cea14375b4bdac6c5bac3553db2ca6c526c403091b8eeba559b53589ce27
```

Если GHCR недоступен, соберите окружение локально:

```bash
docker build --platform linux/amd64 -f tools/build-tools/Dockerfile -t acv-build-tools .
docker run --rm -it --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work acv-build-tools bash
```

GitHub Packages обслуживает определённые реестры, в том числе OCI-контейнеры, а не произвольные `.tar.zst`. Архивы приложения остаются во [вложениях релизов](https://github.com/GrayRats/ArmoredcoreV-Port/releases). Подробнее: [форматы GitHub Packages](https://docs.github.com/en/packages/learn-github-packages/introduction-to-github-packages).

## 8. Решение проблем

| Симптом | Что проверить |
|---|---|
| Не запускается `better-sqlite3`/логгер | Node22+, `npm ci --prefix tools/decomp-logging`, совместимый Node ABI и инструменты компиляции |
| SDK требует Clang | Выбрать `clang`/`clang++` вместо GCC; проверить версию18+ |
| SDK оказался Debug | Для multi-config build/install нужен `--config Release` |
| Нет `generated/rexglue.cmake` или thunks | Восстановить полное рабочее дерево: публичный архив его не содержит |
| Не совпала ревизия TRG/SDL/ImGui | Восстановить точные hashes из `dependencies.json`; не отключать CMake-проверку |
| Упаковщик не нашёл шрифты/лицензию | Передать `--font-dir` и `--font-license` для своего дистрибутива |
| `GLIBC_*`/`GLIBCXX_* not found` | Сравнить требования бинарника и ABI системы; пересобрать весь согласованный набор под целевую систему |
| Нет DLL в Windows | Проверить реальные зависимости `.exe`/DLL; Linux `.so` не являются Windows-библиотеками |
| Windows не находит `fcntl`/`spawn.h` | Не перенесён слой приложения ACV; одного CMake-флага недостаточно |
| Выходной каталог пакета существует | Выбрать новый путь, сохранив прежний checkpoint |

Команды сверены с закреплёнными исходниками/CLI. Полная нативная компиляция ACV под Windows и воспроизведение принятого игрового runtime из чистого clone пока не проверены. Это отдельные ограничения от рабочей Linux-сборки лаунчера и существующего бинарного пакета.
