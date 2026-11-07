# Vulkan API: immutable Resolve descriptors — 2026-11-01

Astra6 реализовала отдельный opt-in `ACV_RESOLVE_DESCRIPTOR_CACHE=1`.
Default off; release не заменён. Кэш64точных buffer/offset/range bindings только
для unscaled shared-memory buffer. После записи descriptor set не обновляется
и не вытесняется,пока его может использовать GPU. При заполнении/нехватке памяти
используется прежний transient path;device loss возвращается как ошибка.

SDK уже переиспользует transient handles. Убраны повторные descriptor updates
и per-Resolve bookkeeping на cache hit,а не якобы существовавшая vkAllocate
на каждом Resolve. Offset/range и shader dest_base subtraction сохранены.
Валидация учитывает buffer bounds,maxStorageBufferRange и offset alignment.
Pool принадлежит GeneralReadbackContext;destroy после proven queue stop до
shared-buffer/layout teardown. ClearCache не пересоздаёт этот shared buffer.
Новая жизнь буфера получает новый cache,даже если Vulkan повторил handle.

Проверки ASan/UBSan покрывают ключи,границы,выравнивание,64entry fallback,
ошибки Vulkan и запрет преждевременного destroy. Реальный compute GPU oracle
AB/BA:128dispatches/4submissions/4keys,включая ненулевые смещения и разные ranges.
Снимок после каждого dispatch проверяет exact bytes и prefix/middle/tail guards.
`vkUpdateDescriptorSets`:128→4. In-flight immutable reuse PASS.
Fixture allocations128→4 не равны allocation-rate реального SDK.
Время пар4.33237→3.95822ms и2.92638→3.72505ms: стабильного ускорения нет,
FPS-выигрыш не заявляется. Validation layer не включён;это binding oracle,
не полный ACV Resolve shader/gameplay replay. Реальный hit-rate ещё не измерен.

[Helper](tools/native-rexglue/resolve_descriptor_cache.h),
[SDK patch](tools/native-rexglue/patches/resolve-descriptor-cache.patch),
[manifest](tmp/acv-resolve-descriptor-cache-20261101/build.json),
[evidence](logs/resolve-descriptor-cache-20261031/result.json).
Изолированная сборка CP+RTcache,без full Ninja/FFX/Wine.
[Start0/1](tmp/acv-resolve-descriptor-cache-20261101/start.sh) предназначен для
одного кандидата с batching1 фиксированным;forward/reclaim/latest0.
Backup/profile writer lock/canonical config используются штатные;
подготовка/check-only не запускает игру и не создаёт backup.

Уже внесённая Vulkan API доработка: batching сокращает vkQueueSubmit;в двух
наблюдавшихся runtime14.57→17.58swap/s,но маршрут не подтверждён.
[Отдельный результат](kb:FPS_RESOLVE_BATCHING_20261031).

Следующие source-направления (не включены в этот кандидат): пропуск invalidate
ТОЛЬКО для фактически HOST_COHERENT allocation с сохранением fence/barrier;
прямой GetFenceStatus/поиск завершённого prefix вместо нулевых WaitForFences.
Pinned Vulkan-Docs memory.adoc934–940 и descriptorsets.adoc2974–3008;
неизменяемость descriptor сохраняет executable/pending command buffers.
После batching уже наблюдалось уменьшение polls66271→32597 за≈60s;
польза дополнительного polling-изменения требует отдельного измерения.
[Source findings](logs/vulkan-api-opt-20261031/source-findings.json),
[документы до изменений](logs/vulkan-api-opt-20261031/docs-before/sha256.json).
