# DEArmoredcoreV

[English](README.md) | [Русский](README.RU.MD)

Исследовательский проект: реконструкция и нативная Linux-сборка **Armored Core V** (Xbox 360).
Машинный код PowerPC из `default.xex` переводится в C++ через ReXGlue SDK и запускается на Linux;
параллельно ведётся независимая C/C++ реконструкция поведения по retail-данным.

*Static recompilation of Armored Core V (Xbox 360) into a native Linux port. Research project; no game data included.*

![Сцена из игры на нативной Linux-сборке с Vulkan-рендером](docs/images/gameplay-vulkan.jpg)


Репозиторий: <https://github.com/GrayRats/DEArmoredcoreV>

> **Важно.** В репозитории нет игры: ни ISO, ни XEX, ни распакованных ресурсов, ни DLC.
> Для работы нужен собственный legally obtained образ игры. Права на Armored Core V принадлежат
> их правообладателям; проект не распространяет и не должен распространять игровые данные.

> **Не аффилировано.** Проект не связан с правообладателями Armored Core V и не одобрен ими.
> Названия и торговые марки принадлежат их владельцам.

## Возможности

- **Статическая рекомпиляция.** Инструкции PowerPC из `default.xex` переводятся в C++ и собираются
  как нативная программа для Linux; эмуляция CPU в рантайме не используется.
- **Рендер через Vulkan.** GPU-плагин Xenos транслирует шейдеры в SPIR-V (glslang); отладка кадров
  ведётся через RenderDoc replay-инструменты.
- **Распаковка ресурсов.** Скрипты для BHD5/BDT, BND3, FLVER, TPF и текстур Xbox 360 (BC1) в `tools/`.
- **Прозрачный журнал.** Каждый запуск анализа и сборки пишется в SQLite-логгер (`tools/decomp-logging`),
  поэтому результаты воспроизводимы и проверяемы.
- **Независимая реконструкция.** Параллельная C/C++ реконструкция поведения по retail-данным;
  доказательства по адресам и функциям перечислены в [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md).

## Текущий статус

[Примечания к выпуску ACV-Linux 1.1](RELEASE_NOTES_1.1.md) описывают лаунчер и пакет на русском и английском. В релиз включены исходники/файлы сборки и части архива существующего пакета `ACV-Linux-release-1.0`; точную версию, provenance и порядок объединения частей см. в примечаниях.

Текущий отдельный пакет — [ACV-Linux-release-1.0](../ACV-Linux-release-1.0/start.sh). TRG-лаунчер, backend профилей/настроек и файлы игры/runtime проверены как локальный Linux-пакет. В лаунчере доступны параметры экрана и графики, XDG-профили, backup/restore, управление, лимиты кэша текстур и ограниченная подгрузка архивов.

В пользовательской сессии release1.0 игра работала дольше184с. За60с замера получено29,02 guest swap/с, загрузка GPU по устройству92,4%, потерянных CPU IP samples нет. Миссия и маршрут не указаны, поэтому это ещё не воспроизводимый игровой baseline. FPS выше30, 1% low, корректность изображения и темп игрового времени не приняты. Подробности — в [отчёте FPS](report-fps.md).

