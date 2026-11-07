# Другие решения оптимизации readback — 2026-10-30

По запросу пользователя поиск продолжен за пределами Forward128. Кодовые
изменения выполняет Astra6. Прирост FPS этим этапом пока не подтверждён;
этот FPS-этап не устанавливает кандидатов поверх release1.0. Прежние
Forward128 и idle-reclaim standalone checkpoints сохранены.

## На чём основан выбор

Историческое окно120–180s сессии171732:60.00829s,41442 Resolve
(690.60/s),20598 fitting-slot calls и столько же pool waits. На ожидания пула
пришлось33.57% wall GPU Commands. CPU-demand waits0,full drain0;
41443 admission calls заняли31.667ms. Таймеры inclusive и вложены друг в друга:
их проценты нельзя складывать или считать гарантированным приростом FPS.
[Исходный stage summary](logs/fps-forward-test-20261029/on-01/stage-summary120-180.json),
[разбор source и pinned reference](logs/fps-alternatives-20261030/source-findings.json).

Трасса выделяет давление на выделенный staging pool, но не содержит распределения
размеров/занятости слотов. Она не различает64slot cap и64MiB cap и не доказывает
фрагментацию свободными малыми буферами. Это проверяет новая условная диагностика.

## 1. Освобождение завершённых малых слотов

Прежний allocator выбирает свободный подходящий слот. Когда лимит достигнут,
он ожидает занятой подходящий слот; освобождение накопленных маленьких буферов
происходит только после отсутствия такого слота и полного drain. Поэтому
свободные маленькие allocations могут удерживать бюджет при наличии занятого
большого буфера.

Astra6 добавила opt-in `ACV_GENERAL_READBACK_RECLAIM_IDLE=1`, default off.
Политика создаёт tentative VkBuffer, получает его точный VkMemoryRequirements.size,
планирует достаточное освобождение только завершённых меньших слотов и использует
тот же буфер для binding. Все выбранные IDs проходят retire/collect до любого
free. При невозможности полного безопасного плана используется прежний путь;
ошибка retirement сохраняет ownership и возвращает failure. VkResult allocation/
binding/mapping проверяются. CPU coherence и порядок публикаций не изменены.

[Политика](tools/native-rexglue/readback_pool_selection.h),
[SDK patch](tools/native-rexglue/patches/idle-readback-reclaim.patch),
[host regression](tools/native-rexglue/readback_pool_reclaim_test.cpp),
[GPU oracle](tools/native-rexglue/vulkan_readback_pool_reclaim_test.cpp).
Лимиты сохранены:64slots/64MiB allocations,64jobs/64MiB pending queue.
При существующем ACV_GPU_STAGE_PROFILE после90s, не чаще раза в5s, записываются
allocated/pending/requested/idle-small и причины count/byte pressure.

Одинаковые32 GPU-written поколения в порядке AB/BA, GTX1660 SUPER:

| Метрика | Прежний путь | Reclaim |
|---|---:|---:|
| Fitting wait callbacks |31|0|
| Admission wait callbacks |0|23|
| CPU wait callbacks |1|1|
| Максимум pending jobs |1|9|
| Peak allocated bytes |63,774,720|67,002,368|
| Опубликованные поколения |32|32|
| Время пары AB,ms |96.5145|101.520|
| Время пары BA,ms |95.3996|114.020|

Exact bytes,guards,новейшие перекрывающиеся поколения и сохранение частичной
CPU-записи PASS. ASan/UBSan покрывают точное округление allocations,оба cap,
занятые leases,неосуществимый план без частичного free,ошибки/исключения retirement;
прежние selection/bridge/lease regressions PASS.
**Ускорения нет:** transfer-only тест медленнее на5.2% и19.5%. Он включает
allocation churn и CPU oracle checks.31→23 — число callbacks ожидания ёмкости,
не измеренное время блокировки и не FPS. Выигрыш от перекрытия render/transfer
в игре ещё не показан. Validation layer не установлен/не включён;
fixture использует явный host accessor вместо всех guest page-fault aliases.

