# Архив отработанных этапов

Здесь короткие сводки завершённых этапов и отложенных веток. Открытые ограничения
сохранены; это не утверждение о завершении декомпиляции. Текущая Linux-работа:
[PLAN](../PLAN.md), [PROGRESS](../PROGRESS.md).

- [Установка ReXGlue и 360tools](REXGLUE_INSTALL.md) — Завершённый этап установки; текущая Linux-ветка описана в PLAN_B.
- [Отложенная Unity-ветка](UNITY_ARCHITECTURE.md) — Исторический этап; Unity/C# не требуются для текущей Linux-цели.
- [Установка и первая генерация XenonRecomp](XENONRECOMP.md) — Завершённый этап установки/генерации; альтернативная линия.
- [Покрытие Xenon-инструкций](XENON_MORE_INSTRUCTIONS.md) — Отработанная изолированная оценка; оставшиеся вопросы отложены.
- [Ограниченный native host L3](NATIVE_HOST_L3.md) — Завершённые host-пробы; полный host этой линии не построен.
- [Инвентаризация границы XEX/runtime](NATIVE_XEX_RUNTIME.md) — Инвентаризация завершена; реализация сервисов этим не подтверждена.
- [Статическая цепочка 0x82300948](NATIVE_DECOMP_82300948.md) — Этап статического dataflow отработан; семантика и parity открыты.
- [Ограниченная реконструкция 0x82303138](NATIVE_DECOMP_82303138.md) — Реконструкция wrapper и ограниченные сравнения завершены.
- [Ограниченная реконструкция 0x8245FA20](NATIVE_DECOMP_8245FA20.md) — Реконструкция bounded state operations и host-сравнения завершены.
- [Scalar helpers 0x830D68F0 / 0x830D69D0](NATIVE_SCALAR_830D68F0.md) — Ограниченные scalar-реконструкции отработаны.
- [Кандидат scalar helper 0x830D6AB0](NATIVE_SCALAR_830D6AB0.md) — Статическое исследование и ограниченный host-модельный тест выполнены.

Полные исходные документы сохранены без изменений в
[originals-20261022.tar.gz](originals-20261022.tar.gz). Проверка байтов и SHA-256:
[manifest.json](manifest.json). Пример чтения полного отчёта из корня проекта:

```bash
tar -xOf docs/fine/originals-20261022.tar.gz docs/NATIVE_HOST_L3.md
```

Основные инструкции, SQLite logging, данные timing, форматы ресурсов и текущий
ReXGlue Linux runtime остаются в активном docs/.

## Объединённые планы

Прежние PLAN, PLAN_B и TASK-XBOX объединены в [единый PLAN](../PLAN.md).
Полные исходники: [planning-originals-20261022.tar.gz](planning-originals-20261022.tar.gz).
Хеши, заголовки и исходные ссылки: [planning-merge.json](planning-merge.json).
