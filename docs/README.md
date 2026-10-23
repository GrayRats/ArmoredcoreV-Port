# ACV docs — быстрый выбор документа

Для продолжения: [STATE](X360_PROJECT_STATE.md) → нужный раздел [PLAN](PLAN.md).
Не читать все файлы подряд. Рабочая платформа Linux. Статус текущего запуска — в STATE; только изменение
docs не является разрешением возобновить остановленную runtime-работу.

| Вопрос | Источник истины | Когда читать |
|---|---|---|
| Что делать дальше, правила/приёмка | [PLAN](PLAN.md) | При выборе открытой задачи |
| Где остановились | [STATE](X360_PROJECT_STATE.md) | В начале сессии; это не вторая очередь |
| Что реально проверено | [PROGRESS](PROGRESS.md) | Перед повтором этапа/заявлением результата |
| Timing, buffers, DrawThread, lookup | [ACV_TIMING](ACV_TIMING.md) | Для соответствующих guest VA/полей |
| Runtime boundaries, слои | [ARCHITECTURE](ARCHITECTURE.md) | Перед архитектурным изменением; исторические даты учитывать |
| Native Linux build/imports | [NATIVE_REXGLUE_LINUX](NATIVE_REXGLUE_LINUX.md) | Для воспроизведения этой линии |
| Keyboard/mouse/config | [LINUX_INPUT](LINUX_INPUT.md) | Перед input/Setup изменениями |
| ISO/BDT/BHD/TPF/DCX | [ASSET_FORMATS](ASSET_FORMATS.md), [RE log](REVERSE_ENGINEERING.md) | Только нужный формат/адрес |
| Ghidra snapshot/integrity | [GHIDRA_CAPTURE](GHIDRA_CAPTURE.md) | Перед bounded capture |
| SQLite commands/history/retry | [SQLITE_LOGGING](SQLITE_LOGGING.md), [TASK-SQL](../TASK-SQL.MD) | Перед logger changes/recovery |

## Vulkan API и Ghidra

При Vulkan использовать [implementation/API skill](/home/art/.codex/skills/vulkan/SKILL.md)
и [локальные Khronos references](/home/art/.agents/skills/vulkan/SKILL.md).
При PPC/XEX analysis использовать [Ghidra RPC skill](/home/art/.agents/skills/ghidra-rpc/SKILL.md):
проверить доступность MCP; fallback — scripts/ghidra-rpc.sh, правильный проект/
бинарник и raw instructions при неполной базе. Доступность не предполагать,
живую DB не чинить без отдельного разрешения. [Правила](../AGENTS.md).

## Поиск и обновление

1. Взять один открытый пункт PLAN; проверить relevant SQLite task/errors и evidence.
2. Искать адрес/символ через rg в выбранном файле/SDK/индексе, затем расширять поиск.
3. Уточнить одно утверждение, проверить его подходящим инструментом и записать
   источник/уверенность/ограничение. Гипотеза не становится фактом от повторения.
4. Обновить соответствующий technical section и PROGRESS milestone, а не
   добавлять копии одной истории в каждый файл. STATE держать коротким.

Архив fine/ не включать в обычный поиск/оптимизацию; открывать только при явной
ссылке или для конкретной отложенной ветки. Сырые SQL output и log files читать
точечно. Исходные версии текущего сжатия: [snapshot](../logs/docs-context-20261022/manifest.json).

Проверка активных ссылок и отсутствия прежних дублирующих файлов (read-only;
не обходит fine/, не проверяет удалённые URL и HTML anchors):

```bash
node tools/decomp-logging/cli.mjs run --stage docs_check -- python3 tools/check_active_docs.py
```
