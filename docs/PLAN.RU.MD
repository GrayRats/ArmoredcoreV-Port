# ACV — текущий план

2026-11-05: игровой Vulkan backend ReXGlue теперь запрашивает API 1.4,
проверяет результат запроса версии loader и отклоняет loader/GPU ниже 1.4.
Linux Release и Windows MinGW-targeted Clang сборки `rexui` прошли. Полная
Windows SDK/game сборка останавливается на MSVC CRT настройке vendored FFmpeg;
Wine gameplay, native Windows WSI/package и macOS приёмка открыты. Заголовки
ReXGlue закреплены на 1.4.357.0; LunarG 1.4.363.1 остаётся SDK/runtime launcher.
[Очередь Windows](kb:PLAN_WINDOWS).

2026-11-04: Windows/all-OS Vulkan checkpoint — launcher .exe связан с
LunarG SDK 1.4.363.1; приложение требует Vulkan API 1.4. Wine с LunarG loader
1.4.363 не нашёл совместимый Wine ICD bridge; отдельный запуск через встроенный
Wine loader завершил 80-frame Vulkan renderer smoke, но Windows Python backend
отсутствует. Linux SDK 1.4.363.1, Vulkan/OpenGL smoke по 80 кадров и CTest 3/3
прошли; Python portability tests 39/39. Native Windows WSI/game, полный Windows
ReXGlue build/bridge/package, macOS/MoltenVK и межплатформенный FPS открыты.
Профилирование NOT ASSESSED; игровой релиз не менялся.

2026-11-04: отдельный [ACV-Linux-release-2.1](tools/ACV-Linux-release-2.1/start.sh)
скомпилирован и упакован. Пересобраны243 native TU, слинкованы две библиотеки;
хеши пакета, синтаксис launcher и зависимости проверены. v2.1 не запускалась и
не проходила gameplay-тест; это кандидат. Release2.0 и принятый runtime baseline
сохранены. [Сборка и границы](kb:ACV_LINUX_RELEASE_2_1_20261104).

2026-11-04: persistent driver cache подключён также к двум lazy render-target
ownership-transfer pipelines; handle и callback очищаются до teardown владельца.
Изменение включено в сборку v2.1. Compile/package checks прошли, GPU и gameplay
acceptance открыты.
[Результат и границы](kb:FPS_VULKAN_RENDER_TARGET_CACHE_20261104).

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
[Evidence](tools/acv-context/logs/readback-coherent-invalidate-20261101/runtime-comparison.json), [оценка пользователя](tools/acv-context/logs/readback-coherent-invalidate-20261101/user-acceptance.json).

2026-11-01: Astra6 добавила opt-in coherent GeneralReadback invalidate elision.
Actual allocation flags+completed ticket проверяются;noncoherent/unknown path сохранён.
Host/GPU oracle PASS:invalidate32→0,bytes/guards совпали. Noncoherent GPU недоступна,
её ветка host-injected only. Timing неоднороден,FPS не доказан,default/release прежние.
[Результат](kb:FPS_COHERENT_READBACK_20261101).

2026-11-01: Astra6 добавила opt-in immutable Resolve descriptor cache64keys.
Host/GPU binding oracle PASS:128→4vkUpdateDescriptorSets,bytes/guards сохранены.
Изолированный72554319,default off;timing неоднороден,FPS/hit-rate не измерены.
[Результат](kb:FPS_VULKAN_API_20261101).

Runtime0/1 по240s завершены exit0/errors0:наблюдалось14.57→17.58swap/s,1%low12.29→15.58,batchmean3.295/max4. Сцена/маршрут не подтверждены:причинный прирост не принят,релиз не менялся. Далее matched сцена/render/game-time.

Текущий запрос: Astra6 внесла opt-in Resolve batching4jobs/16MiB. Host/GPU oracle PASS,40→14submits с одинаковыми bytes/памятью; отдельный кандидат готов. Далее: matched первая миссия batch0/1,actual batch distribution,frametime/render/game-time. FPS-прирост не заявлен. [Результат](kb:FPS_RESOLVE_BATCHING_20261031).

Обновлено: 2026-10-30. [English](kb:PLAN).

## Текущая сборка-кандидат

