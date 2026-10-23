# Архитектура проекта (ACV, Xbox 360)

Редакция 2026-10-21. `default.xex` извлечён, проанализирован и сгенерирован XenonRecomp. Для независимой Linux-проверки ReXGlue создан приватный проект; conditional branch targets уточнены по retail-байтам, и generation pass теперь не содержит выявленных unresolved-branch fatal paths. Linux C++ build запущен; загрузка игры ещё не проверена. Выходы двух рекомпиляторов не смешиваются.

Текущая целевая платформа: Linux. Windows-сборки, Wine и работа только для
Windows отложены до отдельного запроса пользователя. Существующие Windows
результаты сохраняются как исторические свидетельства.

## Принципы

1. Каждое утверждение: адрес, источник, уровень уверенности (подтверждено / гипотеза / не проверено). Результат инструмента не считается свойством игры.
2. Сгенерированный рекомпилятором код — **машинный перевод** PPC в C++, требующий отдельного runtime для запуска на x64 (Windows/Linux). Это не восстановленный исходник. Unity/C# не обязательны для декомпиляции.
3. Эмулятор (Xenia) — эталон поведения для сверки, а не часть продукта.
4. Данные и инструменты изолированы по играм: ACV — единственный подтверждённый тайтл.

## Слои

```
L0 Источники и свидетельства
   ISO/XEX -> tmp/scan/{text,pdata}.bin -> Ghidra (RPC & MCP) -> tools/xref_index.py -> logs/decomp.sqlite3 (операции/индекс)
   диагностика -> logs/errors.sqlite3; вывод/архив -> logs/output.sqlite3; provenance/очистка -> logs/artifacts.sqlite3
   Xenia Edge: trace (log_lr_at_instruction), чтение гостевой памяти (/proc/pid/mem)

L1 Статическая рекомпиляция кода        (XenonRecomp)
   XenonAnalyse -> switch TOML -> XenonRecomp (+ ppc_context.h) -> C++ shards
   Вход: список функций (.pdata + листовые функции без .pdata), адреса register-save helper, switch-таблицы

L2 Рекомпиляция шейдеров                (XenosRecomp)   [исходники есть, не собран]
   Xbox 360 shader binary -> HLSL/SPIR-V; нужен набор шейдеров, извлечённый из игры

L3 Хост-рантайм для рекомпилированного кода   [частичный изолированный native C++ probe]
   карта памяти гостя, kernel/xam-импорты, потоки, графика, ввод, ФС
   Для текущего XenonRecomp workflow нужен отдельно; установленный ReXGlue runtime пока с ним не интегрирован.

L4 Сверка (oracle)
   рекомпилированная функция <-> Xenia: те же входные регистры/память -> те же выходы

L5 Реконструкция / интеграция (необязательная цель)
   Читаемый C++ и проверки семантики; Unity/C# — только если понадобится
   Ассеты и игровой движок не входят в обязательные критерии декомпиляции
```

## Что к чему относится сейчас

Реконструкции `0x82303138`, `0x830D68F0`/`0x830D69D0` проверены в ограниченном
host-контуре с явно указанными внешними зависимостями и арифметикой; они не
подключены к полному runtime. Для scalar-пары split generated C++ и fused
кандидат дают разные биты результатов; выбор до L4 oracle остаётся открытым.
См. [область scalar-проверки](fine/NATIVE_SCALAR_830D68F0.md).

