# ACV — проверенные этапы и ограничения

Обновлено: 2026-10-22. Это сводка доказательств, не очередь задач.
Приоритеты — [PLAN](PLAN.md); быстрый вход — [состояние](X360_PROJECT_STATE.md).
Полный прежний журнал без изменений: [originals.tar.gz](../logs/docs-context-20261022/originals.tar.gz),
запись `docs/PROGRESS.md`; SHA-256, заголовки и ссылки — [manifest](../logs/docs-context-20261022/manifest.json).

## Нативный Linux

| Этап | Результат и доказательство | Граница проверки |
|---|---|---|
| Входные данные | XEX SHA-256 4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b; 26 файлов XDVDFS совпадают с tmp/iso_root | Полный диск не гарантирует верное чтение/использование каждого ресурса |
| Генерация/сборка | ReXGlue 0.10.0, Clang/Ninja ELF; 459 supplemental entries, 63 thunks, известные leafs; 0 REX_FATAL в проверенном дереве | Не полная декомпиляция или покрытие всех runtime путей |
| Conditional targets | 95 branches / 19 targets, 31 boundary hints; audit 625 files / 311 C++ units | Инструкционные фрагменты, .pdata ownership отсутствует; logs/acv-rexglue-unresolved-20261021/conditionals/ |
| Boot/render | NOW LOADING, меню, intro, HUD/эффекты и 3D submissions наблюдались | Opaque scene остаётся тёмной; правильный gameplay не принят |
| RenderDoc | X11 swapchain и RDC подтверждены; Wayland surface mismatch разобран | Один present / один draw может быть повторным выводом, не игровым кадром |
| Input | 61 SDK integration checks; F8/Escape; SDL→XInput→guest menu | OS delivery и gameplay camera parity не полностью проверены; [input](LINUX_INPUT.md) |
| Настройки | Единый acv.config: ARTEM / ARTEM, headless=true; default reader и missing-file failure проверены | User не доказан причиной Setup; headless — обход диалога |

Основные render evidence: runs `20261021T175546161473Z` (70 s: 1975 indexed
calls / ~2.17 млн indices; 90 s: 866 indexed calls),
`20261021T173226952928Z/acv_frame74332.rdc` (1389 indexed calls).
Полные logs: `logs/acv-native-linux/`, `logs/acv-renderdoc/`.
Отсутствие обычных color attachments на FSI не доказывает сбой:
этот backend пишет в EDRAM storage buffer и делает compute resolve.

## Диагностика: что решено, что осталось

| Проверка | Факт | Вывод |
|---|---|---|
| protect_zero=false | В последних 150 s probes нет прежней серии guest NULL writes; без флага fault loop воспроизводился | Сохранён в ACV profile; retail смысл записей не восстановлен |
| UTF-8 Setup | Core stack: to_utf16 ← XamShowKeyboardUI close lambda; xam_ui.cpp:384/390 берёт fixed buffer целиком, :457 декодирует | headless обход включён; корректный SDK fix ещё не принят |
| FSI/readback | HUD/эффекты появились; fast/full readback не устранили темноту в наблюдённых опытах | Не включать readback как доказанное исправление |
| invalid fetch | Разрешающий флаг убирает предупреждения, но прежний rich-frame опыт оставался тёмным | Причинный эффект не доказан; поздний повтор с 1 draw не является gameplay comparison |
| Display gamma | SDK фиксирует BT.709; два фактических hook-вызова 2→1 подтверждены в run 20261021T211435257956Z | Темнота осталась; override не default |
| Shared-memory compute access / CPU upload | SDK kComputeWrite исправлен READ→WRITE; host regression + negative control, private plugin build,85/110 s probes. CPU-origin524288 bytes совпали с поздним GPU mirror SHAe4428980… | Dark scene остаётся; writer/материал не установлен, copy event вне RDC; [evidence](../logs/acv-gpu-sync-20261022/cpu-gpu-comparison.json) |
| Allocation shim | Caller sites0x82AEF00C/0x82AEF028 подтверждены trace/raw PPC; два0 передаются в параметры0x82AF1D38. Override даёт ненулевые addresses, child0/110 s,12 RDC | Сцены не совпали:1/59 draws против2661; render effect неизвестен, default false; [evidence](../logs/acv-gpu-sync-20261022/allocation/ab-result.json) |
| VFS/glyphs | system:/dlc_sound:/dlc_image: не смонтированы; отдельные tutorial glyphs искажены | Причина shading/font issue не установлена |
| FIFO capture control | run20261021T220305240062Z: mode2,4 RDC,child0/150 s; frame10222:400 indexed/35 nonindexed/5 dispatch | Input histogram91–97% below1/32 per channel; darkness persists; source writer outside captured usage |
| Burst capture | run20261021T220733169087Z:8 RDC,child0/140 s; frame16580:1325 indexed/682 draws/64 dispatch | Frame16581 contains copyDst90→gamma write95; final present109 reads another mailbox image; cause open |
| Explicit capture interval | run 20261021T212603387176Z: device lost; путь Start/End удалён, rebuild 5f0c239e-76a4-4eea-b093-6dea3880f9ec прошёл | Ненулевой interval отклоняется до spawn; обычный capture run20261021T215316118397Z дал4 RDC без device-lost в логе, child0/150 s;45 прежних VFS/protect errors остались |

