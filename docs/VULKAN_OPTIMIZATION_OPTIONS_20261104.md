# Дополнительные варианты Vulkan API — 2026-11-04

Исходный source-аудит до реализации; описания прежнего поведения ниже относятся
к [snapshot](logs/vulkan-options-20261104/evidence.json). Все семь направлений
теперь реализованы opt-in: [реализация и проверки](kb:FPS_VULKAN_SEVEN_20261104).
Прирост FPS не доказан; принятая release2.0 сохранена. Guest/PPC XenonRecomp
не изменялся.

Дополнение 2026-11-04: persistent driver cache подключён также к двум
render-target ownership-transfer pipeline calls и включён в сборку
[ACV-Linux-release-2.1](tools/ACV-Linux-release-2.1/start.sh). Compile/package
проверки прошли; сама v2.1 не запускалась, GPU/game acceptance открыты.
[Статус и patch](kb:FPS_VULKAN_RENDER_TARGET_CACHE_20261104).

## 1. Сохраняемый VkPipelineCache — первый кандидат против микрозависаний

В [pipeline_cache.cpp:3483](tools/rexglue-sdk/src/graphics/vulkan/pipeline_cache.cpp)
vkCreateGraphicsPipelines получает VK_NULL_HANDLE. Имя класса PipelineCache
не означает использование driver VkPipelineCache; существующие shader/PSO
описания и async compilation не заменяют этот Vulkan объект.
Создавать кеш на device, загружать совместимые bytes и сохранять между сеансами.
Проверять vendor/device/pipelineCacheUUID и валидность файла, ограничить размер,
писать атомарно. При отказе загрузки строить пустой кеш с проверкой VkResult.
При параллельном создании pipelines учитывать external synchronization кеша:
lock или per-worker caches с merge. Destroy после остановки workers/device work.
Проверка: matched cold/warm miss counts, create-time p95/p99, одинаковые shaders,
image checks; исправление stutter не выдавать за рост steady-state FPS.
[Khronos pipeline cache](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_cache/README.html).

## 2. Synchronization2 с точными scopes, а не механическая замена API

