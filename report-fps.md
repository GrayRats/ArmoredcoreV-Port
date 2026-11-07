# ACV FPS: другие решения оптимизации — 2026-10-30

2026-11-04: реализованы все 7 opt-in Vulkan путей: persistent cache, sync2,
timeline, vertex-input GPL, dynamic cull/front-face, staging suballocation и
вторая copy queue того же family. Native runtime/GPU пересобраны (243 TU,
без старого PCH). ASan/UBSan, GPU bytes/pixels, deferred/patch replay PASS;
cold/warm runtime135s оба child0/fatal0/errors0; cache reload=true. Прирост FPS и gameplay
не приняты; release2.0 сохранена. [Результат](kb:FPS_VULKAN_SEVEN_20261104).

2026-11-04: реализован opt-in exact descriptor-bind dedup (a8abd3a1).
Host ASan/UBSan/build/replay/load/check-only PASS. GPU oracle скомпилирован,
но отложен из-за активной пользовательской игры; FPS/render не проверены.
Release2.0 сохранён. [Следующая проверка](kb:FPS_DESCRIPTOR_BINDS_20261104).

2026-11-03: готов отдельный ACV-Linux-release-2.0 со stencil3 и предыдущими
Vulkan API доработками. GPU3TU и TRG пересобраны; неизменённые guest ELF/runtime/FFX
переиспользованы. ASan/UBSan, backend, UI600frames,189hashes/loader и135s startup
PASS (child0/fatal0/errors0). Исправлен выбор2.0 вместо сохранённого пути1.0;
пользовательский settings/config не переписан. Версия1.0 сохранена.
Миссия/FPS/stencil-image acceptance открыты.

2026-11-03: по запросу пользователя проверенные Vulkan API доработки включены
в ACV-Linux-release-1.0. GPU/default env и packaging baseline обновлены,
189file manifest и release135s smoke PASS (child0/fatal0/errors0).
Старые файлы с хешами сохранены. Три дополнительных stencil-фильтра собраны,
host-tested,default off в отдельном кандидате; GPU render acceptance открыт.

2026-11-03: Vulkan state dedup (Sol) runtime240s и counter135s завершены без
runtime/fatal ошибок. За40.04s пропущено173122index binds (6.54%) и2638push updates
(4.15%). Vertex/blend/depth не вызывались; их польза в этой сцене не измерена.
FPS/render acceptance открыты. [Результат](kb:FPS_VULKAN_STATE_DEDUP_20261103).

2026-11-03: Sol собрала opt-in Vulkan API state dedup: index/vertex bindings,
push constants, blend constants и depth bias. Host ASan/UBSan и compute push
GPU oracle PASS; render/gameplay набора ещё не проверен. Copy-bytes runtime240s
чистый,3516копий/9размеров; FPS-прирост не доказан.

2026-11-03: Sol добавила copy_bytes в GPU timestamp профиль staging-копий.
Суммирование coalesced команд и сброс слота проверены ASan/UBSan; actual GPU
oracle bytes/guards и метаданные PASS, 3TU кандидат собран. Новый игровой замер
с размерами ещё не выполнен; FPS-прирост не заявлен.

2026-11-03: Astra6 реализовала opt-in GPU timestamps (кандидат38b2e377).
Host/GPU oracle/build/replay/load PASS; реальный240s сеанс завершён без ошибок,
1092 samples проверены, drops/truncation/API failures=0. Профиль указывает на
длинные draw-участки и staging copy для дальнейшего исследования; FPS-прирост,
сцена и render/game-time не приняты. Default/release сохранены.
[Результат](kb:FPS_GPU_TIMESTAMPS_20261103).

2026-11-02: Astra6 добавила opt-in direct vkGetFenceStatus для нулевых опросов.
Host/GPU oracle и сборка7029daf1 PASS;блокирующие wait/prefix/lifetime сохранены.
Экономия микротеста≈0.1µs/call,заметный FPS-выигрыш не заявлен;default off.
Следующий приоритет: GPU timestamps FSI/Resolve/copy. Release не менялся.
[Результат](kb:FPS_FENCE_STATUS_20261102).

