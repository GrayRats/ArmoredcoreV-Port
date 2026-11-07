# ACV — актуальная точка продолжения

## Сводка (читать первой)

- Кандидат: ACV-Linux-release-2.1. Сборка и проверки пакета пройдены; запуск, gameplay, image и save acceptance не заявлены.
- Предыдущий релиз ACV-Linux-release-2.0 и runtime baseline сохранены; пользовательский config не менялся.
- Открытые приёмки по записям 2026-11-03 и 2026-11-04: FPS/render Vulkan-путей, миссия и stencil-image для 2.0.
- Следующий шаг: пользовательский gameplay-тест — только по отдельному запросу.
- Датированные записи ниже (новые сверху) хранятся в базе: `kb.py rows X360_PROJECT_STATE --kind dated --limit 5`.


## Журнал (датированные записи, новые сверху)

2026-11-04: ACV-Linux-release-2.1 собрана в отдельном каталоге и проверена на
уровне пакета. Изменения включают семь Vulkan путей и persistent cache для двух
render-target pipelines. 243 native TU пересобраны, payload 189 файлов,
`start.sh`/зависимости проверены. По текущему запросу выполнены только build и
package checks: v2.1 не запускалась, gameplay/image/save acceptance не заявлены.
Release2.0, baseline и пользовательский config сохранены. Следующий шаг —
пользовательский gameplay-тест по отдельному запросу. [Release report](tools/acv-context/docs/tools/acv-context/docs/ACV_LINUX_RELEASE_2_1_20261104.md).

2026-11-04: реализованы все 7 opt-in Vulkan путей: persistent cache, sync2,
timeline, vertex-input GPL, dynamic cull/front-face, staging suballocation и
вторая copy queue того же family. Native runtime/GPU пересобраны (243 TU,
без старого PCH). ASan/UBSan, GPU bytes/pixels, deferred/patch replay PASS;
cold/warm runtime135s оба child0/fatal0/errors0; cache reload=true. Прирост FPS и gameplay
не приняты; release2.0 сохранена. [Результат](tools/acv-context/docs/tools/acv-context/docs/FPS_VULKAN_SEVEN_20261104.md).

2026-11-04: реализован opt-in exact descriptor-bind dedup (a8abd3a1).
Host ASan/UBSan/build/replay/load/check-only PASS. GPU oracle скомпилирован,
но отложен из-за активной пользовательской игры; FPS/render не проверены.
Release2.0 сохранён. [Следующая проверка](tools/acv-context/docs/tools/acv-context/docs/FPS_DESCRIPTOR_BINDS_20261104.md).

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
FPS/render acceptance открыты. [Результат](tools/acv-context/docs/tools/acv-context/docs/FPS_VULKAN_STATE_DEDUP_20261103.md).

2026-11-03: Sol собрала opt-in Vulkan API state dedup: index/vertex bindings,
push constants, blend constants и depth bias. Host ASan/UBSan и compute push
GPU oracle PASS; render/gameplay набора ещё не проверен. Copy-bytes runtime240s
чистый,3516копий/9размеров; FPS-прирост не доказан.

2026-11-03: Sol добавила copy_bytes в GPU timestamp профиль staging-копий.
Суммирование coalesced команд и сброс слота проверены ASan/UBSan; actual GPU
oracle bytes/guards и метаданные PASS, 3TU кандидат собран. Новый игровой замер
с размерами ещё не выполнен; FPS-прирост не заявлен.

2026-11-03: GPU timestamp кандидат38b2e377 готов (Astra6); host/GPU/build PASS,
реальный240s запуск завершён,1092 records проверены без drops/errors.
Продолжение: длинные draw-участки и staging copy; scene/FPS/render acceptance
открыты,release/default прежние. [Evidence](tools/acv-context/docs/tools/acv-context/docs/FPS_GPU_TIMESTAMPS_20261103.md).