[command_processor.cpp:3186](tools/rexglue-sdk/src/graphics/vulkan/command_processor.cpp)
OR-объединяет src/dst stages, :3298 передаёт общие masks в legacy barrier.
Barrier2 позволяет хранить scopes у каждого buffer/image barrier. Это кандидат
на уменьшение лишнего ожидания FRAGMENT/COMPUTE/TRANSFER для разных ресурсов.
Текущий код уже объединяет одинаковые pending barriers и разделяет зависимые
перекрывающиеся ranges; эти разделения нельзя просто удалить.
Нужен synchronization2 feature (core1.3 либо extension), PFN и legacy fallback.
Сохранять HOST visibility, FSI/EDRAM ordering, layouts, aliasing и ownership.
Проверка: same-input readback/guards, sync validation при наличии, image checks,
GPU intervals и barrier counts. Простая замена имени вызова не доказывает выигрыш.
[Khronos sync2](https://docs.vulkan.org/guide/latest/extensions/VK_KHR_synchronization2.html).

## 3. Timeline semaphore для submission tickets

[command_processor.cpp:5739](tools/rexglue-sdk/src/graphics/vulkan/command_processor.cpp)
ждёт fences, :6055 создаёт новые при пустом free-list, :6236 submits с fence.
Timeline counter может заменить per-submission completion objects и опрос
префикса на get-counter / wait-value. Это глубже уже выполненного fence polling.
Feature timelineSemaphore (core1.2/extension), корректный signal stage и
монотонные tickets обязательны. Сам submit2 не даёт автоматического ускорения.
Readback publication, sparse-bind ordering, device loss и teardown сохраняются;
WSI acquire/present binary semaphores не заменять вслепую.
Проверка: прежний completion-order oracle, errors/loss/retirement, число API
poll/reset calls и matched frametime. Высокая сложность и риск lifetime-регрессий.
[Khronos timeline](https://docs.vulkan.org/samples/latest/samples/extensions/timeline_semaphore/README.html).

## 4. Graphics pipeline libraries

Тот же [pipeline_cache.cpp:3483](tools/rexglue-sdk/src/graphics/vulkan/pipeline_cache.cpp)
создаёт монолитный graphics pipeline. VK_EXT_graphics_pipeline_library позволяет
переиспользовать vertex-input/pre-raster/fragment/output части для разных PSO.
Это отдельный контроль от сохранения VkPipelineCache: один переиспользует части,
другой сохраняет driver representation между запусками.
Проверить extension/features, fast linking свойства драйвера, совместимость
layouts/render-pass interfaces и fallback на текущий monolithic path.
Проверка: link/create p95/p99, число созданных библиотек и память, image oracle.
Ожидаемая область — новые pipelines/stutter, не скорость готового draw.
[Khronos GPL](https://docs.vulkan.org/samples/latest/samples/extensions/graphics_pipeline_library/README.html).

## 5. Extended dynamic state для сокращения вариантов PSO

[pipeline_cache.cpp:1460](tools/rexglue-sdk/src/graphics/vulkan/pipeline_cache.cpp)
кладёт topology/restart в description, :1516 front-face; :3408 уже делает
viewport/scissor и часть native state динамическими. Следующий шаг — только
остающиеся применимые ключи: topology/restart/cull/front-face/depth-stencil
через соответствующие extended_dynamic_state features.
Нельзя повторно считать уже динамические поля. В FSI часть Xenos state
эмулируется shader logic: native dynamic state не заменяет её.
Ключ pipeline надо менять вместе с записью обязательных CmdSet и invalidation.
Проверка: число PSO variants, create time/CPU command cost, image acceptance
по backend; поддержка и скорость не предполагаются по одному номеру Vulkan.
[Khronos extended dynamic state2](https://docs.vulkan.org/samples/latest/samples/extensions/extended_dynamic_state2/README.html).

## 6. Suballocation staging memory вместо индивидуальных allocations

GeneralReadback slots в
[command_processor.cpp:4895](tools/rexglue-sdk/src/graphics/vulkan/command_processor.cpp)
создают buffers, :4926 выделяет memory в соответствующей ветке.
Кандидат — bounded chunks/ring с suballocations и постоянным mapping вместо
отдельных driver allocations на каждый новый slot. Текущий pool уже переиспользует
слоты: не обещать экономию allocation API на каждом кадре.
MemoryTypeBits/alignment/atomSize, coherent/noncoherent publication, cacheline
sharing и запрет reuse до completion обязательны. Не увеличивать64MiB budget
без отдельного измерения. Проверка: peak allocation/buffer counts, growth churn,
allocation stalls, guards/memory/lifetime oracle. Не уменьшает объём PCIe копий.
[Khronos allocation](https://docs.vulkan.org/guide/latest/memory_allocation.html).

## 7. Отдельная transfer queue для GeneralReadback copy

[vulkan_device.cpp:470](tools/rexglue-sdk/src/ui/vulkan/vulkan_device.cpp)
явно предпочитает одну queue; GeneralReadback copy записывается в graphics/compute
submission (command_processor.cpp:5015). Отдельная copy queue может перекрыть
staging transfer и независимые следующие draws, если GPU/драйвер имеют полезный
параллелизм. Совпадение числа queues не доказывает наличие отдельных engines.
Сначала проверить queue-family topology и независимость следующих writes в
shared memory; semaphore dependency producer→copy и copy→consumer/next-writer,
queue ownership либо корректный concurrent sharing, sparse residency и CPU
publication обязательны. При раннем CPU wait дополнительная очередь может
проиграть из-за synchronization overhead. Нельзя просто перенести CmdCopyBuffer.
Проверка: отдельный same-input oracle для пересекающихся поколений, GPU overlap,
input latency/frametime, exact readback bytes и memory budget.
[Khronos queues/async](https://docs.vulkan.org/samples/latest/samples/performance/async_compute/README.html).

## Порядок и исключённые повторы

Начать с1 и измерения compilation stalls; для steady-state GPU исследовать2.
Затем3. Направления4/5 зависят от числа PSO variants и поддержки;6 от allocation
churn;7 — отдельный высокорисковый эксперимент после dependency trace.
Для всех оставить отключаемый fallback до relevant acceptance.

ResetCommandPool уже выполняется (:6191), ONE_TIME_SUBMIT уже установлен,
barrier dedup уже есть. Повторные binds/state, coherent invalidate skip,
Resolve descriptor cache/batching, fence-status poll и новый descriptor-bind
фильтр не считаются новыми пунктами этого списка.