2026-11-02: coherent0/1 runtime оба240s,child0/fatal0/errors0,полные120–180s
CPU/GPU/IP и raw swaps. Пользователь подтвердил «Да, лучше стало» — субъективное
улучшение текущей комбинации batching1/descriptor1/coherent1.
Окна не matched:Resolve1800→41837,draw106196→3881056,GPU29.9→91.85%;
swap29.998→19.373 не является регрессией/приростом от этой API-правки.
Release/default не менялись,оба собственных запуска завершены.
[Evidence](logs/readback-coherent-invalidate-20261101/runtime-comparison.json), [оценка пользователя](logs/readback-coherent-invalidate-20261101/user-acceptance.json).

2026-11-01: Astra6 добавила opt-in coherent GeneralReadback invalidate elision.
Actual allocation flags+completed ticket проверяются;noncoherent/unknown path сохранён.
Host/GPU oracle PASS:invalidate32→0,bytes/guards совпали. Noncoherent GPU недоступна,
её ветка host-injected only. Timing неоднороден,FPS не доказан,default/release прежние.
[Результат](kb:FPS_COHERENT_READBACK_20261101).

2026-11-01: Astra6 добавила opt-in immutable Resolve descriptor cache64keys.
Host/GPU binding oracle PASS:128→4vkUpdateDescriptorSets,bytes/guards сохранены.
Изолированный72554319,default off;timing неоднороден,FPS/hit-rate не измерены.
[Результат](kb:FPS_VULKAN_API_20261101).

2026-10-31: bounded batching0/1 runtime240s оба child0/fatal0/errors0.
Наблюдалось14.57→17.58swap/s (+20.7%),1%low12.29→15.58;batch mean3.295,max4.
Artifact/config/settings/profile одинаковые,но сцена/маршрут не подтверждены:
причинный FPS-выигрыш и render/game-time acceptance остаются открытыми.
Оба запуска завершены,release/default не менялись.
[Evidence](logs/resolve-batching-20261031/runtime-comparison.json).

2026-10-31: Astra6 реализовала opt-in Resolve batching4jobs/16MiB,default off.
Host ASan/UBSan и GPU oracle PASS:40Resolve→14vkQueueSubmit,точные bytes,
все40публикаций,одинаковый peak40,325,120B. Изолированный кандидат готов;
На этапе сборки gameplay/FPS не измерялись; новые runtime результаты выше.
[Результат и ограничения](kb:FPS_RESOLVE_BATCHING_20261031).

**Цель >30FPS пока не достигнута.** По запросу «найди другие решения»
исследованы три механизма: освобождение завершённых малых readback-слотов,
ожидание последнего нужного fence и объединение Resolve submissions.
Первый кандидат d9998da1 собран Astra6 и прошёл host/GPU oracle: callbacks
ожидания ёмкости31→23, но transfer-only тест медленнее на5.2–19.5%; FPS-выигрыша нет.
Второй вариант5bf5a83f собран и проверен: prefix32 передаёт1fence вместо32,
сохраняя bytes/free-list и обработку потери устройства. Игрового FPS A/B ещё нет.
Batching требует изменения контракта submitted jobs и пока остаётся направлением реализации.
[Другие решения и evidence](kb:FPS_READBACK_ALTERNATIVES_20261030).

Forward128 остаётся отдельным opt-in: GPU-written staging≈3%медленнее libc.
Первый240s runtime17.86swap/s,1%low14.39; контроль закрылся до120s, FPS A/B нет.
Оба новых флага default off; FPS-кандидаты не установлены поверх release.
Текущий runtime нового кандидата8e2bae61 отличается от исторического544bdaae:
сравнивать flag0/1 на одном кандидате. Release GPU также независимо обновлён
до27de8fab; новый эксперимент не является принятым обновлением этого релиза.
[Прежний технический результат](kb:FPS_FORWARD_COPY_20261029).

## Сборка и артефакты

Исходный аудит2026-10-29 проверил CMakeCache/build.ninja, compiler metadata и release1.0:

- Host: Release, Clang23.1.4 (прежний compiler cache23.1.3), `-O3 -DNDEBUG`,
  `-mcmodel=large -msse4.1`, generated PPC также `-gline-tables-only`.
  Debug-информация строк не означает Debug-оптимизацию. LTO/-march=native нет.
- SDK multi-config: у Release runtime `-march=x86-64-v2 -O3 -DNDEBUG`,
  `-fno-strict-aliasing -ffp-model=strict`; LTO нет. Debug/RelWithDebInfo записи
  в том же Ninja-файле не означают, что Release линкует Debug runtime.
- SDK commit `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`, с локальными патчами.
  Полные хеши текущих четырёх артефактов совпали с принятой baseline.
  Тот аудит выполнен без пересборки; последующие SDK-кандидаты собраны изолированно.
  Fast-math/LTO и -march=native не добавляли.
