# ACV: bounded GPU timestamps — 2026-11-03

2026-11-03: Sol собрала opt-in Vulkan API state dedup: index/vertex bindings,
push constants, blend constants и depth bias. Host ASan/UBSan и compute push
GPU oracle PASS; render/gameplay набора ещё не проверен. Copy-bytes runtime240s
чистый,3516копий/9размеров; FPS-прирост не доказан.

Кандидат: `tmp/acv-vulkan-api-batch-20261103`. Флаги:
`ACV_VULKAN_DEDUP_BUFFER_BINDS`, `ACV_VULKAN_DEDUP_PUSH_CONSTANTS`,
`ACV_VULKAN_DEDUP_DYNAMIC_CONSTANTS` (каждый =1, default0). Кэши локальны Execute;
blend/depth сбрасываются при каждом bind pipeline. Общий deferred путь применим
к FSI и другим backend, но их image acceptance не выполнен.
[Build manifest](tmp/acv-vulkan-api-batch-20261103/build.json),
[GPU compute oracle](logs/vulkan-buffer-binds-20261103/gpu-oracle.txt),
[copy-size runtime](logs/gpu-copy-bytes-20261103/runtime-01/copy-byte-summary.json).

2026-11-03: Sol добавила copy_bytes в GPU timestamp профиль staging-копий.
Суммирование coalesced команд и сброс слота проверены ASan/UBSan; actual GPU
oracle bytes/guards и метаданные PASS, 3TU кандидат собран. Новый игровой замер
с размерами ещё не выполнен; FPS-прирост не заявлен.

Кандидат: `tmp/acv-gpu-copy-bytes-20261103`. Поле copy_bytes относится только
к помеченным staging-копиям; сопутствующая работа по-прежнему входит во время.
[Проверки](logs/gpu-copy-bytes-20261103/gpu-oracle.txt).

Astra 6 реализовала измерение GPU-интервалов для выбора следующей оптимизации.
Кандидат собран и прошёл host/GPU oracle. Это диагностическая доработка;
сама по себе она не подтверждает прирост FPS. Runtime-результат приведён ниже.

## Реализация и границы

[Общий helper](tools/native-rexglue/gpu_timestamp_profiler.h) используется
SDK и тестами. [Воспроизводимый SDK patch](tools/native-rexglue/patches/gpu-timestamp-profiler.patch)
добавляет метки реальных guest draw/indexed, Resolve dispatch и GeneralReadback copy.
Состояние принадлежит GeneralReadbackContext; размеры CP и DeferredCommandBuffer
сохранены. Новая deferred-команда добавлена в конец enum. Два отсутствующих в
таблице SDK core PFN загружаются через существующий vkGetDeviceProcAddr;
размер общей таблицы функций SDK сохранён.

Пул ограничен 2048 запросами: 64 submission-слота по 16 пар меток.
Выборка максимум 20 submissions/s, до 1200 записей, только 120–180 s после
старта процесса. Последовательные команды одной категории объединяются;
при исчерпании 16 пар новые интервалы пропускаются с признаком truncation.
Начальная метка всегда резервирует место конечной.

Reset записывается на GPU при открытии submission, перед render pass.
Результат читается только после существующего доказательства завершения того же
submission, с 64_BIT и WITH_AVAILABILITY, без WAIT. NOT_READY проверяется максимум
три раза после завершения; затем слот безопасно освобождается без результата.
Ошибки submission удерживают диапазон запросов до остановки GPU; query pool
уничтожается после остановки. Проверены validbits, период и неоднозначное
переполнение; отсутствующая поддержка или core PFN явно выключает диагностику.

Интервалы TOP/BOTTOM включают сопутствующие команды. Они могут перекрываться
на GPU, поэтому их нельзя складывать в проценты кадра или выдавать за чистое
время FSI-шейдера. Временная выборка submissions не является взвешенным профилем
всех кадров. Отчёт содержит охват, число команд, truncation/drop/error counters.

Без ACV_GPU_TIMESTAMPS профилировщик и query pool не создаются, файлы не пишутся,
часы на каждом draw не читаются. Для диагностики также нужен ACV_GPU_STAGE_PROFILE
и фактическое создание GeneralReadbackContext. Default off; release не заменён.

## Проверки

- Host ASan/UBSan: 100000 объединённых draw, пределы пула/пар/1200 samples,
  ticket guards, NOT_READY/availability, failed create с изменённым output handle,
  missing PFN, ошибки чтения, исключения записи и безопасное уничтожение.