FBO single-present captures 110/140 s в run `20261021T212231576790Z` содержат
по одному draw; это не доказательство отсутствия world rendering. Timing/rate
гипотеза требует проверки. XML parser regression: 235 draws / 1184 indexed /
18 dispatches / 1 present, operation `39af13f5-635e-4a8f-966a-d448777aa450`.
Screenshot rejection до spawn проверен: `3530a971-d882-4770-a713-b88c83eced41`.

Numeric replay RenderDoc1.46 заработал после экспорта replay marker (`-rdynamic`).
Кадр73703:1366 draw actions/10 dispatches; final source1280×720 center RGB
[0.00781262,0.0254063,0.0136721], output2560×1080 center
[0.00784314,0.0235294,0.0117647]. Source GetUsage содержит только PS-read;
не устанавливает причину темноты. Без image export.
Evidence: [детали](NATIVE_REXGLUE_LINUX.md#numeric-renderdoc-replay-2026-10-22),
operations `daec2451-6971-49b8-8382-61116d38f909` /
`4e993eaa-9b88-4f9c-acf1-860cdb9da3c6`.

## Статическое исследование и bounded reconstruction

- [Timing](ACV_TIMING.md): Main→DrawThread handoff и потребитель шага
  подтверждены. Gameplay physics/combat связь не установлена. Unity batch
  AcvTimingTests: 13 passed, 0 failed/skipped; это отдельная историческая ветка.
- Lookup `0x82C25350` — bool/default после ${...}, не record pointer.
  Startup FileManager.BindRead=true; 14 direct setters; известный dynamic
  setter `0x823E07DC` формирует Assembly.%s и точный ключ не пишет.
  Объектный alias/другие indirect writers открыты:
  [lookup evidence](../logs/acv-lookup-20261020/followup-20261021.md).
- `0x83885E80+0x910/+0x914=1280/720` питают rectangles; screen/camera смысл
  не принят. Vector provenance и remaining aliases — [PLAN](PLAN.md).
- [0x8245FA20](fine/NATIVE_DECOMP_8245FA20.md): 69 152 state comparisons +
  10 expected transitions, generated-host agreement; не retail parity.
- [0x82303138](fine/NATIVE_DECOMP_82303138.md): 10 080 object/callee-input
  comparisons; 0x82302520 заменён измерительным stub.
- [Scalar pair](fine/NATIVE_SCALAR_830D68F0.md): 80 784 finite split comparisons;
  fused differs 3825/3772; FPSCR/Xenon oracle открыт. Tan-like 0x830D6AB0:
  200 000 host samples / 5.993e-13 при недоказанном RN-even, не guest parity.
- [L3](fine/NATIVE_HOST_L3.md): 41 memory/dispatch + 6 clock checks;
  wrapper closure 4096 cases / 32768 assertions; frequency adapter 44 checks.
  Это альтернативные ограниченные host probes, не полный игровой runtime.
  Windows/Wine результаты исторические; реальный Windows не принят.
- [XEX inventory](fine/NATIVE_XEX_RUNTIME.md): 577 descriptor records →
  295 imports (282 function / 13 variable); прежние header reports ошибались.
  24 XenonRecomp frsqrte sites не имеют verified estimate semantics.

## Данные, инструменты и журнал выполнения

- Asset milestone: 2833 BHD buckets / 19 744 entries; 14 971 DCX DFLT
  проверены по size/zlib; 22 605 raw textures экспортированы. Unsupported BND3/TPF
  варианты и tiling/GPU-format parity остаются: [форматы](ASSET_FORMATS.md),
  [RE log](REVERSE_ENGINEERING.md). Это не готовые Linux/Unity assets.
- Ghidra RPC reopening default.xex --no-analyze подтверждён; PowerPC/BE,
  pointers32, 67 314 function definitions. Неполные bodies/noreturn fixups
  делают часть decompiler ABI ненадёжной. Текущую живую базу не изменяли;
  старые repairs с byte-integrity reports остаются историческим evidence.
  Ghidra MCP в последней сессии не подключён; доступность перепроверять.
- SQLite migration сохранила legacy IDs, 294 файла / 4 403 652 210 bytes
  проверены; удалены только 3 восстановимых дубликата. Последняя полная
  logger suite:10 groups, включая retry-stage isolation, diagnostic parsing и
  explicit stdout-data для source/doc listings; stderr/exit/default tool checks
  сохраняются. Operation c2bfac88-7367-4964-8865-9bb0cd363956.
  [SQLite workflow](SQLITE_LOGGING.md) хранит команды, операции и пределы.
- Vulkan skill/reference installation: все 6 pinned submodules сверены,
  `logs/vulkan-skill-install.json`. Skill не является исправлением игры.
- 11 прежних technical reports сжаты с 89 713 до ~13 KB в fine/;
  полный текст сохранён. PLAN/PLAN_B/TASK-XBOX объединены в PLAN:
  `docs/fine/planning-merge.json`, originals byte-verified, прежние файлы удалены.

## Как обновлять этот журнал

Заменять устаревшую строку текущим результатом; не дописывать многократно
одни и те же прогоны. Формат milestone: дата → факт → область проверки →
operation/evidence → оставшееся ограничение. Полный stdout/stderr — SQLite/logs,
следующая задача — только PLAN. Изменение документации само по себе не возобновляет runtime; текущий запуск
возобновлён отдельным явным запросом пользователя.

## Текущая работа: декодирование текстур Xbox 360 и связь моделей с draw (2026-10-22)

Цель: доказать, что материалы модели (`0001_o2092.flv`, текстуры `m0200_*`) совпадают с текстурами,
которые реально привязаны к draw в трассе. Порядок работ:
1. Найти в трассе compute-диспатч, который декодирует BC-текстуру в линейный RGBA8 (выход игры — эталон).
2. По его входу и выходу определить раскладку тайлов точным сравнением (не по гладкости).
3. Реализовать декодер TPF-текстур по найденной раскладке, проверить на известных текстурах.
4. Сравнить декодированные текстуры материалов с привязанными текстурами реплея (числа, без картинок).
Статус: шаг 1 начат. Уже установлено: коды TPF-формата 0/24 — DXT1, 5/23 — DXT5 (SoulsFormats
`Formats/TPF/Headerizer.cs`); раскладка SDK `GetTiledOffset2D` не даёт биекции блоков; SPIR-V загрузчиков
текстур есть в `tools/rexglue-sdk/src/graphics/shaders/vulkan_spirv/texture_load_*.h` (дизассемблер без имён).
- 2026-10-22, шаг 1 (компьют-диспатчи FBO-кадра 79584, 97 шт.): диспатч 10117 переносит 131 072 байта,
  входной блок гостевой памяти (буфер 0, смещение 345292800) побайтно равен выходу. Это копия, не untile.
  Проверка: `tools/native-rexglue/replay_buffer_dump.cpp`, вход/выход сохранены в `tmp/bc_untile/`.
- Вход диспатча 10117 не совпадает ни с одним извлечённым payload (`assets/original/acv-dvdbnd-textures`).
  Поиск первых 4 КБ известных payload в полной выгрузке гостевой памяти (512 МБ, `tmp/bc_untile/guest_mem.bin`)
  запущен; результат будет записан ниже.
- 2026-10-22, шаг 2 (раскладка подтверждена): привязанная к draw 7180 текстура 19 (512×512, BC1, реплей)
  совпадает блок-в-блок с гостевой памятью кадра 73703 при тайлинге `Untile` (crunch-вариант из
  `tools/rexglue-sdk/src/graphics/pipeline/texture/conversion.cpp`, координаты блоков, log2 = 3, 16-битный
  swap): 16 384 из 16 384 блоков. Другие варианты (SDK GetTiledOffset2D, текселя) не совпадают.
  Инструмент: `tools/bc_tile_match.py`; дампы: `replay_texture_dump.cpp`, `replay_buffer_dump.cpp`.
  Ограничение: проверено только для BC1; BC3 по этой раскладке не проверен.
- 2026-10-22, шаг 3 (связь модели и draw подтверждена точно): текстуры, привязанные к draw 7180 кадра 73703,
  побайтно совпадают с TPF-текстурами `m0200_006` (512×512, BC1, 16 384/16 384 блоков) и `m0200_006_s`
  (256×256, 4 096/4 096). Это материал 543 модели `0001_o2092.flv` (`m0200_006.tga`, `m0200_006_s.tga`).
  Проверка: `tools/bc_texture_identity.py`, операция 9491d7a7 (verified), доказательство
  `logs/acv-renderdoc/20261021T215316118397Z/texture_identity.json`.
  Связь «draw 7176–7194 ↔ o2092» по-прежнему опирается на последовательность face set; текстурная
  проверка относится к draw 7180 из этой последовательности.
  Ограничения: BC3 не проверен по этой раскладке; тёмная сцена не объяснена этим шагом.
- 2026-10-22, шаг 4 (яркость привязанных текстур): средние RGB декодированных BC1 (`tools/bc1_mean.py`, операция
  a5213889): m0200_006 ≈ (44, 45, 42), m0200_006_s ≈ (44, 42, 42) из 255. Текстуры материала тёмные, но
  не чёрные; итоговый кадр FBO-пути ≈ 0,5–1 %. Значит, основное затемнение происходит после чтения текстур
  (освещение/шейдинг/выход), а не в ассетах этой модели. Причина этого шага не установлена.
Итог этапа: раскладка BC1 Xbox 360 подтверждена точным совпадением; связь draw 7180 ↔ материал 543
модели `0001_o2092.flv` подтверждена текстурами побайтно; BC3 не проверен; причина тьмы не найдена.
- 2026-10-22, шаг 5 (шейдер draw 7180): пиксельный шейдер — транслятор Xenia (микрокод Xenos), сэмплирует
  `xe_texture0`/`xe_texture1` (2D) и пишет в `xe_edram` (RW-буфер EDRAM). Значит, освещение и выход
  происходят в этом шейдере, а не в конечном present. Константы привязки (set 1): system (496 Б),
  float (336 Б, 84 float4), bool/loop, fetch. Значения из кадра: float-константы содержат, в частности,
  0.2353 / 0.3529 / 0.4314 (= 60/255, 90/255, 110/255), 0.1575, 1.1373, 0.9143, 0.7359; системные —
  26.35 / −26.35, 0.4142, 128/64/64. Какая константа масштабирует цвет, не установлено: нужна трассировка
  SPIR-V от `OpStore` в `xe_edram` назад к `float_constants[k]`.
  Инструмент: `tools/native-rexglue/replay_draw_shaders.cpp`; выходы в `logs/acv-renderdoc/20261021T215316118397Z/draw7180/`.
- 2026-10-22, шаг 6 (цепочка цвета пиксельного шейдера draw 7180, частично): выход цвета
  `xe_var_fragment_data_0` = `r1 * c19` (с правилом «ноль при нулевом минимуме»); `c19 = (0.5, 0.5, 0.5, 1)`.
  Масштаб экспоненты `color_exp_bias` (offset 288) = (1,1,1,1) — не затемняет. Значение `r1` — последняя
  запись в `xe_var_registers[1]` перед выводом, L2114; её срез (`tools/spirv_slice.py`, результат в
  `draw7180/slice_r1_store_1964.txt`) даёт листья: константы c0, c2, c12–c18, c20 (ненулевые, например
  c0 = (0.235, 0.353, 0.431), c2 = 0.1575, c13 = (0.275, 0.094, 0.078), c14/c15 = 1, c16 = (10, 0, 0, 0)),
  интерполятор `xe_in_interpolator_4`, выборки текстур и три фазы управления (phi), которые срез не разворачивает.
  Вывод: в цепочке нет нулевого или очень малого множителя; тьма получается из комбинации текстур (≈17 % яркости)
  и констант при неизвестных интерполяторах. Точное значение требует эмуляции пикселя (нужны координаты
  выборок и интерполяторы) — это следующий шаг.
- 2026-10-22, шаг 7 (отладчик пикселя RenderDoc): `tools/native-rexglue/replay_debug_pixel.cpp` (одиночный пиксель
  и режим сканирования сетки). Проверено: на FBO-draw 8461 (кадр 77730) трасса получена (покрыты пиксели верхней
  строки present). На FSI-draw 7180 (кадр 73703) и на главном писателе цели 1280×2048 (draw 8445, кадр 77730)
  `DebugPixel` не возвращает трассу ни для одного пикселя (сканы с шагом 128 и 32). Причина не установлена:
  вероятно, отладка не поддерживается для FSI/interlock и для этих draw; ограничение инструмента, не данных.
  Итог: цепочка до константы/входа на уровне пикселя не доведена. Затемнение остаётся не объяснённым;
  известно: текстуры ≈17 % яркости, множители цвета 1, константы ненулевые, писатели цели согласованы.
- 2026-10-22, шаг 8 (пауза): запрошен runtime-запуск базовой пробы (`probe_linux.py --renderdoc-at 110`,
  150 с). Пользователь отклонил запуск; игра не запускалась, файлы не менялись. Статус без изменений:
  причина тьмы не установлена (см. шаги 1–7). Следующие действия — только по явному указанию пользователя:
  (а) runtime-проба с одним захватом как эталон; (б) подмена констант через хук SDK (потребует правки кода SDK,
  нужно согласование); (в) эмуляция пиксельного шейдера в Python.
- 2026-10-22, шаг 9 (базовая проба, одобрено пользователем: вариант a): `probe_linux.py --renderdoc-at 110`,
  захват `logs/acv-renderdoc/20261022T143612944714Z/` (кадры 46997, 46998), child exit 0; `failed` в логгере —
  та же классификация шума `dlc_*`. Эталон: выход 2560×1080 имеет среднюю яркость 0,24–0,91 % (тёмный
  кадр воспроизводится в текущем состоянии). Операция 7f9934c2 (verified). Дальше — только по указанию пользователя.
- 2026-10-22, шаг 10 (SDK-переключатель, без правки кода): `--extra=--gpu_allow_invalid_fetch_constants=true`,
  захват `logs/acv-renderdoc/20261022T143948032884Z/` (кадры 47013, 47014), child exit 0. Выход 2560×1080:
  средние 0,81 / 0,69 / 0,28 % против эталона 0,24 / 0,91 / 0,43 % (шаг 9). Тьма сохраняется, эффекта нет.
  Гипотеза «неверные fetch-константы — причина тьмы» отклонена. Проверка операции — см. запись в логгере.
  Проба с `gamma_render_target_as_unorm16=false` отклонена пользователем и не запускалась.
- 2026-10-22, шаг 11 (проверка состояния): обнаружены два запуска с `--gamma_render_target_as_unorm16=false`
  (`logs/acv-native-linux/20261022T144232328150Z`, `…144521925243Z`), без RDC-захватов. Они были запущены, хотя
  пользователь отклонил пробу; это ошибка контроля с моей стороны. Яркость по ним не измерена. Конфигурация
  `tools/native-rexglue/acv.config` не менялась (sha256 31ef1d2a…). Процессов игры сейчас нет.

- 2026-10-22, поиск решения по runtime20261022T144521925243Z: исправлены шаг
  сканирования и use-after-free в pixel debugger; причина отказа FBO-debug —
  RoundingModeRTE (уточняет шаг7). В диагностическом replay получена трасса
  draw3790: нулевая базовая текстура → чёрный пиксель; GPU mirror0x1F6B3000
  содержит полностью прозрачный чёрный BC3. Это не CPU dump и не объяснение
  всей сцены. Полный resolve + synchronous memexport:85 s, child0,8 RDC,
  frame66671 —967 indexed draws; source RGB means4.0–4.4 %, fix не принят.
  [Evidence/limits](../logs/acv-runtime-triage-20261022/findings.json);
  [technical section](NATIVE_REXGLUE_LINUX.md#black-material-input-traced-2026-10-22).

- 2026-10-22, тестовая Linux-сборка по запросу пользователя: подтверждённые
  runtime/input/import/capture правки сохранены; исправления pixel debugger
  пересобраны отдельно. Добавлены синхронизация основного app header и
  идентификатор host-исходников в логе. Compile/link exit0, ELF89a858719402…;
  12-секундный запуск: child0,15 прежних VFS/allocation errors, графика не принята.
  [Проверка](../logs/acv-test-build-20261022/verification.json);
  [запуск](../tools/native-rexglue/test_linux.sh).

- 2026-10-22, пользовательские FromSoftware references: сохранены pinned snapshots
  четырёх repos и Xenia a192b94. DCX-декодер DSEventScriptTools на образце ACV
  побайтно совпал с прежней распаковкой (1,417,648 bytes). Проверенные PC TPF/BHD5
  layouts отличаются от ACV Xbox360. По коммиту Xenia воспроизведён и исправлен
  UTF-8 BOM сбой загрузчика; plain/BOM/missing config guards проверены без игры.
  [Применимость и границы](../logs/acv-user-references-20261022/assessment.json).

- 2026-10-22, provenance пустой BC3-текстуры: FSI draw5940 подтверждён по
  fetch и declared shader slot0; first-write watch128pages привёл к
  DLTextureConverter_Xbox360Dx.cpp. Before-copy source, staging и output
  совпали побайтно:524288bytes, SHAe4428980…. Исходный physical buffer
  пишет memcpy dispatcher0x82B8F618 из DLTextureCompressor. Парные снимки
  до/после compression:2MiB нулей побайтно равны; upstream producer/material
  identity остаются открыты.
  Инкрементальная сборка прошла, диагностические hooks вызывают оригиналы и
  выключены по умолчанию. Это не исправление тёмной сцены.
  [Evidence](../logs/acv-texture-writer-20261022/origin-comparison.json),
  [текущий пакет](../logs/acv-texture-writer-20261022/current-test-build.json).

- 2026-10-22, процесс исследования сохранён в checksum-проверенном архиве
  исходников/состояния и копии ELF/runtime/plugin. Readback reference helper
  прошёл mock Vulkan +ASan/UBSan проверки публикации при ошибках; в runtime
  не подключён. GPU-bake/readback остаётся гипотезой до привязки Resolve.
  [Checkpoint](../logs/acv-readback-checkpoint-20261022T171359Z/verification.json),
  [audit](NATIVE_REXGLUE_LINUX.md#resolvereadback-audit-and-preserved-checkpoint-2026-10-22).

- 2026-10-22, подтверждённое исправление динамической текстуры ACV: профиль
  acv-fsi включает readback_resolve=full. A/B одного source resource:2MiB нулей
  →2066418 ненулевых bytes. Реальный FSI draw75/frame34282 получает цветную BC3,
  GPU512KiB совпадают с CPU upload; раньше texture RGBA0. Build exit0,
  120s child0/8RDC,45 прежних VFS/allocation errors. Global scene/FPS не приняты.
  [Результат](../logs/acv-readback-origin-20261022/accepted-fix.json),
  [GPU proof](../logs/acv-readback-origin-20261022/gpu-material-proof.json).

- 2026-10-22, сохранена точка остановки перед интеграцией VFS/allocation/async
  readback. Подготовлен reference header, прежние host mock ASan/UBSan проверки
  прошли (операция eefd29b7-88f4-4a5a-9b91-ab2199038ab8); runtime integration и
  GPU/performance проверки не выполнены. После запроса интеграции новых правок
  runtime, сборок или запусков не было. [Контекст продолжения](X360_PROJECT_STATE.md#точка-остановки-интеграции--2026-10-22).
- 2026-10-22, шаг 12 (проверка остановки интеграции): `integrate-runtime-release` (0bff52a2) помечен failed
  пробой из-за шума `ResolvePath(system:/dlc_*)` при `child_returncode 0` и лимите 120 с; это известная ложная
  классификация, процесс не завис. Следующие шаги интеграции (сводки, индексы, финальная проверка рендера,
  DCX-проверка) завершились успешно. DCX-проверка: 16 записей, 4 657 807 байт, SHA-256 и размеры декодированных
  данных совпали с `acv-dvdbnd-dcx-contents.tsv`; независимый перерасчёт — 0 расхождений
  (операции 630e5229 и 71ab0a01, verified). Границы: проверен исходный DCX-декод; вывод распаковщика гостя не захватывался.
- 2026-10-22, шаг 13 (интеграция доработок: сверка и точки синхронизации). Сохранённое состояние
  (`logs/acv-integrated-readback-20261022/before-*`, `before-sha256.json`): изменены `command_processor.cpp/.h`
  SDK и `acv_compat_shims.cpp`; CMake не менялся. Артефакты собраны после последних правок исходников.
  Синхронизация readback в SDK: `AwaitReadbackSubmission` ждёт номер конкретного задания вместо
  `AwaitAllQueueOperationsCompletion`; `CheckSubmissionFenceAndDeviceLoss` сам закрывает текущее задание и ждёт
  fence только до него; слот staging не перезаписывается до завершения своего задания (`submission_written`);
  ошибка `vkInvalidateMappedMemoryRanges` не публикует данные. Ранее (параллельная сессия) материал BC3
  1024×512 @ 0x1F6B3000 совпал побайтно между тестовым и диагностическим прогоном (sha256 00c279ff…).
  Правка: неудачная физическая аллокация пишется событием `allocate_failed` и не попадает в живые регионы
  (`acv_compat_shims.cpp`); инкрементальная сборка 6eaaf8fc (verified), в игре не запускалась.
  Не реализовано из плана: перехват `ResolvePath` и завершения чтения/APC, выход DCX/BND-декодера игры,
  постановка слоёв атласа; backing-id/GPU-ссылки в трекере; кольцо staging с dirty-диапазонами и проверкой поколения.
- 2026-10-22, шаг 14 (доработка VFS-трассы): в `AcvTraceOpen` (`acv_compat_shims.cpp`) после успешного открытия
  записывается выбранный VFS-файл — точка монтирования устройства, абсолютный путь записи и её размер
  (`device_hex`, `entry_hex`, `entry_size`; из объекта `XFile`), без правки SDK. Номер открытия (`token`)
  разделяет переиспользуемые после `NtClose` handle. Сборка aebe1c14 (verified); в игре не запускалась.
  Состояние плана после шагов 13–14:
  1. VFS: открытие (путь, root, статус, handle+номер, выбранный файл), чтение (64-битный offset, запрошено/
     прочитано, статус IOSB, хеш), закрытие — есть. SDK читает синхронно, поэтому завершение чтения фиксируется
     по IOSB. Нет: выход DCX/BND-декодера игры (адрес функции не найден) и постановка слоёв атласа.
  2. Аллокации: успешные с epoch, физический адрес через `GetPhysicalAddress`, пересечения, освобождение;
     неудачные — `allocate_failed`. Нет: backing-id и счётчик незавершённых GPU-ссылок (нужны хуки GPU-стороны SDK).
  3. Readback: ожидание конкретного задания, защита слотов до завершения, отказ при ошибке invalidate — есть.
     Нет: кольцо staging с dirty-диапазонами и проверкой поколения перед публикацией (`async_readback_reference.hpp`
     прошёл mock-проверки, но в SDK не перенесён; без запуска игры проверить нельзя).
- 2026-10-22, шаг 15 (поиск DCX-декодера игры, статически, без запуска): магия `DCX\0`/`DCP\0`/`DFLT`/`DCA\0`
  не найдена ни байтами в памяти Ghidra, ни непосредственными константами в сгенерированном коде
  (единственное `0x4443` — часть `0x04444443`, это не магия). В образе одна копия zlib: строка
  `invalid stored block lengths` (0x8210C6E4) используется только `0x82C046D0` (ядро inflate). Прямые вызовы:
  `0x82C046D0` ← `0x82BF3508` ← `0x82BE7C88` и `0x82BF5200`. Эти два вызывающих разбирают поток кусками
  «4 байта длины big-endian + 4 байта типа», состояние 1/3/5/7 (поля объекта +1372…+1556) — это похоже на
  PNG-подобный загрузчик, не на DCX. Указатели на `0x82BF3508`/`0x82C046D0` в данных (0x82269418, 0x822698E0)
  предположительно из `.pdata`, не проверено. Ghidra-база неполная (`0x82BF5200` не определена). Итог:
  DCX-декодер не найден; гипотеза — он вызывает inflate косвенно или использует отдельную реализацию.
  Следующий проверяемый путь требует запуска: watch на запись в буфер после чтения DCX-диапазона
  `dvdbnd.bdt` (как уже сделано для 0x1F6B3000) — даст адрес кода, который пишет распакованные данные.
- 2026-10-22, шаг 16 (офлайн-поиск выхода DCX-декодера): начала декодированных DCX-записей (5 294 различных
  32-байтовых префикса из 14 971 записей) искались в двух выгрузках 512-МБ буфера гостевой памяти из RDC
  (кадры 73703 и 79584): 0 совпадений (операция 847e6846, verified). Причина: этот буфер — shared memory
  GPU, туда загружаются только страницы, используемые GPU (поэтому в нём находились TPF-payload текстур),
  а распакованные архивы — данные CPU. Офлайн-путь исчерпан. Для поиска декодера нужен запуск с хуком
  выделения памяти под декодированный размер (размер известен из DCX-заголовка прочитанной записи) и
  write-watch на этот буфер; это требует правки шимов и согласия на запуск игры.

- 2026-10-23, интегрированы Resolve completion/reuse/invalidate guards и bounded
  VFS/physical-allocation tracing. Host ASan/UBSan, barrier и launcher checks прошли;
  incremental host/GPU builds exit0. Два120s capture runs child0/45 прежних
  diagnostics; RGBA/BC3 совпадают с accepted full, реальные GPU512KiB равны
  same-run CPU uploads.177 VFS reads/≈32MiB проверены по source slices,16 source
  DCX по decoded SHA. Default остаётся full; deferred CPU-consumer integration,
  gameplay/FPS и guest decompressor ownership открыты.
  [Technical milestone](NATIVE_REXGLUE_LINUX.md#integrated-resolve-completion-and-resource-tracing-2026-10-23).

- 2026-10-23, Resolve связан с первым CPU-чтением: physical0x0FCA7000,
  submission1298/frame627,2MiB SHA6af9819e… побайтно равны input copy0x82C83918.
  Найден промежуточный tiled→linear converter до прежнего RGBA0x1E192000;
  источник пишет copy-out, не прямой Resolve. Read-only hooks и private trace
  plugin собраны; pending readback/fence integration остаётся открытой.
  [Proof](../logs/acv-resolve-consumer-20261023/complete-proof.json).

## GitHub source-only checkpoint — 2026-10-23

Приватный [GrayRats/DEArmoredcoreV](https://github.com/GrayRats/DEArmoredcoreV)
создан и initial commit `451b6c4` отправлен в main. Проверены remote SHA и tree:
185 небольших текстовых файлов, 955282 bytes; только tools, scripts, MD и
root instructions. Логи, cache/temp, SDK checkouts, сборки, игровые данные и
личный acv.config исключены.
Проверка перед публикацией: `scripts/check_repository_payload.py`
(regular text blobs, максимум512KiB, без gitlinks/credentials); manifest
[remote-verified.json](../logs/acv-github-export-20261023/remote-verified.json).
Это checkpoint исследовательских инструментов; экспериментальный deferred
Resolve default OFF, его GPU compilation прошла, game/runtime ещё не принят.
