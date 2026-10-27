# ACV — короткий контекст продолжения

Обновлено:2026-10-26. Исторический подтверждённый запуск: FSI/full PID73719 после отклонённого
FBO/full (чёрный gameplay подтверждён пользователем). FBO test child0/fatal0;
[checkpoint](../logs/acv-fbo-retest-20261026/checkpoint.json). Пользователь подтвердил отсутствие чёрного экрана и вход в gameplay.
Замер после137с: ~15FPS; [CPU/GPU](../logs/acv-fsi-full-confirmed-20261026/summary.json).
 Очередь реализации — [PLAN_GUI](../PLAN_GUI.MD), краткий
статус — [PLAN](PLAN.md), технические факты —
[NATIVE](NATIVE_REXGLUE_LINUX.md), этапы — [PROGRESS](PROGRESS.md).

- **Актуальный пакет (2026-10-27):** `../ACV-Linux-TRG-WideView-20261027/start.sh` — TRG, обновлённый рантайм
  (`librexruntime` ebf338a1…, содержит join/таймеры/WaitMultiple), хуки `wide_view` (UltraWide/16:10, эксперимент),
  RenderDoc по запросу, `system:` смонтирован. Верификация пакета и жизненного цикла GUI→игра пройдена; пользователь
  подтвердил работу. Не проверено: отсечение по краям, прицел, ролики, другие миссии. [LAUNCHER_PACKAGE](LAUNCHER_PACKAGE.md).
- **Сохранения проверены по файлам (2026-10-27):** сейв лежит под XUID профиля `00094AF2A434841E`
  (`APGD.DAT` 230672 B, `PAINT.DAT` 116248 B); в копии `tmp/acv-user-data-name-trial/` `APGD.DAT` перезаписан
  26.10 02:31 (размер тот же, хеш другой) — игра загрузила скопированный сейв и записала новый. Backup/restore на
  копии настоящих данных проходит (порча → исходный хеш, повреждённая копия отвергается). Не проверено: что внутри
  изменилось, перенос на другой XUID, другие миссии. [PLAN_GUI](../PLAN_GUI.MD).
- **Миссия работает — подтверждено пользователем 2026-10-25.** Обычная сессия без GDB
  `20261025T165411391172Z`, runtimef3600aca, изолированный профиль `tmp/acv-user-data-name-trial/`:
  180с, child0/fatal0. Оригинал сейва не изменён; принятие не означает безопасную миграцию
  всего профиля или завершённый save/load round trip.