- Manifest: `tmp/acv-rexglue-linux-indexed/acv_linux_probe_manifest.toml`;
  включает три boundary-файла из tools/native-rexglue. Это codegen manifest,
  runtime-настройки поступают через canonical acv.config и профильный backend.
- Linux: GTX1660 SUPER6GiB, driver615.78.08; runtime log Vulkan1.4.363.
  perf CLI отсутствует; имеются nvidia-smi, RenderDoc и собственный
  perf_event_open sampler. Доступность perf events на PID11835 проверена; доступность на новом PID проверять заново; perf_event_paranoid=2 не изменялся.
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
| Новый CPU/GPU/IP замер |29.02 swap/s /34.46мс /нет1%low|GPU92.4%; пользователь уточнил: первая миссия, gameplay|
| Forward128 внесён |17.86swap/s /56.00ms /1%low14.39|On-runtime0errors; контроль неполный; GPU-staging≈3%медленнее, default/release прежние|
| Idle-slot reclaim d9998da1 |Игровых замеров нет|GPU bytes PASS,capacity wait callbacks31→23,transfer-only время хуже5.2–19.5%|
| Latest-fence5bf5a83f |Игровых замеров нет|GPU prefix32:1handle вместо32,bytes/fences/reuse PASS,default off|

Источники предыдущих экспериментов:
[readback](kb:FPS_READBACK_PROGRESS_20261027),
[CPU/copy](kb:CPU_PARALLEL_READBACK_20261027),
[streaming](kb:RESOURCE_STREAMING_20261027).
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

Принятый runtime сохраняет агрегаты300swap-интервалов. Отдельный bounded
per-frame trace уже реализован и проверен в диагностических кандидатах;
он дал raw1%low для forward1, но полного matched control пока нет.
Кандидаты не заменяют release1.0.

## Продолжение: сбор CPU/GPU/IP — 2026-10-29

Добавлен [perf_capture.py](tools/native-rexglue/perf_capture.py).
[Host regression](tools/native-rexglue/test_perf_capture.py) запускает реальные
профилировщики на отдельном Python-процессе: одинаковый PID/start, перекрытие
окон, hashes/log snapshot, запрет перезаписи и CLI gate>=90с проверены;
ранний выход наблюдаемого процесса не принимается как успех.
Результат host-проверки не является игровым baseline или проверкой render/pacing.
Пользователь затем запустил игру; первый игровой замер описан ниже.
Пользователь уточнил первую миссию и мелкие зависания; маршрут пока не задан.
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
не объявляется корректной по чистому60-секундному окну. После замера пользователь
уточнил: первая миссия, gameplay, мелкие зависания.
[Аннотация пользователя](logs/fps-baseline-20261029-session161152-02/user-annotation.json).

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
историческая сцена не идентифицирована, настройки cache различаются, performance-правок нет.
## Повтор первой миссии и мелкие зависания

[Второй60с профиль](logs/fps-baseline-20261029-session161152-03/analysis.json),
[настройки/identity](logs/fps-baseline-20261029-session161152-03/session.json).
Тот же PID/session; начало на471.15с,21:19:44–21:20:44(+05:00).
Пользователь: первая миссия, gameplay, мелкие зависания; маршрут/камера не заданы.

60CPU/GPU строк,5791IP,lost0/unavailable0. Пять полных300swap окон:
29.3226swap/с,34.1034мс,окна28.03–30.14/с; max49.10мс.
Пауза802.88мс первого замера в этих окнах не повторилась.
GPU93.65%(80–97%); GPU Commands71.22%,DrawThread52.63%,Main20.80%ядра.
Физические disk reads0байт при231.5MB логических чтений; нельзя считать эту
просадку доказанно дисковой. В окне warning/error нет. Это повтор наблюдения
той же миссии, не контролируемый replay/A/B и не доказательство устранения зависаний.

### Горячий DrawThread: raw guest evidence

