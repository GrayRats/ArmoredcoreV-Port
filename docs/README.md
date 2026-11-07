# ACV docs — быстрый выбор документа

Для продолжения: [STATE](kb:X360_PROJECT_STATE) → очередь реализации
[PLAN_GUI](kb:PLAN_GUI); краткий статус и технические документы — ниже.
Не читать все файлы подряд. Рабочая платформа Linux. Статус текущего запуска — в STATE; только изменение
docs не является разрешением возобновить остановленную runtime-работу.

| Вопрос | Источник истины | Когда читать |
|---|---|---|
| План Vulkan для Windows launcher и игры | [PLAN_WINDOWS](kb:PLAN_WINDOWS) | Windows Vulkan SDK, Win32 приложение, лаунчер и acceptance matrix |
| Сборка Linux v2.1 и её границы | [ACV_LINUX_RELEASE_2_1](kb:ACV_LINUX_RELEASE_2_1_20261104) | Компиляция, пакетная проверка и runtime/gameplay acceptance |
| Очередь реализации и критерии | [PLAN_GUI](kb:PLAN_GUI) | При выборе следующего этапа |
| Текущий статус native линии | [PLAN](kb:PLAN) | Быстрый обзор и ограничения |
| Где остановились | [STATE](kb:X360_PROJECT_STATE) | В начале сессии; это не вторая очередь |
| Что реально проверено | [PROGRESS](kb:PROGRESS) | Перед повтором этапа/заявлением результата |
| Все текущие изменения FPS/readback и остаток | [FPS_READBACK_PROGRESS_20261027](kb:FPS_READBACK_PROGRESS_20261027) | Проверенные результаты, кандидаты, подготовленное сравнение и следующий шаг |
| Live memcpy и forward128 кандидат | [FPS_FORWARD_COPY_20261029](kb:FPS_FORWARD_COPY_20261029) | Точный caller/size анализ текущей игры, opt-in CPU copy, host результаты и границы |
| Семь реализованных Vulkan путей | [FPS_VULKAN_SEVEN_20261104](kb:FPS_VULKAN_SEVEN_20261104) | Реализация, предыдущие native/GPU проверки и включение в v2.1 |
| Render-target pipelines в persistent cache | [FPS_VULKAN_RENDER_TARGET_CACHE_20261104](kb:FPS_VULKAN_RENDER_TARGET_CACHE_20261104) | Дополнительные lazy pipelines; включены в v2.1, gameplay/GPU acceptance открыты |
| Исходный аудит Vulkan оптимизаций | [VULKAN_OPTIMIZATION_OPTIONS_20261104](kb:VULKAN_OPTIMIZATION_OPTIONS) | Семь source-backed направлений,риски и acceptance |
| Descriptor binds | [FPS_DESCRIPTOR_BINDS_20261104](kb:FPS_DESCRIPTOR_BINDS_20261104) | Точный ключ/dynamic offsets,host/build evidence,следующий GPU check |
| Vulkan state dedup | [FPS_VULKAN_STATE_DEDUP_20261103](kb:FPS_VULKAN_STATE_DEDUP_20261103) | Пять opt-in фильтров,фактические counts,проверки и границы |
| GPU timestamp профиль | [FPS_GPU_TIMESTAMPS_20261103](kb:FPS_GPU_TIMESTAMPS_20261103) | Bounded query pool,1092runtime samples,draw/Resolve/copy интервалы и границы |
| Fence status API | [FPS_FENCE_STATUS_20261102](kb:FPS_FENCE_STATUS_20261102) | Direct polling,корректность,малый CPU эффект и GPU-profile приоритет |
| Coherent readback API | [FPS_COHERENT_READBACK_20261101](kb:FPS_COHERENT_READBACK_20261101) | Actual memory flags,invalidate32→0,проверки и границы |
| Vulkan API descriptor cache | [FPS_VULKAN_API_20261101](kb:FPS_VULKAN_API_20261101) | Immutable bindings,128→4updates oracle,границы и следующие API-направления |
| Resolve submissions batching | [FPS_RESOLVE_BATCHING_20261031](kb:FPS_RESOLVE_BATCHING_20261031) | Lifecycle,40→14submits GPU oracle,отдельный кандидат и границы |
| Другие решения оптимизации readback | [FPS_READBACK_ALTERNATIVES_20261030](kb:FPS_READBACK_ALTERNATIVES_20261030) | Idle-slot reclaim, latest-fence wait, submit batching; проверки и ограничения |
| Распределение CPU и параллельная копия текстур | [CPU_PARALLEL_READBACK_20261027](kb:CPU_PARALLEL_READBACK_20261027) | Пулы потоков, блокировки, тесты и игровое сравнение |
| Исчезновение моделей у края UltraWide | [ENGINE_OFFSCREEN_20261028](kb:ENGINE_OFFSCREEN_20261028) | Local frustum, сферы видимости и отдельная выгрузка ресурсов |
| Настройки памяти и подгрузка ресурсов | [RESOURCE_STREAMING_20261027](kb:RESOURCE_STREAMING_20261027) | Последний запрос, новый кандидат, проверки и невоспроизведённая выгрузка |
| Timing, buffers, DrawThread, lookup | [ACV_TIMING](kb:ACV_TIMING) | Для соответствующих guest VA/полей |
| Runtime boundaries, слои | [ARCHITECTURE](kb:ARCHITECTURE) | Перед архитектурным изменением; исторические даты учитывать |
| Отдельная папка игры/лаунчера | [LAUNCHER_PACKAGE](kb:LAUNCHER_PACKAGE) | Сборка, пути и проверка standalone-пакета |
| MangoHud FPS, дизеринг и наборы настроек | [LAUNCHER_SETTINGS_20261030](kb:LAUNCHER_SETTINGS_20261030) | Host/Guest-счётчики, адаптивный кэш, пресеты и проверка всех параметров |
| Native Linux build/imports | [NATIVE_REXGLUE_LINUX](kb:NATIVE_REXGLUE_LINUX) | Для воспроизведения этой линии |
| Keyboard/mouse/config | [LINUX_INPUT](kb:LINUX_INPUT) | Перед input/Setup изменениями |
| ISO/BDT/BHD/TPF/DCX | [ASSET_FORMATS](kb:ASSET_FORMATS), [RE log](kb:REVERSE_ENGINEERING) | Только нужный формат/адрес |
| Ghidra snapshot/integrity | [GHIDRA_CAPTURE](kb:GHIDRA_CAPTURE) | Перед bounded capture |
| SQLite commands/history/retry | [SQLITE_LOGGING](kb:SQLITE_LOGGING), [TASK-SQL](kb:TASK_SQL) | Перед logger changes/recovery |

