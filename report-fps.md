# ACV FPS: разведка и граница измерений — 2026-10-29

**Цель >30FPS пока не достигнута.** Новый одновременный CPU/GPU/IP замер
пользовательской сессии:29.02guest swap/с,34.46мс по1500интервалам внутри окна.
GPU92.4% в среднем; это кандидат на GPU-ограничение, а не доказанная причина.
Миссия/действие ещё не указаны; сырые интервалы/1%low не записаны.
[Baseline](perf/baseline.md) отделяет исторические данные от нового замера.

## Сборка и артефакты

Проверены текущие CMakeCache/build.ninja, compiler metadata и release1.0:

- Host: Release, Clang23.1.4 (прежний compiler cache23.1.3), `-O3 -DNDEBUG`,
  `-mcmodel=large -msse4.1`, generated PPC также `-gline-tables-only`.
  Debug-информация строк не означает Debug-оптимизацию. LTO/-march=native нет.
- SDK multi-config: у Release runtime `-march=x86-64-v2 -O3 -DNDEBUG`,
  `-fno-strict-aliasing -ffp-model=strict`; LTO нет. Debug/RelWithDebInfo записи
  в том же Ninja-файле не означают, что Release линкует Debug runtime.
- SDK commit `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`, с локальными патчами.
  Полные хеши текущих четырёх артефактов совпали с принятой baseline.
  Сборку заново не запускали. Не добавляли fast-math/LTO или -march=native без A/B.
- Manifest: `tmp/acv-rexglue-linux-indexed/acv_linux_probe_manifest.toml`;
  включает три boundary-файла из tools/native-rexglue. Это codegen manifest,
  runtime-настройки поступают через canonical acv.config и профильный backend.
- Linux: GTX1660 SUPER6GiB, driver615.78.08; runtime log Vulkan1.4.363.
  perf CLI отсутствует; имеются nvidia-smi, RenderDoc и собственный
  perf_event_open sampler. Доступность perf events на новом игровом PID ещё
  не проверена; perf_event_paranoid=2 не изменялся.
- Windows/DLL и D3D12-сравнение не выполнялись: текущий порт Linux,
  Windows/Wine требует отдельного запроса. Наличие D3D12 source не означает
  доступный backend в этом Linux runtime.

## Гипотезы и уже существующие результаты

| Изменение/состояние | FPS / frametime / 1% low | Вывод |
|---|---|---|
| Исторический поздний пользовательский лог |22.23 swap/s /44.99мс /нет данных|Просадка подтверждена, сцена неизвестна|
| FBO/full, прежний эксперимент |Невалидно / — / —|Чёрный gameplay; отклонён по корректности|
| Deferred pool selection, прежний A/B |≈60 cadence обеих сторон / — / —|Pool wait2.791→0.0966%wall, FPS не вырос; другая сцена|
| Parallel readback2, уже принят |Сопоставимого FPS A/B нет|Host streaming65.93→58.43мс; это не игровой frametime|
| GPU poll backoff, прежний A/B |Ускорение не доказано|GPU Commands≈89–91%ядра; Main≈24–25%, polling не главный в той сцене|
| Texture budgets/readahead, уже включены |Ускорение не доказано|В прежнем окне182MiB текстур, evictions0, большая часть чтений из RAM|
| Новый CPU/GPU/IP замер |29.02 swap/s /34.46мс /нет1%low|GPU92.4%; миссия/маршрут ещё не указаны|
| Новые performance-правки |Не применялись|Нет baseline той же сцены|

Источники предыдущих экспериментов:
[readback](docs/FPS_READBACK_PROGRESS_20261027.md),
[CPU/copy](docs/CPU_PARALLEL_READBACK_20261027.md),
[streaming](docs/RESOURCE_STREAMING_20261027.md).
Не переносить показатели загрузки GPU/CPU из этих запусков в текущий baseline.

## Проверенные настройки и риски

Источник cvar — `tools/rexglue-sdk/src/`, а не названия из чужого порта:

- `graphics/command_processor.cpp:41–89`: vsync, frame_limit_fps,
  readback_resolve(none/fast/some/full), readback_memexport,
  readback_memexport_fast, async_shader_compilation. Асинхронная компиляция
  по умолчанию уже включена; `graphics/graphics_system.cpp:38` store_shaders=true.