| Слой | Состояние | Доказательство |
|---|---|---|
| L0 | работает; Ghidra RPC в GUI-режиме, индексы по `.pdata` | `docs/PROGRESS.md` |
| L1 | основной XenonRecomp собран; baseline ACV: 797 shard-файлов. Изолированные instruction experiments снизили неподдержанные сообщения 38→24; в остатке `frsqrte`-оценка без подтверждённой Xenon-совместимой реализации. Switch `0x82EF87B0`: 29 case labels подтверждены отдельной генерацией | [XENONRECOMP.md](fine/XENONRECOMP.md), [XENON_MORE_INSTRUCTIONS.md](fine/XENON_MORE_INSTRUCTIONS.md) |
| L2 | исходники XenosRecomp склонированы, не собраны; формат ACV шейдеров и набор входов не установлены | `docs/PLAN.md` |
| L3 | ограниченный XenonRecomp host: memory/dispatch, injected-clock closure и один configurable frequency import; проверки Linux/UBSan проходят. Независимая ReXGlue генерация покрывает 19 byte-verified exit fragments и не оставляет известные 95 conditional fatal paths. Linux C++ build/launch, полный runtime и Xenon parity не подтверждены. Windows остаётся по отдельному запросу | [NATIVE_HOST_L3.md](fine/NATIVE_HOST_L3.md), [NATIVE_XEX_RUNTIME.md](fine/NATIVE_XEX_RUNTIME.md), [NATIVE_REXGLUE_LINUX.md](NATIVE_REXGLUE_LINUX.md) |
| L4 | не начат | — |
| L5 | необязательная линия; подтверждённый reference-компонент и Unity EditMode тесты есть, в геймплей не подключены | `docs/PLAN.md` |

## Решения по архитектуре (предложение, требует подтверждения)

1. **Рекомпиляция — основной путь анализа**, не автоматическая цель доставки. L4 сверяет малые функции по одинаковым входам/выходам в generated/native harness и Xenia. Unity/C# подключаются только при отдельной необходимости.
2. **Покрытие L1.** Baseline XenonRecomp сообщил 10 939 неподдержанных адресов; isolated `BBB_MoreInstructions` оставил 38. Ordered lookup Xenia классифицировал raw words как `frsqrtex` (24), `vnor128` (3), `vsldoi128` (1), `lhbrx` (8), `mulhdx` (2). Изолированные изменения добавили `vnor128`, убрали PPC403 alias для Cell/Xenon и добавили `lhbrx`, `mulhdx`; diagnostics снизились до 24 (`frsqrte`). Host probes: `vsldoi128` все 16 shifts, `vnor128` 1 048 576 lanes, `lhbrx` 3 aligned halfwords, `mulhdx` 9 signed cases. Это узкие проверки кода и генерации, не Xbox 360 trace, full-shard build или runtime verification. Открыто: подтвердить Xenon-compatible оценку `frsqrte`. Switch `0x82EF87B0` разрешён: signed `addi` даёт базу `0x82EF87B4`; все 29 целей внутри owner `.pdata`, TOML принят отдельной генерацией. Подробности — [XENONRECOMP.md](fine/XENONRECOMP.md) и [XENON_MORE_INSTRUCTIONS.md](fine/XENON_MORE_INSTRUCTIONS.md).
3. **Единый реестр функций.** Один файл (`tmp/functions.json` → в будущем `logs/functions.sqlite`) с полями: адрес, длина, источник границ (`.pdata` / эвристика / ручной), владелец-листовая ли, статус рекомпиляции, статус сверки. Сейчас границы разбросаны по `.pdata`, Ghidra и xref; расхождение уже привело к ошибке (привязка `mftb` к неверной функции, см. «Correction» в `PROGRESS.md`).
4. **Гейты качества.** Любой вывод, использующий граф вызовов, обязан пройти проверку на листовые функции без `.pdata` и на косвенные вызовы (`bctrl`, vtable); граф только по `bl` — нижняя оценка.
5. **Инструменты внешних авторов:** исходники в `tools/<имя>/`, сборка в `tmp/<имя>-build/`, локальные настройки — отдельными файлами. Не смешивать кодогенерацию и runtime разных проектов без проверки совместимости.

## Внешние инструменты

