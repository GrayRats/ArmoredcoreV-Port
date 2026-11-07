# Vulkan API: coherent readback — 2026-11-01

## Runtime и пользовательская оценка — 2026-11-02

Оба240s запуска завершены child0/fatal0/runtime_errors0.120–180s сборы полные,
IP3494/4512,lost0. Artifact/config/settings/profile совпадают;меняется только
coherent0/1,batching1 и descriptorcache1 одинаковы. Backup/verify перед обоими
запусками выполнены штатно. Runtime предупреждения сохранены в completed logs.

|Метрика|Coherent0|Coherent1|
|---|---:|---:|
|Raw intervals|1800|1163|
|Swap/s|29.9983|19.3725|
|1%low|28.1856|15.4114|
|GPU device-wide,%|29.90|91.85|
|Resolve calls|1800|41837|
|Draw calls|106196|3881056|

**Разная нагрузка:**контроль был лёгким,max batch1;второе окно выполняло
значительно больше Resolve/draw. Это не причинный FPS A/B и не регрессия
coherent-skip. Отдельного runtime счётчика invalidate нет;32→0 относится
к GPU oracle,не к измеренному счётчику этих игровых запусков.

Пользователь после второго окна:«Да, лучше стало». Принята субъективная
плавность текущей комбинации,не доказан отдельный вклад invalidate и не
подтверждена полная image/game-time/save-load parity. Release не заменялся.
[Сравнение](logs/readback-coherent-invalidate-20261101/runtime-comparison.json),
[оценка пользователя](logs/readback-coherent-invalidate-20261101/user-acceptance.json),
[исходные документы](logs/readback-coherent-invalidate-20261101/runtime-docs-before/sha256.json).

Astra6 реализовала отдельный opt-in пропуск избыточного invalidate в GeneralReadback.
Слот хранит actual memory type flags+known для обычного allocation и reclaim path.
Shared helper пропускает vkInvalidateMappedMemoryRanges только при включённой
опции,подтверждённых HOST_VISIBLE|HOST_COHERENT и завершённом submission.
Неизвестные/noncoherent flags вызывают прежний invalidate;VkResult/исключение
проверяются до publication. Fence,device→HOST_READ barrier,CPU memory lock,
copy-worker join и порядок поколений сохранены. Fast/full/memexport не изменены.

Основание: pinned Vulkan-Docs memory.adoc934–940,HOST_COHERENT не требует host
cache maintenance. Это не разрешение читать до GPU completion или убрать barrier.
Root review проверил actual submission_completed_>=slot->submission guard,
оба allocation paths и общий production/test publication helper.

ASan/UBSan PASS. Реальный GPU AB/BA:type4flags14 (VISIBLE|COHERENT|CACHED),
32поколения и guards совпали;invalidate32→0. Noncoherent type недоступен на этой
GPU:ветка проверена host-injected,не реальным noncoherentGPUallocation.
Время85.102→77.916ms и78.0224→81.3601ms неоднородно;speed/FPS gain не заявлен.
Это публикация синтетических transfers,не игровой Resolve replay.

[Helper](tools/native-rexglue/coherent_readback_publication.h),
[patch](tools/native-rexglue/patches/coherent-readback-invalidate.patch),
[manifest](tmp/acv-coherent-readback-20261101/build.json),
[evidence](logs/readback-coherent-invalidate-20261101/result.json).
GPUa4c7f2400008942b2d330850ad47ea3272f62dce776723a5226352cde067a988.
Изолирован CP TU+relink с сохранённым725RTcache object,без fullNinja/FFX/Wine.
На этапе сборки defaultoff,игра не запускалась;последующие runtime результаты выше.
Релиз/конфиг/сейвы не заменялись.

[Start0/1](tmp/acv-coherent-readback-20261101/start.sh) держит batching1 и
Resolve descriptor cache1 одинаковыми;другие эксперименты0. Штатный профильный
backup/lock/canonical config используются при настоящем запуске;check-only
не создаёт backup и не запускает игру. Реальная FPS/картинка/темп приёмка открыта.
[Предыдущая API-доработка](kb:FPS_VULKAN_API_20261101),
[архив документов](logs/coherent-readback-20261101/root-docs-before/sha256.json).