- `graphics/vulkan/command_processor.cpp:64`: vulkan_async_skip_incomplete_frames.
  Пропуск незавершённых кадров может улучшить cadence ценой картинки;
  это не допустимое доказательство ускорения корректного рендера.
- `graphics/vulkan/render_target_cache.cpp:40`: render_target_path_vulkan;
  рабочий ACV профиль FSI/full. Выключение readback уже ломало CPU-generated atlas.
  Общий deferred provider и parallel-copy2 уже входят в принятый пакет;
  добавление ещё одной «тройной буферизации» без трассы не обосновано.
- `graphics/pipeline/texture/cache.cpp:30–75`: реальные лимиты кэша,
  anisotropic_override, resolution_scale и draw_resolution_scale_x/y.
- `d3d12_pso_no_block_at_submission_end` в проверенных graphics sources отсутствует.
- `graphics/graphics_system.cpp:149–165`: vsync=false переводит guest vblank
  на интервал guest_tick_frequency/1000 вместо частоты video mode. Не менять
  этот флаг без отдельной проверки игрового времени.
- `graphics/command_processor.cpp:1043–1076`: frame_limit_fps ограничивает swap,
  не timestep. Лимит60 не объясняет устойчивый интервал45мс сам по себе;
  FIFO/драйвер/очередь/CPU/readback ещё требуется разделить.

Последние3пользовательских runtime-лога: два ранних без error/warn,
последний содержит одну пару resolve/draw ошибок, не повторяющийся shader storm.
В этой выборке не обнаружены сообщения unimplemented или memexport failures.
Исторические unresolved codegen diagnostics относятся к раннему bring-up;
новый codegen в этой сессии не запускался.

## Как собрать недостающий профиль

Запустить release1.0 обычным лаунчером, загрузить указанную миссию и воспроизвести
одинаковый60-секундный маршрут/камеру после120с от запуска. Не менять оригинальные
сейвы/конфиг. Для последующих A/B использовать уже имеющийся профильный backup/clone
под writer lock; не копировать профиль во время записи игрой.

Из корня репозитория после готовности сцены выполнить один сбор
(или сообщить агенту «готов», чтобы он запустил сбор):

```bash
node tools/decomp-logging/cli.mjs run --stage fps_capture --timeout-ms 200000 -- python3 tools/native-rexglue/perf_capture.py --scene "МИССИЯ: УЧАСТОК И ДЕЙСТВИЕ" --output logs/fps-baseline-01
```

Сборщик находит ровно один процесс release1.0 (при нескольких нужен `--pid`),
ждёт возраста120с и параллельно запускает существующие CPU/GPU и user-IP samplers
на60с. Каталог должен быть новым. Сохраняет scene, process identity, timestamps,
maps, SHA-256 файлов ELF/загруженных ReXGlue/FFX библиотек до/после замера,
runtime.log и соседний launch.json, если он существует. Inode/device из maps
сохраняются для библиотек; различие inode отклоняется. В текущей системе device
в maps=00:1f, stat=00:37 при совпавшем inode; это расхождение записано явно,
не трактуется как доказанная замена файла. Это хеши файлов, а не дамп памяти.

stdout/stderr сборщиков сохраняются и передаются в logger; недоступные perf events,
потери сэмплов, неполное окно, отсутствие GPU-данных или завершение игры дают
неуспешный сбор. Сборщик останавливает только собственные sampler-процессы.
Не запускать GPU benchmark, сборку или replay одновременно. Ошибку perf permissions
сохранить; sudo/ptrace/sysctl не менять. GPU utilization из nvidia-smi относится
ко всему устройству, а не к отдельному3D-engine/GPU timestamp.

Ручной gameplay-сценарий: release1.0,2560×1080, текущие настройки; после120с
пройти согласованный60-секундный участок без меню/паузы, повторяя маршрут и камеру.
Сообщить об исчезнувших объектах, неверных материалах, зависаниях и изменении темпа
логики. CAS/FSR effects и другие разрешения не тестировать. Для каждого повтора
новый output и описание сцены. Оригинальные сейвы/config не копируются сборщиком.
Снимок runtime.log включает прежние строки сессии; окно измерений определяется
timestamps/gate в session.json и sampler output, а не всем файлом лога.