| Инструмент | Назначение (по описанию автора репозитория) | Состояние |
|---|---|---|
| hedge-dev/XenonRecomp | Xbox 360 PPC -> C++ | установлен, собран (`ddd128bc…`) |
| hedge-dev/XenosRecomp | шейдеры Xbox 360 -> HLSL (README), затем DXC -> DXIL/SPIR-V | исходники в `tools/XenosRecomp/` (коммит `990d03b2`), submodule догружаются, не собран |
| level0rd/c-xbox_tool_2.0.7 | Windows-программа для **оригинальной Xbox** (XISO, FTP, правка XBE); не рекомпилятор | в `tools/c-xbox_tool_2.0.7/` (коммит `758a5b67`), только как справочный материал, не запускалась |
| rexglue/rexglue-sdk | Xbox 360 PPC -> C++ и runtime | официальный Linux x86-64 SDK `v0.10.0` установлен в `/home/art/.local/opt/rexglue-sdk/0.10.0`; 360toolsUpdated и skill установлены. независимая Linux-кодогенерация ACV начата в tmp/acv-rexglue-linux; полный build/boot ещё не подтверждён |
| RenderDoc (`renderdoc`, cachyos-extra-v3 1.46) | захват и разбор кадров Vulkan: draw-вызовы, render target-ы, текстуры, итоговый swapchain | разрешён пользователем 2026-10-21 для диагностики чёрного экрана; установка через `sudo pacman -S renderdoc` (нужен пароль пользователя); захват только нашего `acv_linux_probe`, файлы `.rdc` в `logs/acv-renderdoc/` |

Подтверждено чтением README (2026-10-20):
- XenosRecomp заточен под Unleashed Recompiled, автор пишет «не ждите, что заработает из коробки». Нужны данные рефлексии констант внутри шейдерных бинарников; формат контейнера изучен «ровно настолько, насколько нужно для Sonic Unleashed». Для ACV формат шейдеров не установлен (R5).
- c-xbox_tool ориентирован на оригинальную Xbox (README: XISO, FTP на консоль, XBE, MXM/Menux). Для Xbox 360 и XEX применимость не показана, роль в рекомпиляции отсутствует. Бинарники `.exe` не запускались.

## Проверенный первый шаг L3

`tools/native-host-probe/` сохраняет ABI существующего hedge-dev XenonRecomp: `PPCContext&, uint8_t* base`. Резерв 4 GiB, проверяемые guest-pages, big-endian load/store и dispatcher работают для двух изолированных call-free функций `0x822B0818` и `0x822B0820`. Clang + UBSan: 41 проверка пройдена; Windows backend через VirtualAlloc/VirtualFree проходит те же 41 проверку под Wine. Это не тест на реальном Windows; RTTI-диагностика static link остаётся. Полные shards не компилировались; XEX не загружался, entry point не вызывался. Это синтетический CPU/memory probe, не L4-сверка с Xenia и не подтверждение совместимости ACV. Подробности и ограничения: [NATIVE_HOST_L3.md](fine/NATIVE_HOST_L3.md).


## Контроль исполнения и журналов

`tools/decomp-logging/` хранит операции/задачи в `logs/decomp.sqlite3`,
диагностику в `logs/errors.sqlite3`, сжатый полный вывод и архив журналов в
`logs/output.sqlite3`, каталог/историю очистки в `logs/artifacts.sqlite3`.
Существующие IDs и таблицы главной базы сохранены; старые event/artifact записи
отражены в отдельных файлах. Старые stdout/stderr previews помечены неполными.

Общие скрипты `decomp.sh`, `ghidra-rpc.sh`, `assets.sh`, `unity.sh` автоматически
входят в общий runner. Остальные команды выполняются через `cli.mjs run`;
результаты прямого MCP сохраняются и передаются в `ingest`. Политика для будущих
агентов — в корневом `AGENTS.md`. Нет перехвата произвольных внешних процессов.

Файлы legacy logs импортируются потоково, сжатые chunks проверяются обратным
чтением и SHA-256. Очистка оставляет зарегистрированные/упомянутые доказательства
и удаляет только точные дубликаты с проверенным архивом и audit-записью.
SQLite WAL не обеспечивает атомарность crash recovery между несколькими файлами;
индексы/диагностика повторно согласуются по IDs, незавершённые streams остаются
помеченными. Процедуры и реальные ограничения: [SQLITE_LOGGING.md](SQLITE_LOGGING.md).

## ReXGlue Linux — current state, 2026-10-21

