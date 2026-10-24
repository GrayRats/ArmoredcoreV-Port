# ACV — текущий статус

[English](PLAN.md)

Актуально: 2026-10-24. Подробная очередь реализации — в
[PLAN_GUI.MD](../PLAN_GUI.MD). Здесь сохранены краткие факты о native Linux
линии и решения пользователя.

## Цель и границы

Цель — native Linux запуск Armored Core V через ReXGlue. Исходный ISO read-only;
исполняемые и временные данные лежат в `tmp/`, логи и evidence — в `logs/`.
Не смешивать ReXGlue с независимой веткой XenonRecomp и не считать generated
PPC C++ восстановленным исходником.

Обычный диагностический запуск использует
[acv.config](../tools/native-rexglue/acv.config) и профиль ARTEM/ARTEM.
Диагностический ELF: `tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv_linux_probe`.
Настройки, профили и launcher implementation идут по [PLAN_GUI.MD](../PLAN_GUI.MD).

## Подтверждено

- Пользователь подтвердил исправление чёрных текстур мехов. Установленная
  причина shader fetch — неверный dword `exp_adjust`; сохранять принятый GPU
  plugin `90d8ef1a…`. [Техническое evidence](NATIVE_REXGLUE_LINUX.md#texture-rendering-fix-confirmed-by-user-2026-10-24).
- Resolve/readback и lifetime получили синхронизацию, проверку диапазонов,
  обработку ошибок и отложенное освобождение staging ресурсов. Тесты и сборка
  прошли. По решению пользователя пропускать остаточные DLC/readback сигналы:
  отсутствующие sound payloads, optional `system.properties`, отсутствие
  gameplay readback в startup smoke и нефорсированную eviction под нагрузкой.
- Четыре нужных `dlc_image:` контейнера извлекаются из retail `dvdbnd` и
  монтируются read-only. Пять оставшихся sound payloads отсутствуют в доступном
  ISO; решение пропускать запросы не означает, что данных стало больше.

## Документы и архив

- [PLAN_GUI.MD](../PLAN_GUI.MD) — текущая подробная очередь реализации и её
  критерии готовности.
- [PROGRESS](PROGRESS.md) — выполненные этапы и ограничения.
- [STATE](X360_PROJECT_STATE.md) — короткий указатель продолжения.
- [NATIVE_REXGLUE_LINUX](NATIVE_REXGLUE_LINUX.md) — runtime evidence.
- [AGENTS](../AGENTS.md) — правила работы.

Предыдущая версия этого плана сохранена в
[`docs/fine/PLAN_worktree_before_compression_20261024.md`](fine/PLAN_worktree_before_compression_20261024.md);
контрольная сумма и исходный Git blob записаны в
[`PLAN_worktree_before_compression_20261024.json`](fine/PLAN_worktree_before_compression_20261024.json).
Более раннее объединение планов описано в
[`planning-merge.json`](fine/planning-merge.json).