Существующий runtime сохраняет только агрегаты300swap-интервалов. Для точного
1% low нужен отдельный bounded per-frame trace в диагностическом кандидате;
текущие агрегаты этого числа не содержат. Это следующий инструментальный шаг
после выбора сцены, до любых оптимизаций. Кандидаты не заменяют release1.0.

## Продолжение: сбор CPU/GPU/IP — 2026-10-29

Добавлен [perf_capture.py](tools/native-rexglue/perf_capture.py).
[Host regression](tools/native-rexglue/test_perf_capture.py) запускает реальные
профилировщики на отдельном Python-процессе: одинаковый PID/start, перекрытие
окон, hashes/log snapshot, запрет перезаписи и CLI gate>=90с проверены;
ранний выход наблюдаемого процесса не принимается как успех.
Результат host-проверки не является игровым baseline или проверкой render/pacing.
Пользователь затем запустил игру; первый игровой замер описан ниже.
Название миссии/маршрут ещё запрошены.
Сырые покадровые интервалы/1%low остаются отдельным следующим шагом.

## Пользовательский замер: сессия161152 — 2026-10-29

[Evidence](logs/fps-baseline-20261029-session161152-02/session.json),
[расчёт](logs/fps-baseline-20261029-session161152-02/analysis.json),
[CPU symbols](logs/fps-baseline-20261029-session161152-02/ips/summary.json).
PID11835, sampling начинается на183.87с;21:14:57–21:15:57(+05:00).
60CPU/GPU строк,5665IP samples, lost0/unavailable0. Игра не остановлена агентом;
состояние живого процесса после записи не выводится из этого исторического замера.

Пять полных300swap окон внутри CPU/GPU sample timestamps:
1500интервалов,29.0203swap/с,34.4587мс; окна27.72–30.10/с.
Максимальная пауза802.88мс. 1%low неизвестен.
Внутри этого окна runtime.log не содержит warning/error; раньше в сессии есть
DLC/STUB warnings и один skipped async-placeholder presentation. Полная сессия
не объявляется корректной по чистому60-секундному окну.

GPU device-wide92.43%(32–97%),1890MHz; VRAM2066–2081MiB.
CPU по доле одного ядра: GPU Commands68.30%, DrawThread44.71%, Main21.56%,
второй GPU Commands17.14%. В DrawThread top user IPs:
sub_82AF5708(26.44%),sub_82BB58A8(21.31%); роль функций пока не установлена.
Disk read delta10.89MiB; RSS1930→1942MiB. Этого недостаточно для причинного вывода.

Команда фиксирует FSI/full,60limit,vsync=true,output2560×1080,guest1280×720,
present_effect=fsr/fxaa,texture cache2048/3072MiB/600s. Это пользовательские
настройки; эффект FSR не тестировался, cache отличается от исторического baseline.
ELF/runtime/GPU disk SHA совпали с принятыми файлами; inode совпали с maps,
device mismatch сохранён. Идентичность загруженных байтов этим не доказана.

Первый сбор отклонён до sampling из-за слишком строгого сравнения device;
[диагностика](logs/fps-mapping-check-20261029.json) и host-regression
сохранили mismatch как ограничение, inode mismatch по-прежнему отклоняется.
Повтор выполнен в новом каталоге. Не смешивать старые22.23 и новые29.02 как A/B:
сцена не идентифицирована, настройки cache различаются, performance-правок нет.
Далее: уточнить миссию/маршрут, bounded per-frame и stage trace отдельного
кандидата; проверить GPU/readback гипотезу на том же участке без смены разрешения,
FSR effects или правил coherence.

Предыдущие версии изменённых документов сохранены и сверены:
[SHA-256](logs/fps-capture-20261029/docs-before/sha256.json).

## Изменения предыдущей разведки

Только сохранение evidence, baseline, этот отчёт и указатели в планах.
Код SDK, CMake, runtime config, игровые данные и принятые артефакты не изменялись.
Performance-коммитов нет: без контролируемого замера нечего принимать как ускорение.
Основной риск дальнейшей работы — ускорить swap/пропуски вместо реальной симуляции
или нарушить CPU↔GPU coherence. Приёмка: >30 на том же участке, улучшение frametime
и 1% low, сохранённые материалы/объекты и темп логики, без новых runtime ошибок.
