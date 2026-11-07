# ACV — очередь реализации и приёмки

## Очередь — обновлено 2026-11-06

Порядок: сверху приоритетнее. Каждый пункт имеет приёмку; сборка, видимое окно или
сдвиг одного счётчика приёмкой не считаются.

1. **Производительность Linux, matched-сцена.** Изоляция 2026-11-05 на меню:
   синхронный `readback_resolve=full` даёт основную потерю (sdk 25.5 → 11.1 swap/с;
   FSI −14 %). `some` и `fast` дают +20 % и +50 % относительно `full`, но корректность
   картинки для них не проверена. Приёмка: одна миссия и маршрут, не меньше трёх повторов
   на каждую сторону, `[acv-display] pacing` и загрузка GPU по сэмплам; для режима, который
   предлагается включить по умолчанию, отдельная проверка картинки.
   [Отчёт](kb:FPS_READBACK_ISOLATION_20261105).
2. **Guest-счётчик MangoHud.** Обновляется по 300 swap: при 10 swap/с это около 30 с.
   Нужен интервал по времени (патч `command_processor.cpp` и пересборка runtime).
   Приёмка: строка обновляется не реже раза в 2 с при 10 swap/с, без падений.
   Host FPS отключён, потому что считает показы окна.
3. **Loose loading (ACVLooseLoader).** Выполнено: распаковка архива, перепаковка карт,
   запуск в staging без падений (`fatal=0`), строгий разбор карт 42 из 42 непустых.
   Осталось: загрузка миссии с loose-картами (модели, текстуры, звук) и сравнение с базовым
   запуском на той же миссии. [План](PLAN.LooseLoader.MD).
4. **Контекст для агентов.** База знаний перенесена в `tools/acv-context/docs/`; корневой `docs/`
   убран из проекта. Тесты плагина 7 из 7, индекс собирается из проекта или из плагина.
   Осталось: при установке плагина вне репозитория проверить fallback на его `docs/`; старые
   ссылки на `docs/` в `tmp/docs-move/` и архив `archive/docs-2026-11-06.tar.gz` оставлены как резерв.
5. **Windows.** [PLAN_WINDOWS](kb:PLAN_WINDOWS): сборка и смоук-тесты без нативной приёмки.

2026-11-05: ReXGlue game Vulkan backend now targets API 1.4 and checks both the
loader version query and physical-device API before creation. Linux Release
`rexui` and Windows MinGW-targeted Clang `rexui` builds pass. Full Windows SDK
build still stops in vendored FFmpeg's MSVC CRT configuration; Windows game
runtime, Wine gameplay, native WSI and package acceptance remain open.
[Implementation queue and evidence](kb:PLAN_WINDOWS).

2026-11-04: отдельный ACV-Linux-release-2.1 собран и упакован. 243 native TU
пересобраны, runtime/GPU libraries слинкованы; 189 payload hashes, launcher
syntax и shared-library resolution проверены. Это compile/package candidate:
v2.1 не запускалась и не получила gameplay/image/save acceptance. Release2.0,
runtime baseline и пользовательский config сохранены. [Отчёт](kb:ACV_LINUX_RELEASE_2_1_20261104).

2026-11-04: persistent driver cache подключён к двум lazy render-target
ownership-transfer pipelines. Callback lifetime очищается перед cache teardown;
default/no-cache путь сохранён. Включено в сборку v2.1; compile/link и package
checks прошли. GPU/game acceptance pending. [Status](kb:FPS_VULKAN_RENDER_TARGET_CACHE_20261104).

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

Обновлено: 2026-10-28. Краткий статус — [PLAN](kb:PLAN).
Этот файл хранит текущий объём, остаток и критерии; история запусков — в технических документах.

## Текущий результат

2026-10-30: MangoHud Host/Guest проверены в ограниченной сессии (30,0 / 30,0002 в startup; миссионное расхождение не воспроизведено). Видимое описание дизеринга, наборы Производительность/Графика/Плавность и адаптивный кэш внесены в release1.0. Аудит 19 настроек/137 значений, interface и package smoke PASS. Открыто: воспроизвести FPS-расхождение в миссии и принять качество эффектов отдельно; CAS/FSR image tests не выполнялись. [Проверки и границы](kb:LAUNCHER_SETTINGS_20261030).

[ACV-Linux-release-1.0/start.sh](../ACV-Linux-release-1.0/start.sh) — отдельная папка
с TRG Launcher, статически перекомпилированной игрой, данными и библиотеками.
Это Linux-сборка через ReXGlue, не полностью восстановленный исходный код игры.
Пакет, запуск из лаунчера и жизненный цикл блокировки профиля проверены.
Исправления миссий, параллельное копирование readback и коррекция UltraWide-отсечения
приняты пользователем и включены в текущий релиз.

