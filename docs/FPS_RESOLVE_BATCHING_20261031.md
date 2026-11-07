# Resolve submission batching — 2026-10-31

## Runtime batching0/1 — 2026-10-31

Два240s запуска одного464ec0f1 завершены child0/fatal0/runtime_errors0.
Проверены одинаковые artifact/config/settings/probe hashes и UUID/XUID;
отличается только batching0/1. Backup+verify выполнены под writer lock перед
каждым запуском. Итоговое окно2560×1080,FIFO,format64,лимит120 сохранено.
Сцену/маршрут/камеру пользователь не подтвердил: это наблюдаемое сравнение,
не доказанный одинаковый игровой вход и не приёмка картинки/темпа логики.

|120–180s|Batch0|Batch1|
|---|---:|---:|
|Raw swap/s|14.5733|17.5848|
|Mean interval,ms|68.6186|56.8672|
|1%low|12.2852|15.5829|
|p99,ms|79.0352|62.7012|
|Max,ms|84.3551|66.6245|
|Device-wide GPU,%|96.00|95.45|
|User-IP samples/lost|4502/0|4199/0|

Разница swap rate +20.7% в этих окнах,не гарантированный прирост FPS.
Batch counters120.025–180.076s:38016jobs/11536readback submissions,
3.295jobs/submission,max4;capacity flush2112,limit7392,CPU/slot/drain0.
Stage EndSubmission calls/Resolve:34061/31497 против12655/38001;
это calls,не отдельный счётчик успешных vkQueueSubmit. Inclusive pool-wait wall
19.08%→26.99%; проценты вложены,их нельзя складывать. GPU по-прежнему занят≈96%.

[Полное сравнение](logs/resolve-batching-20261031/runtime-comparison.json)
ссылается на завершённые session/log snapshots,сырые traces и отдельные
swap/stage summaries. Full120–180s CPU/GPU/IP сборы завершены без потерь.
DLC missing-file warnings и shutdown warning контрольного запуска сохранены;
отсутствие runtime errors не равно отсутствию предупреждений или полной playability.
Оба собственных запуска завершены. Default/release не менялись.
Далее нужен повтор с подтверждённым одинаковым участком и render/game-time acceptance.

По явному запросу пользователя Astra6 реализовала отдельный opt-in кандидат.
Включение: `ACV_GENERAL_READBACK_BATCH=1`. По умолчанию batching выключен. FPS-изменение не устанавливалось в релиз;
на этапе сборки gameplay не запускался; последующие runtime результаты приведены выше.

## Изменение и корректность

Лимит пачки4Resolve/16MiB; большие одиночные копии отправляются сразу.
Общие pool/queue лимиты64slots/jobs и64MiB сохранены. Bridge различает
Preparing,Recorded,Submitted,Published,Failed. Записанные задания находятся
в отдельном списке; после успешного vkQueueSubmit и регистрации fence его узлы
переносятся в submitted queue без выделения памяти,ожиданий или callbacks.
Admission учитывает combined recorded+submitted count/bytes.

CPU-demand,slot reuse,capacity,drain и достижение лимита отправляют пачку.
Существующие primary-buffer-end,Draw/Swap и teardown границы сохранены.
Общий submission scope обрабатывает все ранние отказы EndSubmission;
успех GPU submit с неудачным commit возвращает ошибку явно. Failed callbacks/
staging сохраняются до подтверждённой остановки GPU. Preparing providers
не объявляются submitted при закрытии предыдущих команд.

HOST_READ barrier,invalidation,shared-memory usage barriers и порядок поколений
не удалены. GPU submission order сам по себе не заменяет memory dependency:
pinned Vulkan-Docs090f1b19,synchronization.adoc1906–1948,6608–6698.

Проверка actual SDK исправила предварительную гипотезу: RegisterLocked в
include/rex/system/physical_readback_providers.h не ожидает старые GPU providers.
Он ожидает CPU ownership. Принудительная синхронизация перекрывающихся GPU writes
не добавлена. UploadRanges→SynchronizePhysicalCpu действительно может закрыть
submission; последующий BeginSubmission перед descriptor/ticket capture сохранён.

## Проверки и результат

Host ASan/UBSan: CPU owner-inline/other-thread demand,partial overlap/CPU writes,
Preparing exclusion,combined caps,slot/drain/external close,submit/commit failure,
wrong ticket,recursive flush,lease retention и oversized single PASS.
Прежняя bridge regression PASS. GPU oracle использует общий production bridge
и submission scope; одинаковые40Resolve в порядке AB/BA:

| Метрика | Обычный путь | Batching |
|---|---:|---:|
| Реальные vkQueueSubmit |40|14|
| Опубликованные результаты |40|40|
| Peak allocated bytes |40,325,120|40,325,120|
| Время пары AB,ms |51.7832|49.3959|
| Время пары BA,ms |51.5207|46.7679|

Exact bytes/guards,поколения и CPU write сохранены;max pending11,CPU waits4.
Это synthetic transfer/publication workload,не полный Resolve/render replay.
Он не моделирует частоту production UploadRanges/primary-end flushes. Время
не переносится на FPS. Gameplay/render/game-time acceptance остаются открытыми.

## Воспроизведение и границы

[SDK patch](tools/native-rexglue/patches/resolve-submission-batch.patch),
[bridge](tools/native-rexglue/general_readback_bridge.h),
[queue](tools/native-rexglue/deferred_readback_queue.h),
[submission scope](tools/native-rexglue/readback_submission_scope.h),
[host regression](tools/native-rexglue/readback_submission_batch_test.cpp).
[Manifest](tmp/acv-resolve-batch-20261031/build.json),
[полное evidence](logs/resolve-batching-20261031/result.json).
Изолированная сборка одного TU+relink; без Ninja regeneration/FFX/Wine.
RTLD_NOW,actual loaded library paths/hashes,patch replay и сохранность прежних
кандидатов проверены. Это load/oracle acceptance,не gameplay acceptance.

Подготовлен [start.sh](tmp/acv-resolve-batch-20261031/start.sh) с аргументом0/1:
один кандидат,профильный backup/verify под writer lock,canonical config,
120–180s raw swaps. Подготовка/check-only не запускает игру и не пишет backup.
Forward128,reclaim,latest-fence выключены,general1/parallel2/backoff0.
Проверять первую миссию2560×1080 на одинаковом участке; CAS/FSR не тестировать.
Conditional stage profiling после90s показывает jobs/submissions/max batch и
причины flush. Реальная частота пачек теперь измерена выше; причинный FPS-выигрыш требует matched сцены.

[Основной отчёт](report-fps.md),
[снимок документов до изменений](logs/resolve-batching-20261031/docs-before/sha256.json).
