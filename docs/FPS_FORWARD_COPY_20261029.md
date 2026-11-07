# FPS: live readback copy analysis — 2026-10-29

Live-анализ ниже относится к историческому PID11835/session161152 без перезапуска.
Позднее пользователь разрешил тест новой сборки; результаты отдельных запусков
и GPU-staging проверки приведены ниже. Старый процесс агент не останавливал.
Сцена по прежнему уточнению пользователя — первая миссия/gameplay, мелкие зависания;
маршрут в позднем окне не подтверждён. Это не контролируемый replay.

## Что измерено в работающем процессе

[30s capture](logs/fps-live-copy-20261029-01/analysis.json):
1527 user CPU samples двух GPU-потоков,509 попаданий в обратный libc copy loop,
lost0/unavailable0.477 попаданий вызываются ParallelReadbackCopy::Copy/Worker,
32 — Memory::ReadPhysicalCpu. Это веса CPU-сэмплов, не число вызовов или latency.
GPU device-wide95.73%; два полных pacing окна внутри этого30s замера:
600интервалов,24.7522swap/s,40.4004ms.1%low неизвестен.
Разница с прежними29swap/s не является регрессией: код/настройки не менялись,
маршрут/камера в этих окнах не контролировались.

[Дополнительные15s registers](logs/fps-live-copy-20261029-02/geometry.json):
317/317 sampled copies имеют непересекающиеся диапазоны, оба начала page-aligned,
одинаковые low12 address bits. Частые размеры частей1884160 и3624960байт.

[Sampler](tools/native-rexglue/perf_copy_sample.py) использует perf_event_open:
IP/TID,RCX/RDX/RSI/RDI/RSP и только8байт user stack(return IP),50Hz per CPU second.
Интерпретация ограничена stack-neutral диапазоном libc0x1b1ba0–0x1b1bd3.
В нём RDX сохраняет исходный размер, RDI — destination; source восстановлен из
RSI/RCX с поправкой128байт между двумя decrement instructions.
Проверены локальные Linux perf headers, инструкция call у caller и ELF text LOAD
(offset==vaddr). Полного unwinding или чтения guest/save data нет.
libc pinned SHA071e34179e645b25f8a8c2e7108894dc665caa9f5f8c939cd579a75b6638bc3f;
другая libc отклоняется до семплирования. Inode maps/disk совпал; это не дамп памяти.
[Actual instructions](logs/fps-live-copy-20261029-01/libc-backward-loop.asm),
[caller](logs/fps-live-copy-20261029-01/pool-copy-caller.asm).
eu-addr2line не разрешил адрес plugin(exit1); независимый nm range lookup
подтвердил функции. Source line из DWARF не заявляется.

[Host test](tools/native-rexglue/test_perf_copy_sample.py) подтверждает реальные
perf regs/user-stack layout, известный caller copy_site, точный4MiB размер и source,
отсутствие потерь; наблюдаемый процесс не останавливается.

## Подготовлен кандидат forward128

[Source](tools/native-rexglue/parallel_readback_copy.h),
[SDK patch](tools/native-rexglue/patches/forward-readback-copy.patch).
ParallelReadbackCopy получает optional bool(defaultfalse). Только при явном
ACV_READBACK_FORWARD_COPY=1 и существующем parallel pool2/4 используются
последовательные128байт memcpy chunks и точный хвост.
Малые constant-size memcpy компилируются в обычные SIMD loads/stores и обходят
large-copy backward heuristic. ISA остаётся x86-64-v2; fast-math/LTO не добавлены.
UI/settings schema и пользовательский config не менялись.

Меняется только CPU copy primitive после completed ticket + successful invalidate.
Memory lock/slot lease держатся до завершения fork/join; publication и reuse
по-прежнему после всех workers. Range/length, число потоков, fence/barrier,
fallible VkResult handling, Vk ownership и teardown не изменялись.
Проверена прежняя цепочка device write → HOST_READ barrier → fence wait →
vkInvalidateMappedMemoryRanges → host copy. Локальный Khronos reference:
~/.agents/skills/vulkan/references/Vulkan-Docs/chapters/memory.adoc,
vkInvalidateMappedMemoryRanges, commit090f1b190d60ced4a1d198fd3747d071cc271b1c.

[ASan/UBSan regression](tools/native-rexglue/parallel_readback_copy_test.cpp):
оба режима,1/2/4потока, нулевой размер,127/128/129 boundaries, unaligned/tails/guards,
повторные поколения, конкурентные producers и teardown PASS.

[Actual2-thread pool host A/B](logs/fps-forward-copy-20261029/pool-bench-summary.json):
идентичный rotating64MiB RAM input,5 alternating AB/BA trials, exact bytes.
Speedup1.03–1.16; основные1884160/3624960части:1.134/1.135.
Это ordinary host RAM, **не GPU-written staging и не FPS A/B**.
Активная игра оставалась работающей; нагрузка системы не была изолирована.
[Single-thread primitive experiment](logs/fps-live-copy-20261029-02/forward-host-summary.json):
1.16–1.22 на тех же размерах; не заменяет результат реального пула.

[Build manifest](tmp/acv-forward-copy-20261029/build.json),
[exact argv](tmp/acv-forward-copy-20261029/commands.json),
[isolated build script](logs/fps-forward-copy-20261029/build-isolated.py).
GPU6090b52a1b747f455ece77eb514f81f0746ff352d47225fd4f4c5524a0bf6d77.
Новый Vulkan command processor TU, прежний core TU с bounded swap trace,
остальные existing objects/libs; accepted ELF/runtime/FFX сохранены.
SDK output и release binaries не заменены. Source/build/host-tested;
После подготовки выполнены runtime smoke и GPU-staging correctness ниже;
контролируемый FPS A/B и ручная render/game-time приёмка ещё отсутствуют.
dlopen/local dependencies, exact patch replay и no-launch env/settings dryrun PASS.

