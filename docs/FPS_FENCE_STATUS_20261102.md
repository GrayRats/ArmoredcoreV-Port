# Vulkan API: fence status polling — 2026-11-02

Astra6 реализовала opt-in `ACV_VULKAN_FENCE_STATUS_POLL=1`,default off.
Только нулевые vkWaitForFences заменяются на vkGetFenceStatus.
Блокирующие ожидания,число/порядок опросов,завершённый prefix/free-list,
EndSubmission device-loss guard и HOST_READ barriers сохранены.
VK_NOT_READY хранится как native результат и не считается ошибкой.
Флаг кэшируется один раз;profile-only counters после90s различают API и
SUCCESS/NOT_READY/TIMEOUT/DEVICE_LOST/прочие результаты.

Общий production helper имеет отдельный poll callback и legacy overload.
ASan/UBSan:старые проверки и192новых сценария PASS,включая частичный успех,
ошибки ожидания/опроса и потерю устройства без раннего retirement.
GPU AB/BA:каждый вариант264опроса (256SUCCESS,8TIMEOUT либо8NOT_READY),
точные2MiB outputs/guards и8reset/reuse rounds PASS. Число опросов не сокращено.
Микротест готовых/неготовых fences:43.513→13.842µs и44.127→16.552µs суммарно.
Это≈0.1µs экономии на вызов в этом fixture,не FPS-ускорение игры. Даже при
условных500poll/s эффект такого масштаба — десяткиµs/сек. No FPS claim.
Validation layer не включён;реальная device-loss ветка host-injected.

Pinned Vulkan-Docs090f1b19 synchronization.adoc2691–2728,2858–2882 подтверждает
execution dependency через WaitForFences или pollingGetFenceStatus.
Сам fence не заменяет memory barrier для host visibility.

[Shared helper](tools/native-rexglue/submission_fence_wait.h),
[SDK patch](tools/native-rexglue/patches/fence-status-poll.patch),
[manifest](tmp/acv-fence-status-poll-20261102/build.json),
[evidence](logs/fence-status-poll-20261102/result.json).
Изолирован CP TU+relink,сохранён RTcache object;855входов захешированы.
Patch replay,RTLD_NOW actualdependency paths/hashes и сохранность прежних
кандидатов/release проверены. Игра,backup и profile/config writes не запускались.
[Start0/1](tmp/acv-fence-status-poll-20261102/start.sh) держит batching1,
descriptorcache1,coherent1 одинаковыми;forward/reclaim/latest0.

Следующий приоритет — opt-in GPU timestamps для FSI/Resolve/copy с отложенным
чтением query results после completion. Сначала установить реальную стоимость
GPU-работы;снижать качество картинки или отключать coherence не требуется.
Предыдущее тяжёлое окно имело GPU91.85%;малые host API правки не дают основания
обещать >30FPS. [Runtime scope](kb:FPS_COHERENT_READBACK_20261101).
[Документы до изменений](logs/fence-status-opt-20261102/root-docs-before/sha256.json).