## Vulkan API и Ghidra

При Vulkan использовать implementation/API skill (локальный навык вне репозитория: `~/.codex/skills/vulkan/SKILL.md`)
и локальные Khronos references (локальный навык вне репозитория: `~/.agents/skills/vulkan/SKILL.md`).
При PPC/XEX analysis использовать Ghidra RPC skill (локальный навык вне репозитория: `~/.agents/skills/ghidra-rpc/SKILL.md`):
проверить доступность MCP; fallback — scripts/ghidra-rpc.sh, правильный проект/
бинарник и raw instructions при неполной базе. Доступность не предполагать,
живую DB не чинить без отдельного разрешения. [Правила](kb:AGENTS).

## Поиск и обновление

1. Взять один открытый пункт PLAN; проверить relevant SQLite task/errors и evidence.
2. Искать адрес/символ через rg в выбранном файле/SDK/индексе, затем расширять поиск.
3. Уточнить одно утверждение, проверить его подходящим инструментом и записать
   источник/уверенность/ограничение. Гипотеза не становится фактом от повторения.
4. Обновить соответствующий technical section и PROGRESS milestone, а не
   добавлять копии одной истории в каждый файл. STATE держать коротким.

Архив fine/ не включать в обычный поиск/оптимизацию; открывать только при явной
ссылке или для конкретной отложенной ветки. Сырые SQL output и log files читать
точечно. Исходные версии текущего сжатия: [snapshot](logs/docs-context-20261022/manifest.json).

Проверка активных ссылок и отсутствия прежних дублирующих файлов (read-only;
не обходит fine/, не проверяет удалённые URL и HTML anchors):

```bash
node tools/decomp-logging/cli.mjs run --stage docs_check -- python3 tools/check_active_docs.py
```