Чёрный экран/зависание в миссии2 наблюдались в более ранней сессии; для этого сценария проблема остаётся открытой ([свидетельства](docs/PROGRESS.md#native-linux), [анализ](docs/NATIVE_REXGLUE_LINUX.md#error-mission-2-freeze-on-the-accepted-build-the-instrumented-waits-did-not-fire-2026-10-24)). Неизвестно, относится ли он к более позднему замеру без названия миссии.

В ресурсной части проверены форматы BHD5/BDT и BND3, разобраны заголовки7 111 FLVER, сопоставлены блоки Xbox360 BC1. DLC-звуки смонтированы частично; эмблемы и семь звуков требуют образа с DLC. Открыты эмуляция pixel shader, покрытие readback в игре и поведение kernel-функций `ObReferenceObject` и `IoDismountVolumeByFileHandle`.

Подробный план, приёмка и журнал: [docs/PLAN.md](docs/PLAN.md), [docs/PROGRESS.md](docs/PROGRESS.md).

## Структура

| Путь | Что внутри |
|---|---|
| `tools/native-rexglue/` | хост-шимы ядра (`acv_compat_shims.cpp`), probe и build-скрипты (`probe_linux.py`, `build_linux.py`), RenderDoc replay-инструменты, тесты |
| `tools/decomp-logging/` | SQLite-логгер выполнения (`cli.mjs`): все запуски анализа и сборки проходят через него |
| `tools/native-decomp/`, `tools/native-host-probe/` | ограниченные ручные модели и проверки по retail-доказательствам |
| `tools/*.py` | распаковка и проверка ресурсов: `bhd5_extract_all.py`, `bnd3_extract_dir.py`, `flver_inventory.py`, `x360_bc_decode.py`, `bc_tile_match.py`, `spirv_slice.py` и др. |
| `scripts/` | обёртки: `assets.sh`, `decomp.sh`, `ghidra-rpc.sh`, `unity.sh`, `check_repository_payload.py` |
| `docs/` | состояние, план, журнал, форматы и технические отчёты |
| `logs/` | не публикуется, кроме отдельных доказательных файлов, перечисленных в `.gitignore` |

Внешние компоненты (ReXGlue SDK, XenonRecomp, XenosRecomp, c-xbox_tool_2.0.7 и др.) в репозиторий
не входят: они ставятся отдельно и имеют собственные лицензии в своих каталогах.

## Требования

- Linux x86_64 (разработка ведётся на CachyOS/Arch), Clang или GCC, CMake, Ninja.
- ReXGlue SDK 0.10.0, установленный отдельно; путь задаётся через `CMAKE_PREFIX_PATH` (см.
  [tools/native-rexglue/gpu-plugin/README.md](tools/native-rexglue/gpu-plugin/README.md)).
- Python 3 и Node.js (для логгера `tools/decomp-logging`).
- RenderDoc — для захватов кадров; Ghidra — опционально, через `scripts/ghidra-rpc.sh`.
- Собственный образ игры. Распаковка описана в [docs/ASSET_FORMATS.md](docs/ASSET_FORMATS.md).

## Быстрый старт

```bash
# 1. Локальный профиль (не публикуется; содержит имя игрока и настройки)
cp tools/native-rexglue/acv.config.example tools/native-rexglue/acv.config

# 2. Сборка нативного probe (инкрементальная)
python3 tools/native-rexglue/build_linux.py --jobs 4 \
  --gpu-plugin-artifact tmp/acv-gpu-plugin-build/librexgpu-xenos.so

# 3. Ограниченный запуск (150 с) через логгер
node tools/decomp-logging/cli.mjs run --stage linux_probe --timeout-ms 175000 -- \
  python3 tools/native-rexglue/probe_linux.py --seconds 150 --video-driver x11 \
  --user-root tmp/acv-mnk-profile
```

Запуск — диагностический, а не игровой лаунчер. Статус `failed` у проб из-за шума `dlc_*` в
классификаторе означает ложную классификацию при `child exit 0`; см. [docs/SQLITE_LOGGING.md](docs/SQLITE_LOGGING.md).

Все анализ, сборка и тесты должны запускаться через логгер:

```bash
node tools/decomp-logging/cli.mjs run --stage <имя_этапа> -- <команда> [аргументы]
```

## Документация

| Документ | Назначение |
|---|---|
| [docs/README.md](docs/README.md) | Навигация: какой документ читать для какого вопроса |
| [docs/PLAN.md](docs/PLAN.md) | Единый план, правила работы и критерии приёмки |
| [docs/PROGRESS.md](docs/PROGRESS.md) | Журнал проверенных результатов и операций |
| [docs/X360_PROJECT_STATE.md](docs/X360_PROJECT_STATE.md) | Текущее состояние и блокеры |
| [docs/NATIVE_REXGLUE_LINUX.md](docs/NATIVE_REXGLUE_LINUX.md) | Нативная Linux-линия: сборка, импорты, эксперименты |
| [docs/ASSET_FORMATS.md](docs/ASSET_FORMATS.md) | ISO, BHD5/BDT, DCX, BND3, TPF, FLVER, текстуры |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Слои и границы runtime |
| [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md) | Доказательства по адресам и функциям |
| [docs/LINUX_INPUT.md](docs/LINUX_INPUT.md) | Клавиатура, мышь, конфиг |
| [docs/SQLITE_LOGGING.md](docs/SQLITE_LOGGING.md) | Логгер: команды, история, повторы |
| [docs/ACV_TIMING.md](docs/ACV_TIMING.md), [docs/GHIDRA_CAPTURE.md](docs/GHIDRA_CAPTURE.md) | Тайминг и снимки Ghidra |
| [docs/fine/](docs/fine/) | Сжатые отчёты по отложенным веткам; архивы исключены |
| [ABOUT_DE-AC5.MD](ABOUT_DE-AC5.MD) | Обзор процесса: от ISO до C++ и runtime |
| [TASK-SQL.MD](TASK-SQL.MD) | Политика постоянного журнала и восстановления |

## Для агентов и участников

- [AGENTS.md](AGENTS.md) — правила работы для ИИ-агентов и участников: что проверять, что нельзя
  менять без разрешения, как фиксировать результат.
- Перед изменениями читать [docs/PLAN.md](docs/PLAN.md), затем последние записи в
  [docs/PROGRESS.md](docs/PROGRESS.md).
- Оригинальный ISO и живая база Ghidra — только для чтения.
- Скриншоты и PNG-превью для анализа не сохраняются: доказательства — логи, RenderDoc RDC/XML,
  инструкции и числовые дампы.

## Лицензия

Код и документация проекта распространяются по **GNU General Public License v3.0 (only)** —
см. [LICENSE](LICENSE). Сторонние компоненты (ReXGlue SDK, XenonRecomp, XenosRecomp, RenderDoc,
библиотеки в `thirdparty/`) сохраняют свои лицензии; GPL-3.0 их не заменяет.
Игровые данные Armored Core V в лицензию проекта не входят и не распространяются.
