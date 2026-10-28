# ACV — актуальная точка продолжения

2026-10-28: интерфейс TRG обновлён в существующем ACV-Linux-release-1.0,
patch `2026-10-28-launcher-guidance`. Видимая подсказка исполняемого файла,
36 пресетов разрешений/фильтр и инструкция 10-битного запроса обновлены. Окно игры: `ARMORE CORE V [Rexglue]`. Русский заголовок X11 исправлен; добавлены
лимиты 30/60/120, дополнительные разрешения и фактический статус 10/8 бит; FSR2/3 убраны. Убраны пользовательские ссылки на PLAN;
исправлены черновик настроек/preview, сброс графики и формы/копии профиля.
[Изменения и проверки](LAUNCHER_PACKAGE.md#interface-polish--2026-10-28),
[publication](../logs/acv-launcher-polish-20261028/publication.json).
Игровые ELF/runtime/GPU/FFX принятые, их SHA сохранены.
Ниже — принятая native правка и сохраняющиеся ограничения игры.

Обновлено2026-10-28. Native off-screen visibility fix принят пользователем:
«Да, помогло». [Детали](ENGINE_OFFSCREEN_20261028.md),
[checkpoint](../logs/acv-engine-offscreen-20261028/checkpoint.json).

Исправлено несовпадение local16:9 frustum с расширенной projection2560×1080.
Scope RAII связывает коррекцию с реально расширенной projection данного builder.
Actual retail sphere/SDK ASan/UBSan, FOV/HUD regression и build PASS.
Финальный160с20261028T012252054423Z child0/fatal0; на обоих потоках все eligible
patched (49467/18449 в последнем отчёте). Диагностика после90с.

ELF2fc173a9 (изменён только заголовок окна; прежний efa4756c сохранён) включён в существующий ACV-Linux-release-1.0; manifest186files и
baseline обновлены. Прежний ELF/metadata с SHA в logs/acv-engine-offscreen-20261028/release-before.
Runtime544bdaae/GPUfe4550d5/FFX0933159a, general1/parallel-copy2/backoff0.
Release frontend35с PASS: local libraries/environment, child0/fatal0, lock lifecycle.
Все собственные тестовые игры завершены; итог в checkpoint.
Оригинальные config/save SHA сохранены. CAS/FSR и другие разрешения не тестировать.

Это исправление отсечения видимых боковых объектов. Resource unload/refcount,
объекты за камерой и прежняя политика кэша не подавляются; отдельная задержка
повторной загрузки и стабильные FPS не доказаны этим результатом.

Предыдущий завершённый этап:

Последний запрос выполнен: отдельная итоговая папка
[ACV-Linux-release-1.0/start.sh](../../ACV-Linux-release-1.0/start.sh),
186файлов (~3.88GiB), текущий TRG/Streaming-r2 runtime и игровые данные.
[Release checkpoint](../logs/acv-release-1.0-20261027/checkpoint.json),
[пакет](LAUNCHER_PACKAGE.md). Прежние пакеты/baseline сохранены.
Новый релиз не закрывает перечисленные ниже engine/FPS ограничения.

Последняя техническая задача: [Очередь](../PLAN_GUI.MD),
[текущие изменения/остаток](RESOURCE_STREAMING_20261027.md).

Последний запрос: все настройки acv-launcher и оптимизация загрузки/выгрузки
локаций, моделей и ресурсов. Он расширяет прежний FOV/HUD-only scope.
CAS/FSR интегрированы **без тестов**, иные игровые разрешения не тестировать.
Диагностика gameplay только после90с. Скриншоты/PNG анализа запрещены.

Новый отдельный кандидат:
ACV-Linux-TRG-Streaming-20261027-r2/start.sh (`../../ACV-Linux-TRG-Streaming-20261027-r2/start.sh`, исторический пакет; каталог сейчас отсутствует),
185файлов, без Qt/ICU. ELF6c02660d/runtime544bdaae/GPUfe4550d5/FFX0933159a.
General deferred1 / parallel-copy2 / GPU-backoff0. Старый принятый FOVHUD пакет
и RUNTIME_BASELINE не заменены. Полные SHA и операции:
[checkpoint](../logs/acv-resource-streaming-20261027/checkpoint.json).

Добавлены общие настройки texture-cache1024/1536MiB/180s и readahead auto16MiB,
streaming/compatibility presets, своё разрешение/список режимов backend,
восстановление UI после ошибочных experimental settings. Один bounded archive
worker,8queued hints, fd inode lease; реальные чтения/сейвы/EOF неизменны.
Actual IO/eviction ASan/UBSan,5settings checks, patch replay, SDK/launcher build,
185hashes/dependencies,6UI pages и45s реальный frontend lock/lifecycle PASS.

Два210с сравнения завершены child0/fatal0. Sampling120–155с:
texture cache≈182MiB, evictions0 даже со старым384MiB soft limit;
чтение203–228MiB, диск0.8–3.5MiB. Большинство данных уже в RAM.
Preload399hints/0failures; это не доказательство ускорения загрузки.
**Модели/локации: преждевременная выгрузка не воспроизведена и не исправлена.**
Запрошена миссия/действие (вход, поворот камеры, возврат в область); ответа пока нет.
Далее нужен воспроизводимый переход, resource-build/decompression/lifetime trace,
потом ownership-safe retention/dependency preloading и longer memory plateau check.
Не подавлять Scr_UnloadInGameResource или refcount без доказательств.
NtReadFile SDK:213 принудительно синхронен; async ветка не реализована.
Не убирать true|| без корректных lifetime/APC/coherence/cancellation.
Вклад в задержки пока не измерен. Read-only raw82586050 подтверждает4cleanup calls;82A0E748 — virtual dispatch.

Оригинальные config/save SHA не изменены. Все собственные игровые/сборочные
процессы завершены; Ghidra RPC был доступен для чтения default.xex без анализа/
изменений DB. Прежние активные docs полностью сохранены с SHA:
[снимок](../logs/acv-resource-streaming-20261027/docs-before/sha256.json).

Стабильные60FPS, world-projected HUD/culling, настоящий10bit/YCbCr/temporalFSR
не доказаны этим этапом. FSI/full рабочий, FBO/full с чёрным gameplay отклонён.
Низкие16MiB/7F mirror и bulk Reset/Save/Restore вне общего deferred-пути.
[AGENTS](../AGENTS.md): команды через logger, оригиналы/сейвы сохранять,
generated PPC не исправлять как основной слой.