Ghidra MCP отсутствует; CLI status показал daemon stopped, без запуска/реимпорта.
Использован локальный raw dump, [bytes/listings/SHA](logs/fps-swap-trace-20261029/guest-hotspots.json).
.pdata подтверждает124байта0x82AF5708–0x82AF5784.
Внутри0x82AF5738–0x82AF5750 повторяются вызов0x82BB58A8 и unsigned compare с2.
Leaf getter0x82BB58A8–0x82BB58CC(32байта, без.pdata owner) читает слова
device+0x4248/+0x4250, возвращает их uint32-разность в result+8.
Цикл повторяется, пока разность>=2. Это активный опрос device counters.
Роль счётчиков/их writers и связь конкретной паузы с ними ещё не установлены;
не заменять цикл sleep/yield и не объявлять такой обход FPS-исправлением.
Доля двух функций в повторном DrawThread IP sample:27.60%+27.13%.

## Отдельный покадровый диагностический кандидат

[Header](tools/native-rexglue/swap_trace.h),
[воспроизводимый SDK patch](tools/native-rexglue/patches/bounded-swap-trace.patch),
[анализатор](tools/native-rexglue/analyze_swap_trace.py),
[ASan/UBSan host regression](tools/native-rexglue/test_swap_trace.py).
[Build manifest](tmp/acv-swap-trace-20261029/build.json),
[точные команды](tmp/acv-swap-trace-20261029/commands.json),
[изолированная сборка](logs/fps-swap-trace-20261029/build-isolated.py).

GPU15b043d589394e86c8b5588727f0d0f0a945ac32df247dda8b995dfc0aca1e16.
ELF/runtime/FFX скопированы с принятыми SHA; релиз и SDK output не заменены.
Статус: source available, built, host-tested; **игровой запуск кандидата не выполнен**.
dlopen/local dependency resolution, no-launch settings/environment dryrun,
exact patch replay и hashes PASS. Shader/render/readback algorithms не менялись.

Опционально ACV_SWAP_TRACE включает bounded16384записей с CPU steady timestamps:
arrival перед limiter, paced перед завершением swap handler, done после IssueSwap.
Ни одного файла/буфера без opt-in; одна ограниченная heap allocation перед первой
записью, без allocation/file writes на каждом кадре. При достижении60с окно
сохраняется один раз; output открывается эксклюзивно. Capacity/shutdown/неверный
порядок timestamps помечаются incomplete; потерянные/обрезанные записи не принимаются.
Default age gate120с, минимально90с; без установленного opt-in ABI/лимитер
и вызов IssueSwap сохраняются. Это CPU wall-time, не GPU timestamps/present count.
Запись файла в конце окна может сама дать последующую паузу; она вне замеренных
timestamps. Небольшой overhead часов и диагностики надо учитывать в сравнении.

Анализатор считает1%low =1000/mean самых медленных ceil(N×0.01) raw интервалов,
не1000/p99. Отдельно показывает CPU время после предыдущего paced, между swap
calls и внутри limiter; post-pacing включает IssueSwap и прежнюю статистику handler.
Существующий5с stage trace остаётся inclusive и не даёт причинную привязку
каждого GPU wait к одному кадру.

Первый Ninja build после CMake regeneration зацепил ненужную FFX shader
generation через Wine и завершился macro-redefinition error. Не повторялся.
Кандидат собран отдельно: новый PCH, один изменённый C++ TU и relink с существующими
объектами/библиотеками; без генерации shader permutations и без изменения SDK output.
Полные диагностические записи в SQLite stage fps_trace_build.

Прежний подготовленный trace-only сценарий: нормально выйти из текущей игры (её профиль занят writer
lock), затем [start.sh](tmp/acv-swap-trace-20261029/start.sh).
Он использует существующий packaged probe/profile backend, тот же UUID/settings,
canonical acv.config, Wayland,2560×1080; максимум240с, принятый релиз не заменяет.
К120с войти в первую миссию; с120с по180с играть по тому же участку без меню/паузы.
ACV_SWAP_TRACE_SECONDS=60, stage trace включён явно. Выход и профильный lock
обслуживает существующий probe; userdata во время записи не клонируются.
Результат появится в новом logs/fps-swap-diagnostic-*/swaps.jsonl и stages.jsonl.
Этот launch ещё не проверен в runtime; данный start.sh относится к текущему
профилю/машине, новые настройки в schema не добавляет.

После записи:
```bash
node tools/decomp-logging/cli.mjs run --stage fps_swap_analysis -- python3 tools/native-rexglue/analyze_swap_trace.py logs/fps-swap-diagnostic-ID/swaps.jsonl
```

Далее: по trace отделить редкие паузы от устойчивого GPU/readback ограничения,
проверить одну гипотезу на том же участке без смены разрешения, теста FSR effects
или нарушения coherence. Forward128 реализован как opt-in; ускорение GPU/FPS не подтверждено.