## Этапы A–F

| Этап | Реализовано | Граница проверки |
|---|---|---|
| A — настройки | Общая схема, валидация, пресеты, возможности выбранной сборки, requested/applied и причины fallback | Полный запрос форматов/режимов игрового backend до запуска ещё не реализован; preview черновика его не заменяет |
| B — профили/сохранения | UUID/XUID, раздельные данные, writer lock, импорт, проверяемые backup/restore; запись файлов игрой подтверждена | Полный цикл с подтверждённым прогрессом и перенос между XUID не приняты; сейчас вне выбранного объёма |
| C — лаунчер/пакет | TRG, шесть страниц, асинхронный Python backend, установка из ISO, запуск/остановка, XDG и отдельный пакет | Проверки действий, рендера страниц и запуска проходят; ручной выбор через системные диалоги не проверен |
| D — экран/FOV/HUD | Режимы/разрешение/VSync/лимиты 30/60/120, расширенный обзор, центрирование HUD, боковые planes согласованы с проекцией | Улучшение отсечения принято пользователем; стабильные 60 FPS, точность прицела/маркеров и ролики полностью не подтверждены |
| E — цвет | RGB8, запрос 10 бит с явным fallback к 8 при отсутствии поверхности | Настоящий 10-битный вывод не подтверждён; YCbCr недоступен |
| F — графика | Анизотропия, FXAA, дизеринг, масштаб, CAS/FSR1 и резкость подключены | CAS/FSR интегрированы без тестов по указанию; FSR2/3 недоступны |

### Этап C — сохраняемые требования

- Использовать общую Python-схему и профильный backend; не дублировать политику в GUI.
- Preview не записывает настройки; обновление сохраняет черновик, сброс меняет
  только графику. Отмена возвращает последние сохранённые значения.
- Смена профиля заполняет его поля; список копий отделён от данных сессии.
- PLAY проверяет и сохраняет настройки, запускает обычную сессию без лимита probe.
  Закрытие GUI оставляет игру и writer lock живыми; второй writer запрещён.
- Установка из ISO использует staging/проверку/отмену без порчи существующих данных.
  Пакет запускается из любого cwd; оригиналы, XDG-сейвы и личные настройки сохраняются.
- Изменения проверять на реальном App/backend и в текущем пакете, а не только сборкой.

## Приоритет FPS: другие решения — 2026-10-30

2026-11-03: GPU timestamps38b2e377 реализованы Astra6. Приёмка диагностического
boundary пройдена: bounded64×32pool,completion-safe noWAIT retrieval,
host/GPU oracle,3TU build/replay/load и реальный240s runtime,1092samples без
errors/drops/truncation. [Результат](kb:FPS_GPU_TIMESTAMPS_20261103).
Следующая неопределённость: длинные включающие draw-участки и staging copy,
число команд/объём/зависимости. Для оптимизации нужны same-input bytes/guards,
снижение измеряемой стоимости и matched-scene FPS/render/game-time приёмка.
Не считать интервалы чистым FSI временем или процентами кадра. >30 остаётся открытым.
Direct fence-status polling7029daf1 — отдельная малая API-правка,default off;
[предыдущий результат](kb:FPS_FENCE_STATUS_20261102).

2026-11-02: coherent0/1 runtime по240s без runtime/fatal ошибок. Пользователь
подтвердил улучшение текущей комбинации;FPS A/B не принят:Resolve1800vs41837,
разная нагрузка. Далее matched сцена;release/default сохранены.

Текущая Vulkan API правка: coherent invalidate elision реализована;host/GPU
32→0APIcalls PASS. Следующая приёмка: matched0/1 с batching1/descriptor1,
реальныйFPS/render/game-time. [Evidence](kb:FPS_COHERENT_READBACK_20261101).

Новый запрос: Vulkan API descriptor cache реализован,host/GPU oracle128→4updates PASS.
[Результат и дальнейшая приёмка](kb:FPS_VULKAN_API_20261101).
Исходный scope2026-10-31: Astra6 проверяет
immutable Resolve destination descriptor cache64keys,exact buffer/offset/range,
default off,безопасный fallback и lifetime после GPU completion. Приёмка:
меньше vkUpdateDescriptorSets при одинаковых compute outputs/guards,
shared host/GPU regression,изолированный кандидат. Coherent invalidation и
fence polling пока отдельные source-гипотезы,не смешивать в этом A/B.

Текущий запрос2026-10-31: batching Resolve submissions реализован (Astra6),
4jobs/16MiB,default off. Приёмка: explicit recorded/submitted/failed lifecycle,
CPU-demand/slot/capacity/drain/Swap flush,ошибки без раннего освобождения staging;
shared host/GPU byte oracle PASS:40→14vkQueueSubmit на одинаковом входе,
отдельная сборка готова. Gameplay FPS/картинка/темп — последующая приёмка.
[Результат](kb:FPS_RESOLVE_BATCHING_20261031).
Runtime0/1 оба240s child0/errors0;наблюдалось14.57→17.58swap/s,
batchmean3.295/max4. Сцена не подтверждена,приёмка FPS/render/game-time открыта.

