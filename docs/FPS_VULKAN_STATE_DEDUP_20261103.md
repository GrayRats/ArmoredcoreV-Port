# Vulkan state dedup — 2026-11-03

2026-11-03: готов отдельный ACV-Linux-release-2.0 со stencil3 и предыдущими
Vulkan API доработками. GPU3TU и TRG пересобраны; неизменённые guest ELF/runtime/FFX
переиспользованы. ASan/UBSan, backend, UI600frames,189hashes/loader и135s startup
PASS (child0/fatal0/errors0). Исправлен выбор2.0 вместо сохранённого пути1.0;
пользовательский settings/config не переписан. Версия1.0 сохранена.
Миссия/FPS/stencil-image acceptance открыты.

[Запуск2.0](../ACV-Linux-release-2.0/start.sh),
[checkpoint/manifest hashes](logs/release2-20261103/checkpoint.json).
Начальная UI30frame проверка завершилась до готовности backend; независимый
snapshot прошёл, UI600frame повтор завершился backend_ready1. Это не падение
игры. Canonical acv.config ARTEM и существующий профильный lock/backup backend
сохранены. Startup135s выполнялся с Wayland diagnostic override; обычный start.sh
сохраняет X11 default. Полная сборка SDK/FFX/Wine/codegen не запускалась.

2026-11-03: по запросу пользователя проверенные Vulkan API доработки включены
в ACV-Linux-release-1.0. GPU/default env и packaging baseline обновлены,
189file manifest и release135s smoke PASS (child0/fatal0/errors0).
Старые файлы с хешами сохранены. Три дополнительных stencil-фильтра собраны,
host-tested,default off в отдельном кандидате; GPU render acceptance открыт.

[Публикация и release smoke](logs/vulkan-finalize-20261103/publication.json),
[проверка189файлов](logs/vulkan-finalize-20261103/manifest-check.json),
[rollback hashes](logs/vulkan-finalize-20261103/release-before/sha256.json),
[дополнительный stencil-кандидат](logs/vulkan-stencil-20261103/result.json).
Пять установленных filters + batching/descriptor/coherent/fence-status включены
в start.sh. Унаследованный flag=0 позволяет диагностически выключить конкретную
опцию. Счётчики и timestamps без диагностических env не пишутся.

Дополнительные SetStencilReference/CompareMask/WriteMask используют раздельные
кэши точного(faceMask,value), сбрасываемые каждым bind pipeline. Один общий
`ACV_VULKAN_DEDUP_STENCIL_CONSTANTS=1` включает их только в новом кандидате
`tmp/acv-vulkan-stencil-20261103`; default0. Front/back/combined masks и смена
значений проверены host ASan/UBSan. Сборка/RTLD_NOW PASS; GPU rendering не проверен.
[Patch](tools/native-rexglue/patches/vulkan-stencil-state-dedup.patch).
Новый эксперимент не устанавливался в итоговый пакет.

Sol реализовала пять opt-in фильтров в общем deferred decoder: index bindings,
vertex bindings, push constants, blend constants и depth bias. Общий helper:
[buffer_bind_cache.h](tools/native-rexglue/buffer_bind_cache.h).
Точные параметры и байты сравниваются перед Vulkan-вызовом. Кэши локальны одному
Execute/command buffer. Blend/depth cache сбрасывается при каждом bind pipeline;
vertex диапазон вне32slots и push constants больше256bytes идут обычным путём.
Другие push updates заменяют ключ, поэтому overlapping/stage/layout изменения
не скрываются. Vulkan lifetime, барьеры и содержимое команд не изменены.

Переключатели `ACV_VULKAN_DEDUP_BUFFER_BINDS`,
`ACV_VULKAN_DEDUP_PUSH_CONSTANTS`, `ACV_VULKAN_DEDUP_DYNAMIC_CONSTANTS` (=1).
Каждый default off; release не заменён. FSI и другие backend используют общий
код, но точность изображения каждого пути остаётся непроверенной.

Host ASan/UBSan проверяет exact repeats, смену offset/type/stages/layout,
перекрывающиеся bindings, fallback и сброс состояния. Compute GPU oracle проверяет
пропуск второго одинакового push update и exact snapshots/guards. Это не GPU
render oracle для vertex/index/dynamic state. Изолированная сборка/RTLD_NOW PASS.
Ошибка include ordering Vulkan headers исправлена до успешной сборки; в версии
с counters дополнительно исправлено фактическое имя acv_gpu_stage_profile.h.

## Проверка в игре

Набор со всеми переключателями1 прошёл240s: child0/fatal0/runtime errors0,
1101 timestamp samples. [Полный сеанс](logs/vulkan-buffer-binds-20261103/runtime-01/runtime-result.json).
Затем кандидат со счётчиками прошёл135s, child0/fatal0/runtime errors0.
Счётчики включаются только при ACV_GPU_STAGE_PROFILE после90s, выводятся раз в5s.
Восемь полных окон суммарно40.0366s; последний неполный хвост не включён:

| API | Входные команды | Vulkan-вызовы | Пропущено |
|---|---:|---:|---:|
| BindIndexBuffer | 2647167 | 2474045 | 173122 (6.54%) |
| PushConstants | 63504 | 60866 | 2638 (4.15%) |
| BindVertexBuffers | 0 | 0 | не использовался |
| SetBlendConstants | 0 | 0 | не использовался |
| SetDepthBias | 0 | 0 | не использовался |

[Фактические counters и scope](logs/vulkan-api-counts-20261103/runtime-01/counters.json),
[копия runtime log](logs/vulkan-api-counts-20261103/runtime-01/runtime.log),
[manifest кандидата](tmp/acv-vulkan-api-counts-20261103/build.json).
API сокращение измерено на decoder boundary, не является процентом экономии
кадра или доказательством FPS. Сцена/маршрут не контролировались. Rendering,
игровое время и полный save/load не приняты. Собственные процессы завершены.

## Следующий шаг

Проверять index/push на matched сцене и CPU frametime; vertex/dynamic фильтры
не включать в вывод о выигрыше для этого workload. В byte-profile отдельного240s
сеанса3516копий/9размеров,430больше4MiB; staging-copy интервалы включающие.
[Размеры копий](logs/gpu-copy-bytes-20261103/runtime-01/copy-byte-summary.json).
Снижение объёма требует отдельной проверки destination extents/неизменяемых байтов;
пропускать копии по одному совпадению адреса нельзя.

Для повторного counter замера: кандидат
`tmp/acv-vulkan-api-counts-20261103/start.sh 0` с нужными тремя flags=1;
135s, timestamps off, post90s counters on через существующий stage profile.
Сохраняются acv.config ARTEM, профильный writer lock и проверяемая backup.
[Основной SDK patch](tools/native-rexglue/patches/vulkan-api-state-dedup.patch),
[добавление счётчиков](tools/native-rexglue/patches/vulkan-api-dedup-counters.patch).

Локальные Khronos references pinned090f1b190d60ced4a1d198fd3747d071cc271b1c:
drawing.adoc BindIndexBuffer, fxvertex.adoc BindVertexBuffers,
descriptorsets.adoc5236–5258 (push update and pipeline layout compatibility).