[Полный результат и operations](logs/readback-pool-reclaim-20261030/result.json),
[manifest отдельного кандидата](tmp/acv-idle-reclaim-20261030/build.json).
GPU SHA-256:d9998da19b29d8419dcf16824c36888e5bcef9ddf2c1b6ab6afae9f8ff427aba.
Он built/host-tested/GPU-oracle-tested; gameplay не запускался. Не включать
по умолчанию по одному уменьшению числа callbacks.

## 2. Ожидание только последнего нужного fence

SDK CheckSubmissionFenceAndDeviceLoss ожидает массив из всех fences до нужного
submission с waitAll=true. EndSubmission добавляет fence только после успешного
vkQueueSubmit в graphics_compute queue,index0; submission IDs монотонны.
Sparse binds связаны semaphore с этой очередью и не добавляются в tracked fences.

Pinned Vulkan-Docs commit090f1b190d60ced4a1d198fd3747d071cc271b1c,
chapters/synchronization.adoc2769–2795: scope более позднего fence signal включает
предшествующие fence signal operations в signal-operation order одной очереди.
Это позволяет ожидать один последний нужный fence и затем освободить тот же
завершённый prefix. Остальные очереди этим правилом автоматически не покрываются.

Astra6 реализовала `ACV_VULKAN_WAIT_LATEST_FENCE=1`,default off.
[Shared helper](tools/native-rexglue/submission_fence_wait.h) используется
непосредственно SDK и обоими oracles;
[patch](tools/native-rexglue/patches/latest-submission-fence.patch) воспроизводит
точный source поверх сохранённого idle-reclaim checkpoint.
Нулевые timeout polls и retirement того же завершённого prefix сохранены.
Host [ASan/UBSan regression](tools/native-rexglue/submission_fence_wait_test.cpp)
покрывает success,timeout,host errors,device loss,invalid/empty prefix,порядок
free-list и успешный/неуспешный EndSubmission. Shared preparation guard останавливает
retirement при DEVICE_LOST из EndSubmission без повторного уведомления.

[Реальный GPU oracle](tools/native-rexglue/vulkan_submission_fence_wait_test.cpp)
проверил одну очередь,32submission fences,requested prefixes1/2/8/32,AB/BA,
точные mapped bytes,статусы каждого завершённого fence,reset/reuse и free-list.
На prefix32 в6измеренных rounds:192→6переданных handles,по одному на каждый wait.
Это уменьшение длины аргумента API,не числа32GPU submissions и не устранение
необходимого ожидания GPU. Prefix1 и8 дали неоднородные времена: например,
trial1,prefix1 —0.501→171.689µs,prefix8 —38.864→57.750µs; latency speedup
не подтверждён. Отдельно prefix32 был443.239→421.657µs и390.247→381.050µs,
но эти два результата нельзя переносить на gameplay или считать изолированной
ценой обработки массива. Validation layer не включён; фон desktop GPU не изолирован.

Flag кэшируется один раз. С существующим ACV_GPU_STAGE_PROFILE только после90s
записываются5sокна: blocking calls,исходная длина prefix,passed handles,количество
prefix>1,max prefix,polls,blocking ms. Без profiling нет clock reads/агрегации/logging
новой диагностики. Если реальные blocking prefixes всегда1,сокращать массив нечего.

[Manifest кандидата](tmp/acv-latest-fence-20261030/build.json),
[полный результат и operations](logs/readback-latest-fence-20261030/result.json).
GPU SHA-256:5bf5a83faec978ec84bbcf8a84b463363e39a08b6ca059dec8f3f27826330b64.
Он source/built/host-tested/GPU-oracle-tested; gameplay не запускался.
SDK include-order defect из первой попытки сборки исправлен использованием
helper после штатного SDK Vulkan API include,без замены Vulkan headers/PCH/options.
Сборка изолированная: один TU и relink,без Ninja regeneration/FFX/Wine.