1. Astra6 реализовала два отдельных opt-in: idle-small-slot reclaim d9998da1
   и latest-required-fence5bf5a83f. Source/build, ASan/UBSan и GPU byte/fence oracles PASS.
   Оба default off; FPS-кандидаты не устанавливались поверх текущего release.
2. Reclaim: capacity wait callbacks31→23, но transfer-only oracle медленнее5.2–19.5%.
   Latest fence: prefix32 передаёт1handle вместо32; prefix/free-list,ошибки и
   EndSubmission device-loss guard проверены. Это не доказательство роста FPS.
3. Прежний подготовленный matched A/B — тот же latest-fence кандидат с флагом0/1, general1,
   parallel2,backoff0,Forward1280,reclaim0;2560×1080,первая миссия120–180s.
   Измерить actual prefix distribution,raw frametime/1%low; принять только
   с сохранёнными материалами/объектами/темпом игры и без новых ошибок.
   Новый кандидат использует текущий release runtime8e2bae61; старые6090/d999 —544bdaae.
   Не выдавать сравнение этих разных runtime за matched A/B.
4. Более крупный путь — batching Resolve submissions; перед кодом нужен явный
   recorded/submitted/failed lifecycle и CPU-demand flush. Просто убирать
   EndSubmission нельзя. Forward128≈3%медленнее на GPU staging, FPS A/B неполный.
   Кодовые изменения — Astra6 по запросу пользователя.
   [Альтернативы и evidence](kb:FPS_READBACK_ALTERNATIVES_20261030), [отчёт](report-fps.md).

## Текущая очередь: загрузка и время жизни ресурсов

Лимиты кэша текстур, удержание и ограниченная фоновая подгрузка архивов уже
реализованы и проверены. В наблюдавшихся сценариях преждевременное вытеснение
текстур не воспроизведено; большинство поздних чтений обслуживалось из RAM.
Исправленное UltraWide-отсечение не означает изменение политики выгрузки ресурсов.

1. Если задержка после возврата камеры или перехода сохраняется, зафиксировать
   воспроизводимую миссию/действие на текущем релизе и исходные показатели.
2. Разделить задержки IO, распаковки, создания ресурсов и управления их временем жизни.
   Не подавлять unload/refcount и не включать асинхронный IO без подтверждённого ownership.
3. Исправить установленную причину: удержание или зависимую предварительную загрузку
   с ограничением RAM/VRAM, сохранением ошибок, EOF и безопасности in-flight ресурсов.
4. Проверить тот же сценарий, повторные переходы и стабилизацию потребления памяти.
   Принятие: меньше повторных загрузок/пауз, нет пропавших ресурсов, утечек и повреждений сейвов.

Ускорение загрузки и lifetime моделей/локаций пока не доказаны.
Русский заголовок X11 исправлен и проверен; добавлены подсказка Linux ELF,
36 пресетов с фильтром 16:9/16:10/UltraWide/32:9, видимая подсказка пути к игре
и инструкция запроса 10 бит/обновление фактического формата. FSR2/3 убраны из UI.
Ручная проверка системных диалогов остаётся отдельным пунктом интерфейса.

## Ограничения текущего объёма

- CAS/FSR не тестировать; проверки игровых разрешений — только 2560×1080.
- FPS возобновлён явным запросом2026-10-29. Полный save/load, регрессия остальных
  миссий и контроллер/HiDPI остаются вне текущей очереди.
- Gameplay-профилирование/логирование — после 90 секунд от запуска процесса.
  Скриншоты/PNG для анализа запрещены.
- Рабочий путь — FSI/full; FBO/full с чёрным gameplay отклонён.
  Принятые артефакты и оригинальные config/save сохранять; правила — [AGENTS](kb:AGENTS).

## Подробности и история

- [Лаунчер, пакет и проверки интерфейса](kb:LAUNCHER_PACKAGE).
- [Подгрузка ресурсов: реализация и остаток](kb:RESOURCE_STREAMING_20261027).
- [Принятая коррекция отсечения](kb:ENGINE_OFFSCREEN_20261028).
- [Readback: изменения и ограничения](kb:FPS_READBACK_PROGRESS_20261027).

Прежние 610 строк, включая исходные критерии и историю, сохранены без изменений:
[текст](logs/gui-plan-compression-20261028T175246Z/PLAN_GUI.MD), [SHA-256](logs/gui-plan-compression-20261028T175246Z/manifest.json).