- **Последний диагностический вылет был в GDB:** PID36997, SIGABRT, `thr != nullptr`
  в breakpoint.c:9182. Return observer заменён;160 потоков/3840 callbacks PASS.
  Опрос ввода каждый кадр по умолчанию выключен. Следующая работа — FPS в миссии
  без GDB и с прогретым cache; далее прежние UltraWide/экран/цвет/CAS/FSR.
  [Новый checkpoint](NATIVE_REXGLUE_LINUX.md#mission-accepted-and-debugger-failure-2026-10-25).
- **FPS: диагностика только после90с.** Split timers: full wait≈57%, memcpy≈13%,
 pending-slot waits0. Fast9217 раз ждал новое поколение из-за незавершённого прошлого;
 исправлен выбор поколения (host ASan/UBSan PASS), isolated GPU45dbaab6,
 tmp/acv-gpu-fast-generation-fix/. 180с child0/fatal0, всё ещё15–17FPS; GPUd2404533 с неблокирующей отправкой fast-копий:
 180с child0/fatal0, пользователь вошёл в gameplay, поздние22–25FPS, GPU88,4%.
 Картинка fast не принята; TimerQueue CPU fix подтверждён ниже. Full/default и
 accepted package сохранены; stable60FPS открыт. [Checkpoint](NATIVE_REXGLUE_LINUX.md#resolve-wait-attribution-and-fast-generation-correction-2026-10-26).
- **Завершённый тест таймеров:** PID69477, session20261025T215215281009Z,
 runtime7b1f69a0/GPUd2404533, tmp/acv-timer-queue-fix/. User вошёл в gameplay,
 CPU/IP gate90.00036s; при продолжении PID уже отсутствовал.
 Host1ms CPU10.03%→1.19%; callback/cancel/WaitMultiple/join PASS. Runtime90 rows/6393 IPs:
 timer CPU9.33%→0.64%, GPU≈88%, cadence21–24FPS;
 fast картинка не принята. [Таймеры](NATIVE_REXGLUE_LINUX.md#timerqueue-blocking-wait-and-deadline-validation-2026-10-26).
- **Новый CPU fix:** WaitMultiple сохраняет дробную часть1мс и соблюдает deadline
  при contention. Host99,5%→1,1%, Audio Worker78,05%→0,56% ядра в игре.
  Runtimeca04c1c6, сессия20261025T171212311161Z190с child0/fatal0.
  Пакет ../ACV-Linux-TRG-WaitFix-20261025/start.sh,185 файлов.
  Тяжёлая сцена16–21FPS, стабильные60 открыты. [Замеры и patch](NATIVE_REXGLUE_LINUX.md#waitmultiple-fractional-sleep-and-measured-cpu-fix-2026-10-25).
- **TRG checkpoint:** frontend C++17/SDL2/ImGui собран, UI/JSON/process и реальный ISO импорт проверены.
  Выбранный runtimef3600aca и игра запущены из GUI; закрытие GUI не снимает профильный lock.
  Новый отдельный кандидат `../ACV-Linux-TRG-20261025/start.sh`,185 файлов,Qt/ICU нет.
  Последний запуск `20261025T161149596052Z`: child0/fatal0, XUID совпал, shm без новых сирот;
  [checkpoint](../logs/acv-trg-final-20261025-r2/checkpoint.json). Пакет и игровая приёмка ещё не заменяют baseline; старый `../ACV-Linux/` сохранён.
  Последний источник реализации/ограничений — [LAUNCHER_PACKAGE](LAUNCHER_PACKAGE.md#trg-frontend-and-installation--2026-10-25).
- **Цель:** корректный native Linux gameplay. Пользователь разрешил исправления,
  сборки и bounded tests. Windows/Wine/Unity вне текущей задачи.
- **Правила:** [AGENTS](../AGENTS.md). ISO/assets/live Ghidra DB read-only;
  generated translation не первичный fix. SQLite logger для анализов/сборок/тестов,
  сохранять PCH/objects. Без screenshots/PNG; RDC/replay/raw bytes допустимы.
- **Решено:** потеря динамической текстуры из-за выключенного Resolve readback.
  Профиль acv-fsi в probe_linux.py теперь добавляет --readback_resolve=full.
  SDK config и --extra могут переопределять; sdk profile не меняется.
  ARTEM/ARTEM/headless=true config сохранён побайтно, SHA31ef1d2a….
- **Доказательство:** исходный RGBA resource0x4350A980, pixels0xFE191000,
  physical0x1E192000,1024x512,2MiB. None →2MiB нулей; full →2066418 ненулевых
  bytes при одинаковых resource/native headers. Компрессор и конвертер получают
  содержимое вместо пустого изображения.
  [A/B](../logs/acv-readback-origin-20261022/ab-chain.json).
- **GPU:** run20261022T172345196638Z/frame34282/event75,4158indices, declared
  slot0,BC3 physical0x1F6B3000. GPU512KiB точно равны CPU upload submission25699,
  SHA00c279ff…; texture center RGBA[0.223529,0.215686,0.223529,0.14902], раньше0.
  [Proof](../logs/acv-readback-origin-20261022/gpu-material-proof.json).
  Первый converter snapshot и поздний upload относятся к разным обновлениям:
  не требовать их равенства. Точная визуальная корректность всей сцены не принята.
- **Текущий пакет:** tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv_linux_probe,
  ELF SHAf61ee442…, fixed GPU90d8ef1a…, runtime d8fb56af….
  Resolve slot completion/reuse и invalidate-result guards интегрированы;
  staging округляется к64KiB. Full остаётся default. Последний120s run
  20261022T182231377426Z: child0,45 прежних diagnostics; global scene не принята.
- **Проверка интеграции:** [same-run CPU/GPU bytes](../logs/acv-integrated-readback-20261022/gpu-byte-proof.json),
  [normal GPU](../logs/acv-integrated-readback-20261022/release-gpu-proof.json).
  RGBA/BC3 SHA совпадают с accepted full; ASan/UBSan host publication tests прошли.
- **VFS/память:** ACV_TRACE_RESOURCE_IO включает bounded open/read/close и
  alloc/free-request trace.177 reads/≈32MiB совпали с source slices;16 source DCX
  декодированы с прежними SHA. Guest decompressor/queue ownership не проверены.
  [Technical details](NATIVE_REXGLUE_LINUX.md#integrated-resolve-completion-and-resource-tracing-2026-10-23).
- **Запуск:** tools/native-rexglue/test_linux.sh; canonical config сохранён.
  Сборка tools/native-rexglue/build_linux.py --jobs 4
  --gpu-plugin-artifact tmp/acv-gpu-texture-fix/librexgpu-xenos.so.
  Без override relink может вернуть installed plugin. Диагностика opt-in.
- **Осталось:** сопоставимый gameplay/scene regression и FPS full readback;
  VFS system:/dlc_* и две physical allocations0x40000/0x800000 protect0x20000400.
  Allocation override остаётся opt-in: прежние A/B сцены не совпали.
- **RE цепочка:** исходник → compressor0x82C82700/0x82C82090/0x82C82130 →
  physical0x1E112000 → converter0x82C83DE8 → physical0x1F6B3000. Raw PPC в
  logs/acv-texture-writer-20261022; source origin/A-B в logs/acv-readback-origin-20261022.
- **Ghidra:** scripts/ghidra-rpc.sh status перед использованием; headless
  ACV_RETRY.gpr /default.xex, saved --no-analyze. Listing неполон: read-bytes +
  Capstone fallback. DB не чинить. XEX SHA4fac8734…, image0x82000000,
  text0x822B0000,entry0x82B91F88, ReXGlue0.10.0.
- **Сохранение:** [проверенный прежний checkpoint](../logs/acv-readback-checkpoint-20261022T171359Z/verification.json),
  [состояние перед принятием fix](../logs/acv-readback-origin-20261022/docs-before-accepted-fix.sha256.json).

Upstream sync reference header — только пример; в runtime не подключён.
Рабочее исправление использует существующий full readback SDK, а не новый
barrier/fence workaround. Полные ограничения — в technical section NATIVE.

## Продолжение интеграции — 2026-10-23

Slot/fence/invalidate hardening и VFS/allocation tracing уже подключены и проверены.
Resolve/CPU consumer связан: physical0x0FCA7000,submission1298/frame627 →
first copy0x82C83918,2MiB побайтно равны SHA6af9819e…; затем tiled→linear→
RGBA0x1E192000→compressor. [Proof](../logs/acv-resolve-consumer-20261023/complete-proof.json).
Следующий пункт PLAN — отложенная публикация до guest fence/CPU first-read.
EVENT_WRITE_SHD сейчас сразу пишет guest completion, это надо учитывать. Generic async reference пока не заменяет
full в игре. Для немедленного CPU consumer необходимо актуальное поколение;
готовность прошлого кадра не подходит. FPS и p95/p99 ещё не измерены.

GPU основного пакета теперь fixed90d8ef1a… с исправленным texture exponent;
normal55937953… сохранён только как предыдущий checkpoint. Trace plugin отдельно tmp/acv-bake-chain-test;
его не переносить в normal package. Raw PPC/трассы в logs/acv-resolve-consumer-20261023.
Ghidra restarted headless, saved default.xex opened --no-analyze; DB не менялась.

Исходники инструментов/скрипты/MD сохранены в приватном
[DEArmoredcoreV](https://github.com/GrayRats/DEArmoredcoreV), initial451b6c4.
Данные игры, SDK, logs/tmp/cache и персональный config в GitHub не отправлены.
Проверка payload: scripts/check_repository_payload.py; подробности в PROGRESS.

Текущий приоритет пользователя: исправить чёрные модели gameplay. Deferred
prototype проверен на2MiB/47 samples, но не включён в normal GPU. Продолжать
с full capture frame37095/event69 и других opaque draws: shader inputs, lighting
и EDRAM writes; не подменять visual fix очередной async-оптимизацией.

**Подтверждено пользователем 2026-10-24:** правки помогли с рендером текстур
в игре. Причина: `exp_adjust` ошибочно брался из dword4 вместо dword3,
что давало множитель1/65536. Fixed GPU90d8ef1a… сохранять при пересборках.
[Техническая запись и checkpoint](NATIVE_REXGLUE_LINUX.md#texture-rendering-fix-confirmed-by-user-2026-10-24).
Дальше — регрессии оставшихся материалов, если они проявятся;
не считать этот исправленный descriptor bug снова неизвестной причиной.

Продолжение2026-10-24: same-input replay подтвердил RGB≈0.208 вместо3e-6
при неизменном texture sample — множитель65536 устранён.105s fixed run
завершился child0 с одним error system.properties; gameplay model regression
по автозахватам ещё не принят. Актуальный остаток — в текущей очереди PLAN.

Обычная `build_linux.py --jobs4` теперь сохраняет выбранный GPU из manifest
с проверкой hash: реальная сборка удержала90d8ef1a…, пять wrapper cases прошли.
Если source artifact изменён/потерян — явно выбрать preserved texture-fix SO.
Дополнительные event74/79 replay не приняты как same-input regression;
следующий функциональный пункт всё ещё gameplay material coverage.

system.properties классифицирован по raw PPC: missing-file ветка пропускает
parser и штатно возвращается; это startup properties override, не текстура.
VFS diagnostic сохранён, dummy files/mounts не добавлены. См. NATIVE section
Missing system.properties is handled и сохранённый classification.json.

Host f61ee442… содержит проверенный bounded UTF-8 profile decoder; Unicode
scalar/guest-buffer tests прошли, ARTEM55s headless smoke child0. GPU90d8ef1a…
и config98d1c2ed… сохранены; previous package в tmp/acv-profile-text-previous-package.
Это не исправление non-headless SDK keyboard dialog или tutorial glyphs.


## Resume snapshot — 2026-10-24

User confirms the mech texture rendering fix; do not reopen that bug. Current
scope is DLC file requests and GPU readback lifetimes. Final native package
`2d112d64…` uses rebuilt GPU `c2c9ae72…`, runtime `fa27bc22…` and unchanged
config `98d1c2ed…`; its75s X11 smoke exited0. Readback eviction now defers
freeing either staging slot until its submission ticket completes; actual-source
ASan/UBSan test, SDK/native build and startup runtime pass. High-pressure runtime
eviction was not forced.

All four emblem assets and indexed sound banks resolve. Latest log retains five
unavailable sound requests (four names absent from retail BHD5 and one literal
`$(voicelang)` with no matching locale candidates) plus optional
`system.properties`. The missing sound payloads are not in the current ISO.
See [DLC/GPU audit](NATIVE_REXGLUE_LINUX.md#readback-eviction-lifetime-and-file-open-audit-2026-10-24),
[queue](../PLAN_GUI.MD), and [progress](PROGRESS.md).

## Текущий resume — отдельная папка (2026-10-24)

Пользователь уточнил итог: игра + лаунчер отдельно от дерева разработки.
Собран `../ACV-Linux/start.sh`, runtime с XUID/лимитером/FidelityFX пока кандидат.
XDG профили/логи вне пакета; фактические runtime/Qt mappings локальны.
Профилировщик использует canonical probe и требует X11/чистый shm. Descriptor
flock наследует игра, чтобы гибель supervisor не разрешала второго writer.
[Проверки и оставшиеся ограничения](LAUNCHER_PACKAGE.md), очередь — PLAN_GUI.

Продолжение display: приоритет — UltraWide/16:10, FPS, экран, цвет, CAS/FSR.
SDK разделяет guest/window mode; вписывание/resize подтверждены матрицей3×35s.
Новые surface/output/cadence telemetry и экспериментальный10-bit SDR request
в launcher-display-output.patch. На этом X11 только8-bit; fallback проверен.
CAS/FSR shader passes исполнены, качество картинки/скорость игры ещё не приняты.
Native FOV/HUD и >60 остаются незавершёнными. Детали — LAUNCHER_PACKAGE.

UltraWide current reference: experimental wide_view and TRG setting now support
UltraWide/16:10; user accepted the experimental output. Accepted separate
WideView package is described in LAUNCHER_PACKAGE; current hooks/census are in
NATIVE_REXGLUE_LINUX. Edge culling, reticle, cut-scenes and other missions still
need acceptance. Older one-callsite-only experiments are historical.

- Latest built/host+GPU-tested candidate: tmp/acv-fitting-slot-fix/,
  host08045fae/runtimeebf338a1/GPUf1ccdfb9/accepted FFX0933159a. Minimal-prefix
  admission plus oldest-fitting-slot retirement are implemented.64-slot/64MiB
  limits retained; invalid/failed/uncommitted slots cannot be reused. New
  admission/fitting-reuse/full-drain metrics retain the >=90s gate.
- Latest candidate has NOT been launched in the game. Last game run was menu-only
  GPUc558cb37/session20261027T043019725818Z,180s child0/fatal0; ended.
  No gameplay60FPS claim. Synthetic admission1.298x and mixed-pool1.280x are
  separate component results and must not be treated as/multiplied for FPS.
- All owned build/test sessions completed at save. Scripted A/B has NOT started.
  Offline clones tmp/acv-readback-script-control and ...-candidate contain
  identical userdata/cache copies, made under source writer lock/verified backup.
  Reuse12:Return/20..100:Space sequence from existing census sessions; begin
  sampling after120s, without concurrent build/replay/GPU tests. User mission
  preference can override the diagnostic scenario. Preserve original saves,
  canonical config, current WideView package and all earlier candidates.
- Resume with the full ledger and explicit prepared/unstarted distinction:
  [FPS_READBACK_PROGRESS_20261027](FPS_READBACK_PROGRESS_20261027.md),
  [latest hashes/tests](../logs/acv-readback-fitting-slot-20261027/checkpoint.json),
  [comparison plan/backup](../logs/acv-readback-fitting-slot-20261027/scripted-comparison-plan.json).