2026-11-02: Astra6 добавила opt-in direct vkGetFenceStatus для нулевых опросов.
Host/GPU oracle и сборка7029daf1 PASS;блокирующие wait/prefix/lifetime сохранены.
Экономия микротеста≈0.1µs/call,заметный FPS-выигрыш не заявлен;default off.
Следующий приоритет: GPU timestamps FSI/Resolve/copy. Release не менялся.
[Результат](tools/acv-context/docs/tools/acv-context/docs/FPS_FENCE_STATUS_20261102.md).

2026-11-02: coherent0/1 runtime оба240s,child0/fatal0/errors0,полные120–180s
CPU/GPU/IP и raw swaps. Пользователь подтвердил «Да, лучше стало» — субъективное
улучшение текущей комбинации batching1/descriptor1/coherent1.
Окна не matched:Resolve1800→41837,draw106196→3881056,GPU29.9→91.85%;
swap29.998→19.373 не является регрессией/приростом от этой API-правки.
Release/default не менялись,оба собственных запуска завершены.
[Evidence](tools/acv-context/docs/logs/readback-coherent-invalidate-20261101/runtime-comparison.json), [оценка пользователя](tools/acv-context/docs/logs/readback-coherent-invalidate-20261101/user-acceptance.json).

2026-11-01: Astra6 добавила opt-in coherent GeneralReadback invalidate elision.
Actual allocation flags+completed ticket проверяются;noncoherent/unknown path сохранён.
Host/GPU oracle PASS:invalidate32→0,bytes/guards совпали. Noncoherent GPU недоступна,
её ветка host-injected only. Timing неоднороден,FPS не доказан,default/release прежние.
[Результат](tools/acv-context/docs/tools/acv-context/docs/FPS_COHERENT_READBACK_20261101.md).

2026-11-01: Astra6 добавила opt-in immutable Resolve descriptor cache64keys.
Host/GPU binding oracle PASS:128→4vkUpdateDescriptorSets,bytes/guards сохранены.
Изолированный72554319,default off;timing неоднороден,FPS/hit-rate не измерены.
[Результат](tools/acv-context/docs/tools/acv-context/docs/FPS_VULKAN_API_20261101.md).

2026-10-31: bounded batching0/1 runtime240s оба child0/fatal0/errors0.
Наблюдалось14.57→17.58swap/s (+20.7%),1%low12.29→15.58;batch mean3.295,max4.
Artifact/config/settings/profile одинаковые,но сцена/маршрут не подтверждены:
причинный FPS-выигрыш и render/game-time acceptance остаются открытыми.
Оба запуска завершены,release/default не менялись.
[Evidence](tools/acv-context/docs/logs/resolve-batching-20261031/runtime-comparison.json).

2026-10-31: Astra6 реализовала opt-in Resolve batching4jobs/16MiB,default off.
Host ASan/UBSan и GPU oracle PASS:40Resolve→14vkQueueSubmit,точные bytes,
все40публикаций,одинаковый peak40,325,120B. Изолированный кандидат готов;
На этапе сборки gameplay/FPS не измерялись; новые runtime результаты выше.
[Результат и ограничения](tools/acv-context/docs/tools/acv-context/docs/FPS_RESOLVE_BATCHING_20261031.md).

2026-10-30: локальный ACV-Linux-release-1.0 обновлён до `2026-10-30-settings-review`. MangoHud Host/Guest согласованы около30/s в112s startup, child0/fatal0; миссионное расхождение не воспроизведено. Все собственные проверки завершены. Пресеты/дизеринг/адаптивный кэш, аудит19настроек/137значений, пакет189hashes и UI smoke PASS. [Результат и границы](tools/acv-context/docs/tools/acv-context/docs/LAUNCHER_SETTINGS_20261030.md). CAS/FSR image acceptance и gameplay FPS остаются открытыми.

2026-10-30: по запросу «найди другие решения» Astra6 реализовала два opt-in
кандидата: idle-slot reclaim d9998da1 и latest-required-fence5bf5a83f.
Build/ASan/UBSan/GPU oracle PASS; gameplay новых кандидатов не запускался.
Reclaim capacity waits31→23, но transfer-only время хуже5.2–19.5%; latest-fence
передаёт1handle вместо32 в проверенном prefix32. FPS-прироста пока нет в evidence.
Оба default off; FPS-кандидаты не устанавливали в release. Чужие процессы/работа
не изменялись. Новый кандидат использует текущий release runtime8e2bae61;
исторические6090/d999 с544bdaae сохранены. Текущий release GPU27de8fab изменён
параллельной работой; C-кандидат не объявлен текущим release плюс один патч.
Сравнивать flag0/1 на одном кандидате,перед продвижением сверить актуальный source.