- GTX 1660 SUPER: timestampValidBits=64, timestampPeriod=1 ns. В каждом ON-прогоне
  8 submissions, 64 региона, 128 меток и 8 чтений без ожидания; восемь циклов
  повторного использования диапазона. Все 32 compute snapshots/guards совпали
  побайтно с OFF (262144 bytes на mode/trial). Проверен настоящий reset-only NOT_READY.
- Изолированно пересобраны три Vulkan TU. Exact patch replay, 887 input hashes,
  RTLD_NOW, фактические зависимости и сохранность семи кандидатов/release проверены.
  Validation layers в GPU oracle не включались; чистота validation не заявляется.

[Полный результат и операции](logs/gpu-timestamp-profile-20261103/result.json),
[GPU oracle](logs/gpu-timestamp-profile-20261103/gpu-pfn-oracle.jsonl),
[manifest кандидата](tmp/acv-gpu-timestamp-20261103/build.json).
GPU SHA-256: `38b2e377a86d76da926f38740777487b2736b6f40282aebcb12a81f1117ab4cf`.

## Runtime

Сеанс `20261103T043854961459Z` завершён: 240 s, child=0, fatal/runtime errors=0,
лимит достигнут. Использованы существующий профильный backend, writer lock и
проверенная резервная копия. Финальный вывод 2560×1080, format=64, FIFO=2,
requested_bits=10; пользовательские настройки сохранены. Это запись фактического
вывода, не приёмка цветовой точности.

В окне 120–180 s собрано 1092 samples: selected=submitted=completed=processed=1092,
occupied=0; capacity_dropped/truncated/NOT_READY/discarded/failed/errors/ambiguous=0.
Все ticket/availability/finite-duration проверки прошли. Реальный deferred decoder
и метки внутри guest render pass выполнены без runtime-ошибок.

| Измеряемый включающий участок | Регионов | Команд в выборке | Медиана, ms | p95, ms | Максимум, ms |
|---|---:|---:|---:|---:|---:|
| Guest FSI draw + сопутствующие команды | 3746 | 437496 | 0.073952 | 5.881952 | 19.064736 |
| Resolve dispatch + сопутствующие команды | 3953 | 3953 | 0.025824 | 0.031360 | 2.089600 |
| General staging copy + сопутствующие команды | 3953 | 3953 | 0.284512 | 0.956704 | 3.232928 |

Draw-регион может содержать много draw calls, поэтому сравнение одной его медианы
с одним copy/dispatch не определяет цену отдельной команды. Все интервалы могут
перекрываться; суммы/проценты GPU кадра не вычислялись. Raw guest swap cadence
19.3116/s, 1% low=12.9399/s — только наблюдение этого запуска, без OFF-контроля и
подтверждённой сцены/маршрута. Причинный FPS-выигрыш не измерен; render/game-time
и gameplay acceptance остаются открытыми.

Следующее исследование: длинные draw-участки и staging copy, с учётом числа
команд, размера копий и зависимостей. Короткий p95 Resolve dispatch в этой выборке
не поддерживает его выбор в качестве следующей shader-оптимизации; это локальная
гипотеза, а не доля общего времени GPU.

[Результат сеанса и хеши](logs/gpu-timestamp-profile-20261103/runtime-01/runtime-result.json),
[GPU интервалы](logs/gpu-timestamp-profile-20261103/runtime-01/timestamp-summary.json),
[live mappings/settings check](logs/gpu-timestamp-profile-20261103/live-root-check.json).
Собственный игровой процесс завершён. Release/default прежние.

## Воспроизведение

Проверка запуска без игры и записи профиля:

```bash
bash tmp/acv-gpu-timestamp-20261103/start.sh 1 --check-only
```

Тот же script без --check-only запускает диагностический сеанс на 240 s.
Аргумент 0 выключает timestamps. Оба режима фиксируют batching/descriptor/coherent/
statuspoll=1 и Forward128/reclaim/latest=0. Используется исходный acv.config ARTEM;
сейвы не сбрасываются и не восстанавливаются.

Локальная спецификация: Vulkan-Docs commit
`090f1b190d60ced4a1d198fd3747d071cc271b1c`, queries.adoc: availability/reset generation,
validbits/period, vkCmdWriteTimestamp stage semantics. Подробные границы — в result.json.
