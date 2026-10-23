# Scalar helpers 0x830D68F0 / 0x830D69D0

Статус: Ограниченные scalar-реконструкции отработаны.

**Подтверждено в отчёте:** Две .pdata-функции по 224 байта: [0x830D68F0,0x830D69D0), [0x830D69D0,0x830D6AB0). Finite-domain host comparisons с generated C++ согласуются с sin/cos-подобным поведением.

**Ограничения:** Имена не восстановлены. Fused/split различаются; FPSCR, исключения, полный guest ABI и Xenon parity остаются открытыми.

Доказательства: [acv_830d68f0.hpp](../../tools/native-decomp/acv_830d68f0.hpp).
Текущая работа: [PLAN](../PLAN.md). Архивирование не закрывает открытые вопросы.

Полный текст без потерь: [originals-20261022.tar.gz](originals-20261022.tar.gz),
запись `docs/NATIVE_SCALAR_830D68F0.md`. Хеш оригинала и причина переноса: [manifest.json](manifest.json).
