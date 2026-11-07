---
name: acv-project-tools
description: Выбор инструментов DEArmoredcore для GUI, извлечения XISO/STFS, ReXGlue workflow и анализа бинарников. Читать перед соответствующей задачей; не запускать все инструменты подряд.
---

# Инструменты проекта

Это проектная инструкция в формате skill. Точка входа — `AGENTS.md`;
файл не является отдельно установленным глобальным skill.
Все пути ниже отсчитываются от корня DEArmoredcore.

## Выбор по задаче

| Задача | Использовать | Что прочитать / проверить |
|---|---|---|
| Новый GUI или замена Qt | `tools/trg-launcher/` | README, `tools/acv-context/docs/INTEGRATION.md`, `include/trg/`; контракт ACV в [PLAN_GUI](kb:PLAN_GUI) |
| Извлечение XISO в каталог | `tools/extract-xiso/` | README и справка фактического бинарника; исходники не означают наличие готовой сборки |
| Xbox 360, STFS/LIVE/PIRS/CON, ReXGlue/codegen/runtime | [tools/360tools-skill/SKILL.md](tools/360tools-skill/SKILL.md) | Сначала router навыка, затем только нужные resources; исполняемые инструменты искать в `tools/360toolsUpdated/` |
| Дополнительная проверка Xbox-образа старым GUI | `tools/c-xbox_tool_2.0.7/` | `Readme.md`, фактическая поддержка формата и платформы; Windows `.exe`, не основной Linux pipeline |
| Просмотр XDVDFS, выборочное извлечение, независимая проверка содержимого | [antangelo/xdvdfs](https://github.com/antangelo/xdvdfs) | Upstream README и справка выбранной версии CLI; предполагаемый checkout `tools/xdvdfs/` |
| Заголовки, секции, символы и дизассемблирование бинарников | `tools/binutils` | Проверить существование, состав и target каждой утилиты; не предполагать поддержку Xenon PPC |
| Примеры организации ReXGlue-порта и input/camera patches | `tools/Armored-core-ReAnswered/` | CMake, `src/main.cpp`, `src/camera_trace_patch.cpp`, `src/rmb_patch.cpp`; это Armored Core: For Answer, другая игра |

## Порядок применения

1. Определить вход и требуемый результат: GUI, образ XISO, контейнер STFS,
   XEX, извлечённый код или host ELF. Выбрать соответствующую строку таблицы.
2. Проверить локальный путь, версию/commit, README и поддерживаемые параметры.
   Отсутствующий инструмент явно отметить; использовать подходящий доступный
   инструмент либо подготовить его установку в рамках текущей задачи.
3. Запускать команды анализа, сборки и проверки через журнал проекта:
   `node tools/decomp-logging/cli.mjs run --stage STAGE -- COMMAND ARG...`.
   Соблюдать [AGENTS](kb:AGENTS); наличие инструмента не разрешает новые
   runtime-запуски или изменение защищённых данных.
4. Проверить результат на нужной границе и записать путь/версию инструмента,
   вход, выход, код завершения и ограничение проверки. При ошибке изучить
   журнал; повторять с исправленной причиной, а не идентичной командой.

## GUI: trg-launcher

Использовать как основу дальнейшего GUI вместо Qt. Подключать ACV frontend
к существующим профилям и настройкам через адаптер; сохранять UUID/XUID,
блокировки, backup/restore и атомарную запись. Для standalone использовать
SDL2/OpenGL окно TRG; игровой Vulkan runtime остаётся отдельным процессом.
Ревизию и требования Dear ImGui сверять с локальным CMake.
Принимать замену по критериям PLAN_GUI, а не по успешному запуску upstream demo.

## Образы: extract-xiso, xdvdfs, C-Xbox Tool

- Сначала использовать уже извлечённый файл/индекс, если он отвечает на вопрос.
- Для полного извлечения XISO выбирать extract-xiso: режим `-x`, отдельное
  назначение `-d` после проверки справки; оригинал не переписывать через `-r`.
- Для XDVDFS выбирать xdvdfs, когда нужны `ls`, `tree`, `info`, `copy-out`,
  `unpack` или проверка `checksum`. Синтаксис аргументов уточнять через справку
  подкоманды. Возможности перечислены в [upstream README](https://github.com/antangelo/xdvdfs#xdvdfs-cli).
- C-Xbox Tool использовать для подходящего ему формата и явной задачи на этот
  инструмент. Реальный путь — латинская `c`, без завершающего `~`:
  `tools/c-xbox_tool_2.0.7/`. Windows/Wine требуют отдельного запроса по AGENTS.
- ISO/assets read-only; результат — отдельный staging-каталог. Проверить список
  файлов, размеры и хеши нужных файлов; при сравнении двух extractor использовать
  одинаковый вход. Существующие игровые данные не перезаписывать при сбое.
- STFS не считать XISO: выбирать соответствующий путь из 360tools-skill.

## Бинарники: tools/binutils

Перед использованием найти реальные `objdump`, `readelf`, `nm`, `objcopy`
и проверить version/target (например, список целей `objdump -i`).
Для host ELF применять host-инструменты; для Xenon — инструмент с поддержкой
нужного PPC и big-endian. XEX не считать автоматически поддерживаемым ELF:
использовать существующий путь извлечения кода и карту секций/адресов.
Для raw code явно задавать архитектуру, endian и базовый адрес по проверенным
данным. Не смешивать file offset, guest VA и host VA. Преобразования делать
в копии; вывод objdump не заменяет проверку XEX/PPC через соответствующие навыки.

## ReAnswered и воспроизводимость

При доработках ReXGlue/input/camera сверяться с ReAnswered до написания аналогичного
решения. Переносить идею только после проверки на ACV; чужие адреса, hooks и
конфиги не копировать как готовое исправление. Проверять лицензию перед переносом
кода, сохранять атрибуцию.

URL и точные ревизии трёх загруженных checkout:
[tools/external-references.json](tools/external-references.json).
Для восстановления клонировать URL в указанный каталог и выполнить
`git checkout <commit>`; обновлять ревизии явно. Внешние checkout сохраняют
свои лицензии и исключаются из публикации исходников проекта.

Проверено 2026-10-25: trg-launcher, extract-xiso, 360tools-skill,
360toolsUpdated, c-xbox_tool_2.0.7 и ReAnswered присутствуют локально.
`tools/xdvdfs/` и `tools/binutils` отсутствуют; наличие бинарников в PATH здесь
не проверялось. Этот статус не заменяет проверку доступности перед применением.

## Контейнеры и хэши FromSoftware (внешние репозитории)

Не вендорить в репозиторий: это внешние checkout со своими лицензиями. Перед
переносом кода проверить лицензию и сохранить атрибуцию. Существование и описание
репозиториев проверено через GitHub 2026-11-05; точные ревизии не фиксировались.

### YabberExtended

- Репозиторий: [WarpZephyr/YabberExtended](https://github.com/WarpZephyr/YabberExtended), лицензия GPL-3.0.
- Назначение: распаковка и упаковка контейнеров FromSoftware (`.bnd`, `.bhd`/`.bdt`, `.tpf` и др.).
- Применение: распаковка и модификация архивов Armored Core (AC4, ACFA, ACV/Verdict Day), Dark Souls, Bloodborne, Sekiro.
- Перед применением: читать README и справку фактической сборки; не выполнять записи в оригинальные архивы игры, работать с копией в staging-каталоге.

### FromSoftwareHashTool

- Репозиторий: [WarpZephyr/FromSoftwareHashTool](https://github.com/WarpZephyr/FromSoftwareHashTool).
- Назначение: получение хэшей имён файлов FromSoftware из строк (для сопоставления путей с хэш-идентификаторами в `dvdbnd` и `bhd5`).
- Применение: поиск имён файлов, которых нет в известном списке, по строкам из дампа или бинарника; результат — кандидаты, требующие проверки на реальном архиве.

### ACVLooseLoader

- Репозиторий: [WarpZephyr/ACVLooseLoader](https://github.com/WarpZephyr/ACVLooseLoader), лицензия MIT.
- Назначение: автоматическая loose-загрузка (чтение распакованных файлов вместо архивов) для Armored Core V и Verdict Day.
- Применение: эталон механизма loose loading при подготовке ReXGlue VFS. Задача интеграции — в [PLAN.LooseLoader.MD](PLAN.LooseLoader.MD).
- Ограничение: последний push в репозиторий — 2024-07-25; совместимость с текущим ACV-Linux build проверять отдельно.

## Внутренние инструменты dvdbnd / BHD5 / BND3 (уже в репозитории)

Перед внешним YabberExtended сначала проверить эти скрипты: они уже покрывают
распаковку архивов ACV и проверку результата. Все работают только на чтение входа,
вывод пишут в workspace.

- `tools/bhd5_index.py` — разбор индекса BHD5 dvdbnd; общие `parse()` и `decode_dcx_dflt()`.
- `tools/bhd5_extract_all.py` — распаковка всех записей BHD5 с декодированием DCX/DCP DFLT. Имена файлов — 32-битные хэши путей; исходные имена не восстанавливаются. Манифест записывает ошибки.
- `tools/acv_loose_unpack.py` — распаковка dvdbnd5.bhd/dvdbnd.bdt в loose-файлы по словарю имён (хэш пути 32-bit, lowercase, ведущий `/`). Пишет исходные байты (DCX не распаковывается), неизвестные записи — в `_unknown/<hash>`. Заменяет шаг main archive в ACVLooseLoader: его BHD5-читатель не извлёк ни одного файла из этого Xbox 360 варианта.
- `tools/acv_loose_resolve_unknown.py` — переносит `_unknown/<hash>` в реальные имена по путям из `NtCreateFile FAILED` в логах запуска (проверка по хэшу).
- `tools/bnd3_batch.py`, `tools/bnd3_extract_dir.py` — распаковка BND3-контейнеров (второй — из уже декодированных `.bnd3`).
- `tools/verify_dvdbnd_extraction.py` — независимая проверка вывода `bhd5_extract_all.py`: коллизии имён, хэши по манифесту, сравнение raw-записей с BDT побайтово.
- `tools/tpf_x360.py`, `tools/tpf_dvdbnd_batch.py` — TPF-текстуры Xbox 360 из ACV BDT.
- `tools/xdvdfs_list.py` — только чтение листинга XDVDFS-образа, сверка с каталогом по SHA-256.
- `tools/native-rexglue/verify_resource_io.py` — проверка захваченных VFS-чтений против срезов исходных файлов (только чтение). Не доказывает декомпрессию в гостевой памяти.
- `tools/native-rexglue/test_dlc_image_mount.py` — проверка VFS-пакета против retail BHD5/BDT.

- `tools/llmignore.py` — проверка `.llmignore` (`check`, `list`, `tracked`) и генерация `.ignore` для ripgrep (`sync`).

- `tools/acv-context/` — плагин Claude Code: FTS5-поиск по Markdown, дайджест логов, сборщик контекста с бюджетами 15/60/25 (MCP `ctx_*`). См. [CONTEXT_PLUGIN](kb:CONTEXT_PLUGIN_20261105).

## Среда выполнения (проверено 2026-11-05)

- .NET: runtime Microsoft.NETCore.App 8.0.31 и 10.0.12, SDK 8.0.131 и 10.0.112 (`/usr/share/dotnet`). Нужен для ACVLooseLoader и YabberExtended, если они собираются из исходников.
- `sha256sum`, `jq`, `7z` доступны в PATH.
- NuGet/`dotnet restore` в этом окружении зависает из-за нерабочего IPv6-маршрута (curl -6 не отвечает, `curl -4` работает). Обход: `export DOTNET_SYSTEM_NET_DISABLEIPV6=1`. Python `urllib` страдает так же.
- Сборка ACVLooseLoader: нужны соседние клоны `BinderHandler`, `SoulsFormatsExtended`, `BinaryMemory`, `libps3` (коммит `75b2b8c`, последний с `EDAT.DecryptSdatFile`), `Edoke`. Линукс-правки лоадера — в `tools/acv-loose-loader-linux.patch` (путь без обрезки `/`, блокировка консоли только на Windows).
- GPU фактически NVIDIA GeForce GTX 1660 SUPER (TU116, драйвер nvidia; `nvidia-smi`, Vulkan device), а не RX 6700 XT из CLAUDE.md.
- Ghidra: `/usr/bin/ghidra`; обёртка проекта — `scripts/ghidra-rpc.sh`. Для задачи loose loading не требуется, нужна только для анализа кода.