Initial ReXGlue 0.10.0 generation produced 625 C++ files but exposed 95
conditional branches to 19 destinations. Original-byte listings show 18 targets
are `li r3,0; blr`, and `0x82500F14` is `lis/addi/blr`; none has a `.pdata`
owner. Supplemental byte-backed fragment extents are recorded in
`tools/native-rexglue/acv-conditional-targets.toml` without asserting parent
function ownership. A separate output generation took 492 seconds; audit of
all 311 C++ units found 19 definitions and zero unresolved-branch `REX_FATAL`
placeholders. Evidence: `logs/acv-rexglue-unresolved-20261021/conditionals/`;
verified generation operation `3d924902-d86e-4ebf-bcf2-7cc96efd1f42`.

CMake is configured for `generated/conditional-fragments`. The Linux Clang/Ninja
build is active and currently repeats codegen to register the generated depfile
before compiling. After build, perform the bounded launch with `tmp/iso_root`,
inspect logs, and verify XEX entry, graphics, resources and gameplay. The SDK
source audit found 40 warning-only imported functions; investigate them only if
runtime traces reach them. Full runtime, shaders/audio/input/networking and Xenia
or hardware parity remain unverified. Windows awaits a separate request; Unity/C#
is not required.

Rough completion estimate (judgment, 2026-10-21): codegen gate ~90%, Linux
build/boot milestone ~45% (build active), broad semantic recovery ~20%, overall
reconstruction ~15% complete / ~85% remaining. These percentages are planning
estimates rather than measured metrics.

## ReXGlue Linux build components — update 2026-10-21

- `tools/native-rexglue/probe_linux.py`: bounded launch wrapper. Options: `--seconds`,
  `--log-level`, `--screenshot-every` (currently `spectacle -a`, to be replaced by window
  capture), `--extra=--cvar=value` (passed to the SDK).
- `tools/native-rexglue/generate_adjusting_thunks.py` → `tmp/acv-native-adjusting-thunks.cpp`
  (63 thunks); `retail_leaves.cpp` for hand-checked leaves.
- `tools/native-rexglue/propose_data_referenced_boundaries.py` →
  `acv-data-referenced-functions.toml` (459 entries). Filters: separate XenonRecomp
  registry (proposal source only), original-byte CFG contiguity, exits, `.pdata` and
  local-label interiors. Code-materialized pointers (`lis`/`addi` pairs) are included.
- `tools/native-rexglue/audit_data_referenced_codegen.py`: checks registration, definition
  and `REX_FATAL` against the generated tree.
- `tools/native-rexglue/validate_adjusting_thunks.py`: class scan of all `lwz`/`b` pairs.
- `tools/native-rexglue/sample_codegen.sh`: read-only `eu-stack` sampler of the codegen process.
- `tools/rexglue-graph-opt/` (isolated experiment): `indexed/src/codegen/phase_gapfill.cpp`
  replaces the O(G·N) absorb scan; CMake prefers `indexed/` copies of codegen files.
  Not yet verified against the release output.
- Data flow: original XEX → boundary TOMLs → ReXGlue codegen (`acv_linux_probe_manifest.toml`
  includes all three TOMLs) → generated C++ → CMake Clang build with `retail_leaves.cpp`
  and the thunk file → `acv_linux_probe` → `probe_linux.py`.

## Linux host input adapter — 2026-10-21

The private ReXGlue Linux probe uses the installed SDK's MnK driver as a
synthetic XInput controller for player one. The durable profile is
`tools/native-rexglue/input-profile.json`; `probe_linux.py` supplies its cvars.
The app hook in `acv_linux_probe_app.h` controls mouse capture (F8/Escape) and
retains SDK overlay key processing. No retail PPC function was replaced for
input. This is a host adapter, not a reconstructed game input system.

Optional persistent user data stays under workspace `tmp/`. Per-run evidence
stays under `logs/acv-native-linux/` and execution records in the existing SQLite
ledgers. Error samples/duplicate forwarding are bounded; eight unhandled guest
access violations stop only the child launched by that probe. Raw SDK logging
and rotation remain SDK behavior. [Controls and validation](LINUX_INPUT.md).