Следующий шаг: latest-fence0/1,первая миссия120–180s,2560×1080; general1/parallel2/
backoff0,Forward1280,reclaim0. Нужны actual prefix distribution,raw frametime/1%low,
render/game-time acceptance; >30 остаётся открытым. Batching — отдельная более
глубокая задача recorded/submitted/failed lifecycle, не удаление EndSubmission.
[Альтернативы и воспроизведение](tools/acv-context/docs/tools/acv-context/docs/FPS_READBACK_ALTERNATIVES_20261030.md),
[отчёт](tools/acv-context/docs/report-fps.md), [предыдущий Forward128 результат](tools/acv-context/docs/tools/acv-context/docs/FPS_FORWARD_COPY_20261029.md).
Forward128 GPU staging≈3%медленнее; его контроль закрылся до120s, FPS A/B отсутствует.

2026-10-28: интерфейс TRG обновлён в существующем ACV-Linux-release-1.0,
patch `2026-10-28-ui-id-fix`. Исправлен конфликт ID двух списков разрешений;
предупреждение ImGui оставлено включённым. Видимая подсказка исполняемого файла,
36 пресетов разрешений/фильтр и инструкция 10-битного запроса обновлены. Окно игры: `ARMORE CORE V [Rexglue]`. Русский заголовок X11 исправлен; добавлены
лимиты 30/60/120, дополнительные разрешения и фактический статус 10/8 бит; FSR2/3 убраны. Убраны пользовательские ссылки на PLAN;
исправлены черновик настроек/preview, сброс графики и формы/копии профиля.
[Изменения и проверки](tools/acv-context/docs/tools/acv-context/docs/LAUNCHER_PACKAGE.md#interface-polish--2026-10-28),
[publication](tools/acv-context/docs/logs/acv-launcher-polish-20261028/publication.json).
Игровые ELF/runtime/GPU/FFX принятые, их SHA сохранены.
Ниже — принятая native правка и сохраняющиеся ограничения игры.

Обновлено2026-10-28. Native off-screen visibility fix принят пользователем:
«Да, помогло». [Детали](tools/acv-context/docs/tools/acv-context/docs/ENGINE_OFFSCREEN_20261028.md),
[checkpoint](tools/acv-context/docs/logs/acv-engine-offscreen-20261028/checkpoint.json).

Исправлено несовпадение local16:9 frustum с расширенной projection2560×1080.
Scope RAII связывает коррекцию с реально расширенной projection данного builder.
Actual retail sphere/SDK ASan/UBSan, FOV/HUD regression и build PASS.
Финальный160с20261028T012252054423Z child0/fatal0; на обоих потоках все eligible
patched (49467/18449 в последнем отчёте). Диагностика после90с.

ELF2fc173a9 (изменён только заголовок окна; прежний efa4756c сохранён) включён в существующий ACV-Linux-release-1.0; manifest186files и
baseline обновлены. Прежний ELF/metadata с SHA в logs/acv-engine-offscreen-20261028/release-before.
Runtime544bdaae/GPUfe4550d5/FFX0933159a, general1/parallel-copy2/backoff0.
Release frontend35с PASS: local libraries/environment, child0/fatal0, lock lifecycle.
Все собственные тестовые игры завершены; итог в checkpoint.
Оригинальные config/save SHA сохранены. CAS/FSR и другие разрешения не тестировать.

Это исправление отсечения видимых боковых объектов. Resource unload/refcount,
объекты за камерой и прежняя политика кэша не подавляются; отдельная задержка
повторной загрузки и стабильные FPS не доказаны этим результатом.

Предыдущий завершённый этап:

Последний запрос выполнен: отдельная итоговая папка
[ACV-Linux-release-1.0/start.sh](tools/acv-context/docs/tools/ACV-Linux-release-1.0/start.sh),
186файлов (~3.88GiB), текущий TRG/Streaming-r2 runtime и игровые данные.
[Release checkpoint](tools/acv-context/docs/logs/acv-release-1.0-20261027/checkpoint.json),
[пакет](tools/acv-context/docs/tools/acv-context/docs/LAUNCHER_PACKAGE.md). Прежние пакеты/baseline сохранены.
Новый релиз не закрывает перечисленные ниже engine/FPS ограничения.

Последняя техническая задача: [Очередь](kb:PLAN_GUI),
[текущие изменения/остаток](tools/acv-context/docs/tools/acv-context/docs/RESOURCE_STREAMING_20261027.md).

Последний запрос: все настройки acv-launcher и оптимизация загрузки/выгрузки
локаций, моделей и ресурсов. Он расширяет прежний FOV/HUD-only scope.
CAS/FSR интегрированы **без тестов**, иные игровые разрешения не тестировать.
Диагностика gameplay только после90с. Скриншоты/PNG анализа запрещены.

Новый отдельный кандидат:
ACV-Linux-TRG-Streaming-20261027-r2/start.sh (`../../ACV-Linux-TRG-Streaming-20261027-r2/start.sh`, исторический пакет; каталог сейчас отсутствует),
185файлов, без Qt/ICU. ELF6c02660d/runtime544bdaae/GPUfe4550d5/FFX0933159a.
General deferred1 / parallel-copy2 / GPU-backoff0. Старый принятый FOVHUD пакет
и RUNTIME_BASELINE не заменены. Полные SHA и операции:
[checkpoint](tools/acv-context/docs/logs/acv-resource-streaming-20261027/checkpoint.json).

Добавлены общие настройки texture-cache1024/1536MiB/180s и readahead auto16MiB,
streaming/compatibility presets, своё разрешение/список режимов backend,
восстановление UI после ошибочных experimental settings. Один bounded archive
worker,8queued hints, fd inode lease; реальные чтения/сейвы/EOF неизменны.
Actual IO/eviction ASan/UBSan,5settings checks, patch replay, SDK/launcher build,
185hashes/dependencies,6UI pages и45s реальный frontend lock/lifecycle PASS.

Два210с сравнения завершены child0/fatal0. Sampling120–155с:
texture cache≈182MiB, evictions0 даже со старым384MiB soft limit;
чтение203–228MiB, диск0.8–3.5MiB. Большинство данных уже в RAM.
Preload399hints/0failures; это не доказательство ускорения загрузки.
**Модели/локации: преждевременная выгрузка не воспроизведена и не исправлена.**
Запрошена миссия/действие (вход, поворот камеры, возврат в область); ответа пока нет.
Далее нужен воспроизводимый переход, resource-build/decompression/lifetime trace,
потом ownership-safe retention/dependency preloading и longer memory plateau check.
Не подавлять Scr_UnloadInGameResource или refcount без доказательств.
NtReadFile SDK:213 принудительно синхронен; async ветка не реализована.
Не убирать true|| без корректных lifetime/APC/coherence/cancellation.
Вклад в задержки пока не измерен. Read-only raw82586050 подтверждает4cleanup calls;82A0E748 — virtual dispatch.

Оригинальные config/save SHA не изменены. Все собственные игровые/сборочные
процессы завершены; Ghidra RPC был доступен для чтения default.xex без анализа/
изменений DB. Прежние активные docs полностью сохранены с SHA:
[снимок](tools/acv-context/docs/logs/acv-resource-streaming-20261027/docs-before/sha256.json).

Стабильные60FPS, world-projected HUD/culling, настоящий10bit/YCbCr/temporalFSR
не доказаны этим этапом. FSI/full рабочий, FBO/full с чёрным gameplay отклонён.
Низкие16MiB/7F mirror и bulk Reset/Save/Restore вне общего deferred-пути.
[AGENTS](kb:AGENTS): команды через logger, оригиналы/сейвы сохранять,
generated PPC не исправлять как основной слой.



