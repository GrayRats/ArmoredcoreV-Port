# Семь Vulkan доработок — 2026-11-04

Все семь направлений реализованы в host Vulkan backend ReXGlue/Xenos и включены
в отдельный пакет [ACV-Linux-release-2.1](../ACV-Linux-release-2.1/start.sh).
Предыдущий all-seven build проходил описанные ниже host/GPU/runtime проверки;
окончательная сборка v2.1 повторно скомпилирована и package-verified, но сама
не запускалась. FPS-прирост не доказан. Release2.0 сохранена. Generated PPC,
XenonRecomp и оригинальные данные не менялись.

Дополнение подключило тот же persistent `VkPipelineCache` к двум lazy
render-target ownership-transfer pipeline calls и добавило уведомление
фонового writer. Оно собрано в v2.1; для него ещё не выполнялись отдельные GPU,
image или gameplay проверки. См. [отчёт](kb:FPS_VULKAN_RENDER_TARGET_CACHE_20261104).

| Путь | Опция | Реализованная граница |
|---|---|---|
| Driver VkPipelineCache | `ACV_VULKAN_PIPELINE_CACHE=/absolute/file` | 64MiB limit, UUID/vendor/device/checksum, flock, background snapshots, atomic rename, corrupt fallback |
| Synchronization2 | `ACV_VULKAN_SYNCHRONIZATION2=1` | Core1.3 feature/PFN; per-resource buffer/image stages, overlap splits и legacy fallback |
| Timeline submissions | `ACV_VULKAN_TIMELINE_SUBMISSIONS=1` | Core1.2 feature; monotonic tickets, counter/wait retirement, maxTimelineSemaphoreValueDifference; sparse/WSI binary waits сохранены |
| Graphics pipeline libraries | `ACV_VULKAN_PIPELINE_LIBRARIES=1` | KHR+EXT и GPL feature; vertex-input interface reuse, максимум128 libraries, остальные части передаются final link |
| Extended dynamic state | `ACV_VULKAN_EXTENDED_RASTER_STATE=1` | Core1.3; cull/front-face исключены из PSO key и задаются перед guest draw; topology/FSI shaders прежние |
| Staging suballocation | `ACV_READBACK_SUBALLOCATION=1` | Core1.1;64MiB mapped buffer/allocation, до64 atom-aligned диапазонов; actual flags и offset/size invalidate |
| Copy queue | `ACV_READBACK_TRANSFER_QUEUE=1` | Core1.2/timeline и2 queues в graphics family; producer→copy timeline→publication; следующие GPU writes ждут copy |

Для transfer используется отдельный producer submit на каждый Resolve, поэтому
batching при активной второй очереди отключён. Read-only graphics может перекрываться
с копированием. Вариант с отдельной queue family и ownership transfers не реализован.
Две очереди не доказывают наличие независимого DMA engine и ускорение: batching
может оказаться быстрее. Ошибка transfer submit запрещает дальнейшие submissions
этого контекста; ресурсы удерживаются до stop/device loss. Неподдерживаемые функции
используют прежние пути. ABI изменён: старые PCH и allocating objects использовать нельзя.

## Кандидат

[Каталог](tmp/acv-vulkan-all7-20261104),
[полные хеши](tmp/acv-vulkan-all7-20261104/build.json),
[compile/link commands](tmp/acv-vulkan-all7-20261104/commands.json).
Native runtime/GPU пересобраны согласованно:243 TU с текущими textual prefix headers,
без старых PCH/objects. Неизменённые guest ELF, thirdparty archives и FidelityFX
переиспользованы. Wine/FFX shader generation не запускались. Финальные pipeline
partial-handle cleanup и fail-closed transfer submission собраны инкрементально.

Ограниченный135s тест со всеми семью опциями:

```bash
./tmp/acv-vulkan-all7-20261104/start.sh
```

[Launch script](tmp/acv-vulkan-all7-20261104/launch.py) проверяет artifact hashes,
использует canonical acv.config, существующие settings/UUID/XUID и проверенный
profile backup с writer lock. Config/settings не переписываются. Через135s окно
закрывается автоматически. Основные index/push/dynamic/stencil фильтры включены;
новый descriptor-bind dedup остаётся отдельной опцией.

## Проверено

- Native243TU build и обе `.so` PASS; compile diagnostics сохранены рядом с objects.
- [Host regression](tools/native-rexglue/vulkan_modern_features_host_test.cpp),
  ASan/UBSan: missing PFN, monotonic/range admission, failed-submit ticket retention,
  no premature publication,64 pending transfer slots/reuse/device loss,
  запрет повторного submit после ошибки, GPL key/reuse/failure/fallback PASS.
- [GPU oracle](tools/native-rexglue/vulkan_modern_features_test.cpp), ASan/UBSan:
  GTX1660SUPER, raw driverVersion2580775424,2 queues, GPL доступен.80 generations
  exact bytes, staging neighbor guard/reuse PASS; legacy/sync2 barriers чередуются.
  Static/dynamic/GPL offscreen16×16 pixels одинаковы. Driver cache reload и corrupt
  fallback PASS. PNG/screenshots не создавались.
- [Deferred decoder](tools/native-rexglue/vulkan_modern_deferred_test.cpp) с
  [узким device fixture](tools/native-rexglue/vulkan_modern_test_fixture/rex/graphics/vulkan/command_processor.h):
  production decoder без изменения; legacy flatten, sync2 scopes/layouts/deep copy,
  raster PFNs и reset PASS. Fixture не доказывает все CP overlap split combinations.
- Existing batching, idle reclaim, coherent publication и fence-prefix regressions
  повторно собраны с ASan/UBSan и прошли.
- Первый actual runtime135s: child0, runtime_errors=[], fatal0. Лог подтверждает
  timeline counter, sync2 feature, queue1,64MiB pool; cache loaded=false.
- Final warm-cache runtime135s: child0, runtime_errors=[], fatal0; loaded=true.
  Сцена и старые dedup flags отличаются от cold run: это не FPS A/B сравнение.
  Оба собственных игровых процесса завершены, config/settings SHA не изменились.

Validation layer локально отсутствует (`validation=0`); это не clean validation run.
Не приняты mission image, save/load, game-time, другие драйверы, actual noncoherent
GPU, длительная работа и причинный FPS/stutter прирост. Следующее сравнение:
одинаковый маршрут/сцена, transfer против batching, память и frametime p95/p99.

## Evidence и воспроизведение

[Итоговый SDK patch](tools/native-rexglue/patches/vulkan-modern-seven.patch),
[до/после SHA256 и replay](logs/vulkan-all7-20261104/patch-manifest.json).
Aggregate заменяет ранние draft patches persistent-driver/scoped-sync2/timeline/
feature-gates этого этапа: не применять их поверх него. Exact replay14SDKfiles PASS.
[Implementation ledger](logs/vulkan-all7-20261104/implementation-state.json),
[checkpoint](logs/vulkan-all7-20261104/checkpoint.json),
[предыдущие docs с SHA](logs/vulkan-all7-20261104/docs-before/sha256.json).
Команды выполнялись через SQLite logger; подробные результаты остаются в SQLite/logs.

Pinned Khronos Vulkan-Docs `090f1b190d60ced4a1d198fd3747d071cc271b1c`:
`chapters/pipelines.adoc`, `chapters/synchronization.adoc`, `xml/vk.xml`,
`appendices/VK_EXT_extended_dynamic_state.adoc`; references не обновлялись.
Pipeline cache flags0 даёт внутреннюю синхронизацию concurrent create/get-data;
destroy следует join workers. Сборка/host/GPU tests не заменяют игровую приёмку.
