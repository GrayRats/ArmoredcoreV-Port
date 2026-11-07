# Exact descriptor bind dedup — 2026-11-04

Реализован opt-in фильтр vkCmdBindDescriptorSets в общем deferred decoder.
Ключ содержит pipeline bind point, точный layout handle, firstSet/count,
все descriptor set handles и все dynamic offsets. Кэш хранит только последний
bind и живёт один Execute; любой отличный bind заменяет ключ. Это консервативно
учитывает disturbance от перекрывающихся sets и несовместимых layouts.

Предел16sets/32dynamic offsets. Больший запрос сбрасывает кэш и вызывает обычный
Vulkan API. Не добавлены Vk handles/resources, barriers, extensions или waits.
В существующем decoder нет push-descriptor/secondary-command opcodes; будущие
такие команды должны явно инвалидировать этот кэш. Pipeline binds сами по себе
не заменяют descriptor bindings. Две схемы состояния не объединяются по одному
совпадению set handle.

[Helper](tools/native-rexglue/descriptor_bind_cache.h),
[host regression](tools/native-rexglue/descriptor_bind_cache_test.cpp),
[SDK patch](tools/native-rexglue/patches/descriptor-bind-dedup.patch).
ASan/UBSan:10000точных повторов,изменение offsets/layout/point/range/sets,
перекрытие/fallback/reset PASS. Exact patch replay,3TU build,RTLD_NOW и check-only
запуска PASS. Реальный compute oracle подготовлен и скомпилирован, но не выполнен:
preflight обнаружил активную игру PID9052. Чужая игра не останавливалась.

Кандидат `tmp/acv-descriptor-bind-20261104`, флаг
`ACV_VULKAN_DEDUP_DESCRIPTOR_BINDS=1`,default off. В существующие post90s counters
добавлены descriptor_in/out. GPU/render correctness, число сокращённых вызовов
в игре и FPS пока не измерены. Итоговый release2.0 не заменён.
[Build/result](logs/descriptor-bind-dedup-20261104/result.json).

Следующий check: после завершения пользовательской игры запустить
`tmp/descriptor-bind-gpu tmp/resolve_descriptor_oracle.spv` через logger;
проверить snapshots/guards при повторных bindings. Затем bounded135s кандидат,
фактические counters и правильность изображения; лишь после этого решать
вопрос интеграции. Синтетический oracle не доказывает выгоду текущей сцены.

Khronos Vulkan-Docs pinned090f1b190d60ced4a1d198fd3747d071cc271b1c,
descriptorsets.adoc4595–4640: bind point, layout, set range, dynamic offsets,
persistence/disturbance. Применены оба локальных Vulkan skill.
