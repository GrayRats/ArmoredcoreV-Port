# Инвентаризация границы XEX/runtime

Статус: Инвентаризация завершена; реализация сервисов этим не подтверждена.

**Подтверждено в отчёте:** Образ 0x82000000, entry 0x82B91F88; stack 0x100000. TLS: 64 slots, VA 0x83991000, data/raw 1429 байт. Установлено 295 импортов: 282 функции и 13 переменных.

**Ограничения:** Поля XEX не задают политику потоков/TLS и семантику kernel services. Старые некорректные отчёты optional headers не использовать как источник.

Доказательства: [runtime-inventory.json](logs/acv-host-20261021/runtime-inventory.json).
Текущая работа: [PLAN](kb:PLAN). Архивирование не закрывает открытые вопросы.

Полный текст без потерь: [fine__originals-20261022.tar.gz](kb-file:fine__originals-20261022.tar.gz),
запись `tools/acv-context/docs/NATIVE_XEX_RUNTIME.md`. Хеш оригинала и причина переноса: [FINE_MANIFEST](kb:FINE_MANIFEST).
