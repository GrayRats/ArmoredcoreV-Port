# DEArmoredcoreV

Исследовательский проект: реконструкция и нативная Linux-сборка **Armored Core V** (Xbox 360).
Машинный код PowerPC из `default.xex` переводится в C++ через ReXGlue SDK и запускается на Linux;
параллельно ведётся независимая C/C++ реконструкция поведения по retail-данным.

Репозиторий: <https://github.com/GrayRats/DEArmoredcoreV>

> **Важно.** В репозитории нет игры: ни ISO, ни XEX, ни распакованных ресурсов, ни DLC.
> Для работы нужен собственный legally obtained образ игры. Права на Armored Core V принадлежат
> их правообладателям; проект не распространяет и не должен распространять игровые данные.

## Текущий статус

Что подтверждено (детали и операции проверки — в [docs/PROGRESS.md](docs/PROGRESS.md)):

- Нативная сборка собирается и запускается до меню и интро; тёмный кадр геймплея воспроизводится,
  его причина **не установлена**. Правильный геймплей ещё не принят.
- Ресурсы: распакованы все 19 744 записи BHD5, BND3 (включая формат 0x2c), 7 111 FLVER-заголовков;
  раскладка BC1 Xbox 360 подтверждена точным совпадением блоков.
- Логи игры: `dlc_sound:` частично смонтирован из данных образа (75 открытий успешны); эмблемы и 7 звуков
  требуют образа с DLC, которого в репозитории нет.
- Открыто: эмуляция пиксельного шейдера, проверка readback в игре, заглушки ядра `ObReferenceObject` и
  `IoDismountVolumeByFileHandle`.

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