[ACV-Linux-release-2.1/start.sh](tools/ACV-Linux-release-2.1/start.sh) содержит
TRG Launcher, статически перекомпилированную игру, игровые данные и библиотеки.
Сборка и пакетные проверки завершены; запуск v2.1 и gameplay-тест не выполнялись.
Release2.0 сохранён, v2.1 пока не принята как runtime baseline. Это native
Linux-сборка через ReXGlue, а не полностью восстановленный исходный код игры.

## Сделано

- В локальный пакет внесены Host/Guest-счётчики MangoHud, видимое описание дизеринга, три проверенных пресета и адаптивный бюджет VRAM. Проверены19настроек/137значений. Startup-проверка с лимитом30 даёт согласованные30кадр/с; миссионное расхождение пока не воспроизведено. [Подробности](kb:LAUNCHER_SETTINGS_20261030).

- Исправления миссий и параллельное копирование readback приняты пользователем и включены в релиз.
- Реализованы UltraWide FOV/HUD и коррекция отсечения по краям; пользователь подтвердил улучшение.
- Реализованы профили, резервные копии/импорт/восстановление, настройки и действия лаунчера.
  Preview не пишет на диск; обновление сохраняет правки; сброс графики оставляет правки лаунчера несохранёнными.
- Интегрированы лимиты кэша текстур, время удержания и ограниченная подгрузка архивов.
- Исправлен русский заголовок; доступны лимиты 30/60/120 и 36 пресетов разрешения с фильтром групп.
- CAS/FSR1 интегрированы без тестов эффектов; FSR2/3 убраны из интерфейса.
- Проверки текущего пакета, действий лаунчера и запуска проходят; оригинальные
  конфигурация, сохранения и личные настройки сохранены.

## Осталось в задаче загрузки ресурсов

- Воспроизвести задержку загрузки или преждевременную выгрузку на конкретной миссии/действии.
  Принятая коррекция отсечения исправляет видимость по краям; время жизни ресурсов — отдельная задача.
- Найти оставшуюся задержку создания, распаковки или управления временем жизни ресурсов;
  доработать удержание/подгрузку с ограничением памяти, затем проверить повторные
  переходы и потребление памяти. Ускорение загрузки и lifetime моделей/локаций пока не доказаны.

Диалоги выбора файлов/каталогов подключены; выбор через системное окно вручную не проверен.
Стабильные 60 FPS и полный цикл сохранения/загрузки в игре не подтверждены; они
пока не приняты; FPS возобновлён явным запросом2026-10-29. CAS/FSR не тестировать; проверки разрешения —
только 2560×1080. Профилирование gameplay начинается только через 90 секунд.

## Активный запрос FPS — другие решения, 2026-10-30

Цель >30FPS с правильным рендером/темпом логики остаётся открытой. Astra6 собрала
два opt-in кандидата: idle-slot reclaim d9998da1 и latest-required-fence5bf5a83f.
Host ASan/UBSan и GPU data/fence oracles PASS. Reclaim уменьшает callbacks ожидания
ёмкости31→23, но transfer-only тест медленнее5.2–19.5%. Latest-fence передаёт1handle
вместо prefix32, сохраняя completion и ошибки; FPS не измерен.
Оба default off; эти FPS-изменения не устанавливались в релиз.
Прежнее подготовленное сравнение: один latest-кандидат с флагом0/1, actual fence-prefix distribution и matched
первая миссия с raw frametime/1%low/render/game-time проверкой. Его runtime — текущий
release8e2bae61; старые6090/d999 сохраняют544bdaae. Для batching submissions нужен
recorded/submitted/failed lifecycle. Forward128 GPU copy≈3%медленнее;
прежний игровой контроль завершился до окна, поэтому FPS A/B отсутствует.
[Альтернативы](kb:FPS_READBACK_ALTERNATIVES_20261030), [отчёт](tools/acv-context/report-fps.md).

## Подробности

[Очередь реализации](kb:PLAN_GUI) · [Лаунчер/пакет](kb:LAUNCHER_PACKAGE) ·
[Подгрузка ресурсов](kb:RESOURCE_STREAMING_20261027) · [Исправление отсечения](kb:ENGINE_OFFSCREEN_20261028).

Прежний текст плана сохранён с SHA-256 в [архиве](tools/acv-context/logs/plan-compression-20261028T175039Z/manifest.json).
