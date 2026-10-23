# ACV — короткий контекст продолжения

Обновлено:2026-10-23. Очередь — [PLAN](PLAN.md), технические факты —
[NATIVE](NATIVE_REXGLUE_LINUX.md), этапы — [PROGRESS](PROGRESS.md).

- **Цель:** корректный native Linux gameplay. Пользователь разрешил исправления,
  сборки и bounded tests. Windows/Wine/Unity вне текущей задачи.
- **Правила:** [AGENTS](../AGENTS.md). ISO/assets/live Ghidra DB read-only;
  generated translation не первичный fix. SQLite logger для анализов/сборок/тестов,
  сохранять PCH/objects. Без screenshots/PNG; RDC/replay/raw bytes допустимы.
- **Решено:** потеря динамической текстуры из-за выключенного Resolve readback.
  Профиль acv-fsi в probe_linux.py теперь добавляет --readback_resolve=full.
  SDK config и --extra могут переопределять; sdk profile не меняется.
  ARTEM/ARTEM/headless=true config сохранён побайтно, SHA31ef1d2a….
- **Доказательство:** исходный RGBA resource0x4350A980, pixels0xFE191000,
  physical0x1E192000,1024x512,2MiB. None →2MiB нулей; full →2066418 ненулевых
  bytes при одинаковых resource/native headers. Компрессор и конвертер получают
  содержимое вместо пустого изображения.
  [A/B](../logs/acv-readback-origin-20261022/ab-chain.json).
- **GPU:** run20261022T172345196638Z/frame34282/event75,4158indices, declared
  slot0,BC3 physical0x1F6B3000. GPU512KiB точно равны CPU upload submission25699,
  SHA00c279ff…; texture center RGBA[0.223529,0.215686,0.223529,0.14902], раньше0.
  [Proof](../logs/acv-readback-origin-20261022/gpu-material-proof.json).
  Первый converter snapshot и поздний upload относятся к разным обновлениям:
  не требовать их равенства. Точная визуальная корректность всей сцены не принята.
- **Текущий пакет:** tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv_linux_probe,
  ELF SHA fbf7f9ce…, normal GPU55937953…, runtime d8fb56af….
  Resolve slot completion/reuse и invalidate-result guards интегрированы;
  staging округляется к64KiB. Full остаётся default. Последний120s run
  20261022T182231377426Z: child0,45 прежних diagnostics; global scene не принята.
- **Проверка интеграции:** [same-run CPU/GPU bytes](../logs/acv-integrated-readback-20261022/gpu-byte-proof.json),
  [normal GPU](../logs/acv-integrated-readback-20261022/release-gpu-proof.json).
  RGBA/BC3 SHA совпадают с accepted full; ASan/UBSan host publication tests прошли.
- **VFS/память:** ACV_TRACE_RESOURCE_IO включает bounded open/read/close и
  alloc/free-request trace.177 reads/≈32MiB совпали с source slices;16 source DCX
  декодированы с прежними SHA. Guest decompressor/queue ownership не проверены.
  [Technical details](NATIVE_REXGLUE_LINUX.md#integrated-resolve-completion-and-resource-tracing-2026-10-23).
- **Запуск:** tools/native-rexglue/test_linux.sh; canonical config сохранён.
  Сборка tools/native-rexglue/build_linux.py --jobs 4
  --gpu-plugin-artifact tmp/acv-gpu-integrated/librexgpu-xenos.so.
  Без override relink может вернуть installed plugin. Диагностика opt-in.
- **Осталось:** сопоставимый gameplay/scene regression и FPS full readback;
  VFS system:/dlc_* и две physical allocations0x40000/0x800000 protect0x20000400.
  Allocation override остаётся opt-in: прежние A/B сцены не совпали.
- **RE цепочка:** исходник → compressor0x82C82700/0x82C82090/0x82C82130 →
  physical0x1E112000 → converter0x82C83DE8 → physical0x1F6B3000. Raw PPC в
  logs/acv-texture-writer-20261022; source origin/A-B в logs/acv-readback-origin-20261022.
- **Ghidra:** scripts/ghidra-rpc.sh status перед использованием; headless
  ACV_RETRY.gpr /default.xex, saved --no-analyze. Listing неполон: read-bytes +
  Capstone fallback. DB не чинить. XEX SHA4fac8734…, image0x82000000,
  text0x822B0000,entry0x82B91F88, ReXGlue0.10.0.
- **Сохранение:** [проверенный прежний checkpoint](../logs/acv-readback-checkpoint-20261022T171359Z/verification.json),
  [состояние перед принятием fix](../logs/acv-readback-origin-20261022/docs-before-accepted-fix.sha256.json).

Upstream sync reference header — только пример; в runtime не подключён.
Рабочее исправление использует существующий full readback SDK, а не новый
barrier/fence workaround. Полные ограничения — в technical section NATIVE.

## Продолжение интеграции — 2026-10-23

Slot/fence/invalidate hardening и VFS/allocation tracing уже подключены и проверены.
Resolve/CPU consumer связан: physical0x0FCA7000,submission1298/frame627 →
first copy0x82C83918,2MiB побайтно равны SHA6af9819e…; затем tiled→linear→
RGBA0x1E192000→compressor. [Proof](../logs/acv-resolve-consumer-20261023/complete-proof.json).
Следующий пункт PLAN — отложенная публикация до guest fence/CPU first-read.
EVENT_WRITE_SHD сейчас сразу пишет guest completion, это надо учитывать. Generic async reference пока не заменяет
full в игре. Для немедленного CPU consumer необходимо актуальное поколение;
готовность прошлого кадра не подходит. FPS и p95/p99 ещё не измерены.

Текущая host сборка включает только opt-in bake-chain diagnostics; GPU в основном
пакете остаётся normal55937953…. Trace plugin отдельно tmp/acv-bake-chain-test;
его не переносить в normal package. Raw PPC/трассы в logs/acv-resolve-consumer-20261023.
Ghidra restarted headless, saved default.xex opened --no-analyze; DB не менялась.

Исходники инструментов/скрипты/MD сохранены в приватном
[DEArmoredcoreV](https://github.com/GrayRats/DEArmoredcoreV), initial451b6c4.
Данные игры, SDK, logs/tmp/cache и персональный config в GitHub не отправлены.
Проверка payload: scripts/check_repository_payload.py; подробности в PROGRESS.

Текущий приоритет пользователя: исправить чёрные модели gameplay. Deferred
prototype проверен на2MiB/47 samples, но не включён в normal GPU. Продолжать
с full capture frame37095/event69 и других opaque draws: shader inputs, lighting
и EDRAM writes; не подменять visual fix очередной async-оптимизацией.
