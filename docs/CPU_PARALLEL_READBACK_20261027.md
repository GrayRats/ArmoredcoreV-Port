# Параллельная публикация readback и распределение CPU — 2026-10-27

Статус: реализовано, SDK собран, host/ASan/реальный Vulkan проверены.
**Параллельная копия принята пользователем2026-10-27.** Два210с игровых запуска завершены child0/fatal0; перенос в принятый пакет ещё не выполнен.
Основной WideView-пакет пока не заменён; стабильные60FPS не подтверждены.

## Что выявлено по потокам

На машине6 физических ядер/12 логических CPU. SDK ignore_thread_affinities=true:
игровые потоки не закреплены на шести номерах Xbox; актуальный /proc-замер
подтвердил allowed_cpus=0-11 и размещение потоков на разных CPU.
Отдельно работают DrawThread, GPU Commands, AI/PATH, CollisionSystem,
SmallJobRunner и существующие TaskRunner; Vulkan уже имеет пул создания pipelines.
Принудительно распределять пустой цикл ожидания по ядрам не требуется.

Новый sampler сохраняет last_cpu и allowed_cpus только после своего gate.
[Анализатор](../tools/native-rexglue/analyze_cpu_parallelism.py) сохраняет
отдельные TID даже при одинаковых обрезанных именах. Последний CPU показывает
размещение, а не доказательство одновременного выполнения или полезной работы.
Проверки одинаковых имён, миграций и запрета ранних замеров пройдены.

## Проверка busy polling

Горячие guest VA82BAF218/82BAE308 проверены по свежим raw PPC.
Ghidra описывает только первые8байт каждой функции; базу не исправляли.
Сохранены raw bytes и LLVM-декодирование; Xenon db16cyc кодируется7FFFFB78
(обычный LLVM показывает mr31,31),4×8 повторений. SDK codegen пропускает hints.

[gpu_poll_backoff.cpp](../tools/native-rexglue/gpu_poll_backoff.cpp) оборачивает
оригинал __imp__sub_82BAE308. Только ACV_GPU_POLL_BACKOFF=1, только caller82BAF2B0,
только исходный return1: дополнительный host sleep50мкс. Результаты, регистры,
timeout/cancel и последующая проверка completion сохраняются.
Host scheduling может задержать пробуждение дольше50мкс; это эксперимент,
не эмуляция точного числа Xenon cycles.

ASan/UBSan тест фактического hook с ABI-stub проверил pending/completed,
чужой caller, исключения, контекст и строгий env. Host polling250мс:
CPU248.947→6.377мс. В ELF проверена запись guest→strong hook;
вызов идёт через dispatcher, прямой call в objdump не ожидается.

В двух нагруженных210с запусках same ELF96c3f415/GPUf1ccdfb9
без/с backoff Main XThread уже23.85/25.09% ядра, GPU Commands89.44/91.21%.
Hotspot этой сцены не busy polling. Ускорение FPS/уменьшение CPU от backoff
в ней не доказано. Исторические44.8→95.3% нельзя переносить на другую сцену.
Оба запуска child0/fatal0, IP lost0; sampling после120с.
[Потоки off](../logs/acv-poll-backoff-20261027/off-cpu-parallelism.json),
[on](../logs/acv-poll-backoff-20261027/on-cpu-parallelism.json),
[сессии и команды](../logs/acv-poll-backoff-20261027/comparison-state.json).

## Реализованная параллельная работа

Графический поток в этой сцене тратит≈24% inclusive wall на публикацию
готовых readback-данных. Добавлен ограниченный persistent worker pool:
[parallel_readback_copy.h](../tools/native-rexglue/parallel_readback_copy.h).

- Один синхронный job,1..4 суммарных потока, максимум3 постоянных workers.
- Копии меньше1МиБ последовательные; крупные делятся на непересекающиеся части.
- GPU fence и invalidate проверяются до запуска CPU workers.
- Глобальная блокировка Memory удерживает mapping/поколение; workers выполняют
  только memcpy, без Memory/Vulkan API. После join публикуется provider и
  освобождается slot. Следующий job не использует незавершённые указатели.
- Конструктор при частичном отказе создания потоков останавливает и join-ит
  созданные workers. SDK сообщает отказ и сохраняет последовательный путь.
- Переключатель ACV_PARALLEL_READBACK_COPY=2 или4 действует только для явно
  включённого общего readback. По умолчанию worker pool не создаётся.
- SDK private context хранит pool до уничтожения bridge.64слота/64МиБ сохранены.
  Generated PPC, схема настроек и принятый пакет не изменены.

[Durable patch](../tools/native-rexglue/patches/parallel-readback-copy.patch)
воспроизводится без fuzz и совпадает с SDK sources.

## Проверки

| Проверка | Результат и предел |
|---|---|
| Host ASan/UBSan | Точные байты, guards, невыравненные tails, повторные generations, два concurrent producers;1/2/4 потока PASS |
| Streaming host RAM,512МиБ/trial | Медианы65.9345мс serial,58.4269мс2threads,58.0945мс4threads;≈13% меньше времени; не GPU/игровой FPS |
| Реальный NVIDIA Vulkan + SDK Memory |12копий по2МиБ,8alias faults, overlap/partial writes/reuse/drain PASS |
| Базовый GPU test |12копий по4КиБ,8faults PASS |
| Actual teardown ASan/UBSan |8сценариев, в том числе uncommitted copy; освобождение после completion PASS |
| SDK build |GPU8fb12565 PASS, runtimeebf338a1, сохранён FFX0933159a |
| Игра serial/parallel |Оба210с child0/fatal0; сцены вошли в нагрузку в разное время, сравнение FPS невалидно |

Профили клонированы офлайн под writer lock, backup проверен. Сравниваются
одинаковые ELF/runtime/GPU/FFX/settings и исходные saves; AI, shader warmup
и кадры не являются строго детерминированными. Каждый запуск210с,
CPU/GPU/IP после120с на35с. Никаких параллельных сборок/replay/GPU benchmark
во время игрового сравнения.

Кандидат tmp/acv-parallel-copy-fix/, ELF96c3f415/GPU8fb12565.
[Хеши/клоны](../logs/acv-parallel-readback-copy-20261027/build-and-clones.json),
[состояние запуска](../logs/acv-parallel-readback-copy-20261027/comparison-state.json).
Дальше: оценить late publication time, FPS, CPU workers и отсутствие регрессий;
оставить serial default до соответствующей приёмки.


## Принятие и остаток

Пользователь принял parallel-copy исправление. Worker TID20460 выполнял копирование:
16.79% ядра, allowed_cpus0-11; основной GPU thread TID20406 —87.47%.
Это доказательство распределённой работы, не ускорения FPS.
[Off CPU](../logs/acv-parallel-readback-copy-20261027/off-cpu-parallelism.json),
[on CPU](../logs/acv-parallel-readback-copy-20261027/on-cpu-parallelism.json).
Сравнение завершено, фоновых игр/сборок нет. Ближайшая задача — новый отдельный
пакет с принятым режимом, затем сопоставимый gameplay-замер. Backoff отдельно не принят.


Актуально после сокращения объёма пользователем: принятая parallel-copy включена
в отдельный FOVHUD-пакет (`../../ACV-Linux-TRG-FOVHUD-20261027/start.sh`, исторический пакет; каталог сейчас отсутствует).
CAS/FSR интегрированы без тестов. Другие разрешения/FPS/save/controller задачи
пропущены; дальнейшая работа только FOV/HUD2560×1080.
[Последняя точка продолжения](X360_PROJECT_STATE.md).