## Внесённый Forward128: runtime и GPU boundary — завершение этапа

Пользователь уточнил «Forward128 — внеси доработку» и выбрал Astra6 для кода.
Forward128 остаётся в production source и SDK patch, явно включается в тестовом
start.sh(defaultflag1). Новую проверку GPU-staging и page-protected regression
написал и выполнил агент gpt-6-astra; сам copy primitive и кандидат6090b52a
после этих проверок не менялись. По умолчанию библиотека оставляет libc.

### Первый игровой запуск forward1

[Активные параметры](logs/fps-forward-test-20261029/active-on.json),
[raw swaps](logs/fps-forward-test-20261029/on-01/swaps.jsonl),
[расчёт](logs/fps-forward-test-20261029/on-01/swap-summary.json),
[завершённый launch](logs/fps-forward-test-20261029/on-01/launch.json).
PID30356/session171732,240s,child0/runtime_errors0/fatal0, actual2560×1080.
First mission gameplay запрошен; пользователь не прислал отдельную image/time
приёмку или подтверждение одинакового маршрута.

Raw120–180s:1072интервала,17.8587swap/s,mean55.9953ms,p9563.7794,p9967.2573,
1%low14.3942(11самых медленных интервалов),max83.3753ms; >100ms интервалов0.
1%low вычислен по guest swap intervals, не по уникальным показанным кадрам.
В худшем83.3753ms интервале between-swap83.3202ms,post-pacing0.0551ms,
limiter0.00002ms: причина в работе/ожиданиях между swap calls, не в limiter.

[CPU/GPU](logs/fps-forward-test-20261029/on-01/cpu-gpu-summary.json):
60rows,4439IP/lost0, GPU97.77%(97–98%),GPU Commands60.25%,DrawThread21.86%,
Main17.92%,copy worker13.22%ядра; disk reads0.
[Stage120–180](logs/fps-forward-test-20261029/on-01/stage-summary120-180.json):
general_publish16.726%wall,general_wait33.577%,fitting_slot50.732%,draw76.587%.
Метрики inclusive/вложенные, складывать их нельзя; это не GPU timestamps.

### Контроль forward0 не завершил окно

[Параметры](logs/fps-forward-test-20261029/active-off.json),
[launch](logs/fps-forward-test-20261029/off-01/launch.json).
Тот же кандидат/профиль/settings,flag0. PID32394/session172327 завершился
нормально(child0/fatal0) примерно на90.45s, до120s gate.
Сборщик правильно отклонил отсутствие наблюдаемого процесса; raw swaps/CPU
контрольного окна отсутствуют. До90s menu pacing не использовать как gameplay.
Это **невалидный FPS A/B**; разницу с прежними24–29/s не приписывать Forward128.

Перед каждым запуском существующий backend сделал verified backup под writer lock:
[forward1](logs/fps-forward-test-20261029/profile-backup.json),
[control0](logs/fps-forward-test-20261029/off-profile-backup.json).
Restore/reset userdata не делались. Все эти тестовые game-процессы завершены.

### Реальная GPU-written staging память: Astra6

[Код benchmark](tools/native-rexglue/vulkan_readback_copy_bench.cpp),
[ASan/UBSan protected-page test](tools/native-rexglue/parallel_readback_copy_test.cpp),
[полный результат/SHA/операции](logs/fps-forward-gpu-20261029/result.json).
GTX1660S,driver615.78.08,memory type4 flags14 =
HOST_VISIBLE|HOST_COHERENT|HOST_CACHED,mapping4KiB aligned.
Выбор повторяет runtime ChooseHostMemoryType(kReadback); fresh GPU transfer,
HOST_READ barrier, bounded fence wait, whole allocation invalidate перед каждой
политикой.6trials×18iterations,первые2warmup,96samples/policy/size; exact bytes и
4KiBguards проверены после каждой копии. Source/destination protected ends,
127/128/129 и наблюдавшиеся размеры,1/2/4threads/concurrent producers — ASan/UBSan PASS.

| Bytes | libc median µs | Forward128 median µs |
|---|---:|---:|
|3768320|154.744|160.284|
|7249920|327.162|337.852|
|4194304|164.573|169.222|

В этом коротком опыте Forward128 примерно на3%медленнее; разброс отдельных trials
есть, внешняя desktop-нагрузка не контролировалась. Тест synthetic staging/copy
latency, не игровой FPS; только cached coherent memory, validation layer отсутствует.
Первоначальная ошибка reset guards между размерами в test harness исправлена до
PASS; это не дефект copy primitive. Операции сохранены в result.json/SQLite.

**Итог:** доработка Forward128 внесена и воспроизводимо доступна в отдельном
тестовом варианте; корректность байтов подтверждена. GPU copy/FPS выигрыш не
подтверждён, поэтому default/release/RUNTIME_BASELINE не заменены.
Original RAM1.03–1.16 результат остаётся ограниченным RAM-опытом и не перевешивает
реальный GPU-staging результат.

## Дальнейшая граница

[Prepared start.sh](tmp/acv-forward-copy-20261029/start.sh) включает Forward128
явно; flag0 оставлен для контроля. Same candidate6090b52a,accepted ELF/runtime/FFX,
general deferred1/parallel-copy2/poll-backoff0,canonical acv.config,ARTEM,
существующий UUID/backend/writer locks. Больше запусков в этом этапе не требуется.
Для заявления FPS выигрыша нужны повторяемая сцена, полный matched control и
ручная render/game-time приёмка. Цель >30FPS не закрыта.