Предыдущие версии изменённых документов сохранены и сверены:
[SHA-256](logs/fps-capture-20261029/docs-before/sha256.json).

## Продолжение на текущей игре: memcpy — 2026-10-29

По последнему запросу пользователя тестирование продолжено на прежнем PID без
перезапуска/изменения настроек. Локализованы Copy/Worker и точные размеры/адреса
непересекающихся page-aligned копий. Подготовлен отдельный opt-in forward128
кандидат6090b52a; он включает bounded raw-swap диагностику прежнего15b043d5.
[Технический результат](kb:FPS_FORWARD_COPY_20261029),
[build](tmp/acv-forward-copy-20261029/build.json).
Позднее forward1 runtime выполнен, контроль закрылся до120s; A/B не подтверждён.
Astra6 добавила GPU-written staging и protected-page проверки; GPU результата
ускорения нет. Подробности в техническом документе выше.
В start.sh обоих кандидатов явно сохранены accepted env defaults:
general1/parallel2/backoff0; packaged probe сам их не добавляет.
Архив до этого обновления: [SHA](logs/fps-forward-copy-20261029/docs-before/sha256.json).

## readback_resolve = full (решение пользователя, 2026-11-07)

После обновления SDK до v0.10.1 (пакет 2.2) пользователь увидел чёрные детали мехов и территории. Файлы и их
данные при этом совпадают с 2.1 (трассировка `ACV_TRACE_RESOURCE_IO`: нет расхождений по хешам для совпадающих
диапазонов), окно, шимы и список перекомпилированных функций тоже совпадают. Пользователь выбрал вернуть
`readback_resolve = full` в `tools/native-rexglue/acv.config` (пакет 2.2 и репозиторий). Цена: `fast` давал ≈26–27 fps
против ≈16–17 fps у `full` в замерах ниже (разные запуски, не контролируемый A/B). Причина подтверждена пользователем в игре
(2026-11-07): с `full` чёрных деталей нет, с `fast` они были. Механизм (гонка отложенного readback на новом SDK) не изучен.

## readback_resolve = fast (решение пользователя, 2026-11-06)

Выбор пользователя: `readback_resolve = fast` в `[sdk]` файла `tools/native-rexglue/acv.config`
(копия в пакете ACV-Linux-release-2.1). Режимы SDK: `none` (по умолчанию SDK), `fast` (данные предыдущего
кадра, копия каждый кадр), `some` (предыдущий кадр, копия при промахе кэша), `full` (синхронное чтение,
точное, но стоит ожидания GPU).

Приоритет опций: `probe_linux.py` сначала добавляет профиль совместимости ACV-FSI
(`--protect_zero=false --render_target_path_vulkan=fsi --readback_resolve=full`), затем `[sdk]` из конфига;
`acv_settings.dedupe_options` оставляет последнее значение, поэтому действует `fast`.
Проверено 2026-11-07: dry-run Linux-лаунчера даёт `--protect_zero=false --readback_resolve=fast`;
Windows-лаунчер (`backend.py launch-spec`, `acv_launch_spec.py`) собирает тот же порядок. Тест
`test_probe_readback_profile.py` проходит.

Замеры (разные запуски, не контролируемый A/B): `tmp/rb-correct/fast.log` ≈ 26–27 fps,
`full.log` ≈ 16–17 fps, `fsi-profile.log` ≈ 19–20 fps (limit=60). Через Wine Windows-проба
с `fast` показала 15–60 fps в зависимости от сцены.

Не проверено: корректность изображения и CPU-generated atlas в режиме `fast` (чтение запаздывает на кадр;
выключение readback раньше ломало atlas). SDK не пишет эффективный режим в лог — факт применения
подтверждается только командной строкой и логикой выбора режима в `graphics/command_processor.cpp`.

## Исторический объём начальной разведки

Этот пункт относится к исходному аудиту до последующих source/SDK кандидатов.
Тогда выполнялось только сохранение evidence, baseline, отчёта и указателей в планах.
Код SDK, CMake, runtime config, игровые данные и принятые артефакты не изменялись.
Performance-коммитов нет: без контролируемого замера нечего принимать как ускорение.
Основной риск дальнейшей работы — ускорить swap/пропуски вместо реальной симуляции
или нарушить CPU↔GPU coherence. Приёмка: >30 на том же участке, улучшение frametime
и 1% low, сохранённые материалы/объекты и темп логики, без новых runtime ошибок.
