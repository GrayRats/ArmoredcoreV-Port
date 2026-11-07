# Архив отработанных этапов

Здесь короткие сводки завершённых этапов и отложенных веток. Открытые ограничения
сохранены; это не утверждение о завершении декомпиляции. Текущая Linux-работа:
[PLAN](kb:PLAN), [PROGRESS](kb:PROGRESS).

- [Установка ReXGlue и 360tools](kb:FINE_REXGLUE_INSTALL) — Завершённый этап установки; текущая Linux-ветка описана в PLAN_B.
- [Отложенная Unity-ветка](kb:FINE_UNITY_ARCHITECTURE) — Исторический этап; Unity/C# не требуются для текущей Linux-цели.
- [Установка и первая генерация XenonRecomp](kb:FINE_XENONRECOMP) — Завершённый этап установки/генерации; альтернативная линия.
- [Покрытие Xenon-инструкций](kb:FINE_XENON_MORE_INSTRUCTIONS) — Отработанная изолированная оценка; оставшиеся вопросы отложены.
- [Ограниченный native host L3](kb:FINE_NATIVE_HOST_L3) — Завершённые host-пробы; полный host этой линии не построен.
- [Инвентаризация границы XEX/runtime](kb:FINE_NATIVE_XEX_RUNTIME) — Инвентаризация завершена; реализация сервисов этим не подтверждена.
- [Статическая цепочка 0x82300948](kb:FINE_NATIVE_DECOMP_82300948) — Этап статического dataflow отработан; семантика и parity открыты.
- [Ограниченная реконструкция 0x82303138](kb:FINE_NATIVE_DECOMP_82303138) — Реконструкция wrapper и ограниченные сравнения завершены.
- [Ограниченная реконструкция 0x8245FA20](kb:FINE_NATIVE_DECOMP_8245FA20) — Реконструкция bounded state operations и host-сравнения завершены.
- [Scalar helpers 0x830D68F0 / 0x830D69D0](kb:FINE_NATIVE_SCALAR_830D68F0) — Ограниченные scalar-реконструкции отработаны.
- [Кандидат scalar helper 0x830D6AB0](kb:FINE_NATIVE_SCALAR_830D6AB0) — Статическое исследование и ограниченный host-модельный тест выполнены.

Полные исходные документы сохранены без изменений в
[fine__originals-20261022.tar.gz](kb-file:fine__originals-20261022.tar.gz). Проверка байтов и SHA-256:
[FINE_MANIFEST](kb:FINE_MANIFEST). Пример чтения полного отчёта из корня проекта:

```bash
tar -xOf kb-file:fine__originals-20261022.tar.gz tools/acv-context/docs/NATIVE_HOST_L3.md
```

Основные инструкции, SQLite logging, данные timing, форматы ресурсов и текущий
ReXGlue Linux runtime остаются в активном tools/acv-context/docs/.

## Объединённые планы

Прежние PLAN, PLAN_B и TASK-XBOX объединены в [единый PLAN](kb:PLAN).
Полные исходники: [fine__planning-originals-20261022.tar.gz](kb-file:fine__planning-originals-20261022.tar.gz).
Хеши, заголовки и исходные ссылки: [FINE_PLANNING_MERGE](kb:FINE_PLANNING_MERGE).
