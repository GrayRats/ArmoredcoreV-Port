# Кандидат scalar helper 0x830D6AB0

Статус: Статическое исследование и ограниченный host-модельный тест выполнены.

**Подтверждено в отчёте:** Instruction-derived candidate [0x830D6AB0,0x830D6B84), без .pdata owner. Константы 2/π, split π/2, polynomial/reciprocal поддерживают tan-подобную гипотезу.

**Ограничения:** Не верифицированная retail-реконструкция. Host-модель использует недоказанное RN-even допущение для fctid; нужны FPSCR evidence и доверенный execution oracle.

Доказательства: [tan-host-probe.json](logs/acv-math-followup-20261021/tan-host-probe.json).
Текущая работа: [PLAN](kb:PLAN). Архивирование не закрывает открытые вопросы.

Полный текст без потерь: [fine__originals-20261022.tar.gz](kb-file:fine__originals-20261022.tar.gz),
запись `tools/acv-context/docs/NATIVE_SCALAR_830D6AB0.md`. Хеш оригинала и причина переноса: [FINE_MANIFEST](kb:FINE_MANIFEST).
