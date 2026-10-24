# Ограниченный native host L3

Статус: Завершённые host-пробы; полный host этой линии не построен.

**Подтверждено в отчёте:** Проверены call-free 0x822B0818 / 0x822B0820 и clock-closure 0x822C8AD8 → 0x82B8F9C0. Реализованы ограниченные memory/dispatch и injected-clock проверки.

**Ограничения:** Это не загрузчик и не игровой runtime. Xenon clock policy, kernel ABI, исключения и retail parity не подтверждены. Windows-результаты только исторические; новые проверки требуют запроса.

Доказательства: [](../../tools/native-host-probe/).
Текущая работа: [PLAN](../PLAN.md). Архивирование не закрывает открытые вопросы.

Полный текст без потерь: [originals-20261022.tar.gz](originals-20261022.tar.gz),
запись `docs/NATIVE_HOST_L3.md`. Хеш оригинала и причина переноса: [manifest.json](manifest.json).