Runtime нового кандидата — текущий release
8e2bae6150ad5b334c0b617b513205ba84bf18fa99e554e285cb4d5809f45353.
Он был изменён параллельной работой вне этого FPS-этапа; прежние6090/d999 сохраняют
544bdaae0e54c52fc10a60cacdf4a4973841a2ba02e2c9579a54f062fc6ad084.
При финальном loader check текущий release GPU уже27de8fab (полный SHA в result.json),
тоже изменён параллельной работой. C-кандидат собран от сохранённого readback checkpoint,
а не доказан как текущий27de8fab плюс единственный патч. Перед переносом в релиз
потребуются сверка с его актуальным source и регрессия. Ничья библиотека не
восстанавливалась/перезаписывалась. Сравнивать latest0/1 на одном новом кандидате,
не разные runtime или GPU binaries между собой. RTLD_NOW и реальные paths/hashes
трёх загруженных candidate libraries проверены с тем LD_LIBRARY_PATH,который
устанавливает packaged probe; это не проверка gameplay/полной ABI совместимости.

## 3. Объединение нескольких readback submissions

Обновление2026-10-31: [реализовано и проверено](kb:FPS_RESOLVE_BATCHING_20261031);
ниже сохранено обоснование до реализации.

IssueCopy_GeneralReadback заканчивает submission после каждого Resolve:
в выбранном окне≈690/s. EndSubmission выполняет deferred command encoding,
flush uniform writes,работу cache managers и vkQueueSubmit. Несколько Resolve
в одном submission могут уменьшить эти расходы и сбросы cached state.

Механизм flush при ожидании текущего ticket уже есть в
CheckSubmissionFenceAndDeviceLoss, но
[GeneralReadbackBridge::Commit](tools/native-rexglue/general_readback_bridge.h)
и [DeferredReadbackQueue](tools/native-rexglue/deferred_readback_queue.h)
разрешают только успешно submitted copies. Простое удаление EndSubmission
нарушит этот контракт. Нужны состояния recorded/submitted/failed,сохранение leases
до submit/cancel,принудительный flush на CPU access/slot reuse/drain и ограничение
размера batch. Failure не должен выдавать старые bytes за успешную публикацию.

Сам Resolve уже вызывает compute dispatch и SubmitBarriers(true),который
завершает render pass. Поэтому batching без отдельной переделки Resolve
не обещает уменьшить число render-pass ends. Это source-confirmed направление,
последующая реализация описана в обновлении выше. Приёмка: меньше vkQueueSubmit при одинаковых данных,
точные поколения/CPU partial writes,ошибки submit/cancel и bounded memory;
затем matched gameplay frametime/1%low.

## Граница следующего игрового сравнения

Для latest-fence подготовлен [start.sh](tmp/acv-latest-fence-20261030/start.sh)
с обязательным аргументом0или1. Это локальный диагностический запуск максимум240s,
не установка в release. Сначала0(контроль),затем1(кандидат),одинаковый участок.
Скрипт использует существующий ProfileStore backup/verify под writer lock,
потом packaged probe повторно захватывает и удерживает lock игры. Между этими
двумя захватами есть промежуток; конкурирующий writer вызывает безопасный отказ
probe,атомарная передача lock не реализована. При подготовке скрипта профиль
не записывался. Canonical config обязателен; restore/reset и временный config
не используются. Syntax/metadata checks не равны реальному runtime-тесту.

Проверять каждый opt-in отдельно на одном кандидате с флагом0/1; Forward128=0,
general deferred=1,parallel-copy=2,GPU poll backoff=0. Сохранить2560×1080,
обычный профильный backend,writer lock,резервную копию и canonical acv.config.
Первую миссию/камеру повторять после120s,полное окно120–180s без меню/паузы.
Сопоставить raw frametime/1%low,pressure reason и фактические waits/submits;
проверить материалы,объекты и темп логики. CAS/FSR и прочие разрешения не тестировать.

Имеющийся158draw RDC frame18547 — Setup/emblem,не первая миссия.
Presentation-only GPU replay0.641ms другого FBO capture не является временем
рендера gameplay. FSI shader оптимизации требуют репрезентативного GPU capture/
timestamps; FBO/full уже отклонён по чёрному gameplay. Readback/coherence,vblank
и timestep не отключать ради показателя swap cadence.

[Предыдущий Forward128 результат](kb:FPS_FORWARD_COPY_20261029),
[основной отчёт](report-fps.md),
[сохранённые документы до обновления](logs/fps-alternatives-20261030/docs-before/sha256.json),
[снимок после возобновления](logs/fps-alternatives-20261030/docs-before-resume/sha256.json).
