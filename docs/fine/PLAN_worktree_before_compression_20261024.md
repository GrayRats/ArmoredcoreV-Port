# ACV — единый план Linux и декомпиляции

Актуально: 2026-10-24.
Новый план реализации по запросу пользователя: [PLAN_GUI.MD](../PLAN_GUI.MD) —
сохранения, постоянные профили, лаунчер Qt 6 LTS, экран/цвет/FPS и улучшения
графики. Подробная очередь этой новой работы хранится в указанном файле;
реализация пока не начата. Разделы ниже сохраняют статус прежних работ.

Читать вместе с [AGENTS](../AGENTS.md), [журналом](PROGRESS.md),
[архитектурой](ARCHITECTURE.md) и [RE evidence](REVERSE_ENGINEERING.md).
Полный исходный архив трёх планов хранится локально в
`docs/fine/planning-originals-20261022.tar.gz` и не публикуется в source repo;
проверка, исходные заголовки и индекс ссылок доступны в
[planning-merge.json](fine/planning-merge.json).
Старые проценты готовности и статусы «ещё не собрано» являются историческими.

## Цель и границы

Нативная Linux-сборка Armored Core V доходит до геймплея и показывает корректные
кадры; управление, ресурсы и устойчивость подтверждены проверками. Параллельная
цель — понятная C/C++ реконструкция поведения по retail evidence.
Машинный перевод PPC в C++ не является восстановленным исходником.
Windows/Wine — только после отдельного запроса.
Текущая исполняемая линия — ReXGlue; XenonRecomp сохранён как независимая
исследовательская линия. Не смешивать их generated code/runtime без проверки.

## Правила работы

- Исходный `../Original/Armoredcore/iso.iso` только читать.
  Проект: ``; выходы — tools/, tmp/, logs/, docs/.
- Для вывода указывать guest VA или SDK file:line, источник и уверенность:
  подтверждено, гипотеза, не проверено. Отсутствие прямых xrefs не исключает
  indirect calls. Не называть неизвестные поля/функции по сходству.
- Новые C/C++ реконструкции помечать `// @retail 0x...`; сохранять исходные
  инструкции, generated code и ручную модель раздельно. Не заменять проверенную
  реализацию непроверенной. Binary matching заявлять только по реальному oracle.
- Живую Ghidra-базу не менять. Структурный ремонт/.pdata — на отдельной копии
  после разрешения. Не трогать чужие процессы, ptrace_scope, sudo, пакеты,
  системные и пользовательские настройки без явного разрешения.
- Не сохранять скриншоты/PNG-превью для анализа. Использовать логи,
  RenderDoc RDC/XML/replay, инструкции, память и Ghidra RPC. Старые уникальные
  доказательства сохранять; автоматическое удаление не разрешено.
- Работать малыми проверяемыми итерациями, исправлять зависимости раньше caller.
  Не повторять одинаковый сбой; после ~20 неудачных ревизий кандидата записать
  blocker и перейти к независимому. Отчитываться по существенным проблемам.
- Runtime-работа возобновлена явным запросом пользователя после документации.
  Для запусков использовать штатный acv.config; launch.json сохраняет его SHA-256.

## Единый конфиг и запуск

Использовать [tools/native-rexglue/acv.config](../tools/native-rexglue/acv.config):
`pilot_name=ARTEM`, `gamertag=ARTEM`, `headless=true`. probe_linux.py читает его
по умолчанию; отсутствующий файл должен завершать запуск ошибкой, а не переходом
к SDK имени User. Сохранять настройки пользователя; любые диагностические
переопределения записывать явно. Не подменять конфиг временным при обычном запуске.

ELF: `tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv_linux_probe`.
Данные: `tmp/iso_root`; user/cache — внутри tmp/. Профиль acv-fsi передаёт
`protect_zero=false`, `render_target_path_vulkan=fsi`, `readback_resolve=full`;
`--graphics-profile=sdk` нужен только для сравнений. Full принят для восстановленной
динамической текстуры; gamma/allocation experiments остаются opt-in. [Клавиши и мышь](LINUX_INPUT.md).

```bash
node tools/decomp-logging/cli.mjs run --stage linux_probe --timeout-ms 175000 -- python3 tools/native-rexglue/probe_linux.py --seconds 150 --video-driver x11 --user-root tmp/acv-mnk-profile
```

Это ограниченная диагностическая сессия, не постоянный игровой launcher.
Автоввод `acv_probe_keys` / `acv_probe_start_after` выключен по умолчанию;
он проверяет внутренний SDL-путь, а не доставку реальной клавиатуры ОС.

## Проверенное состояние Linux

| Часть | Что установлено | Практический предел |
|---|---|---|
| Инструменты | ReXGlue SDK 0.10.0, private Clang/Ninja build скомпилирован | Не полная semantic recovery |
| Discovery | 459 supplemental entries + 63 adjusting thunks + leaf; 625 generated files / 311 C++ units, 0 REX_FATAL в проверенном дереве | Не гарантия всех indirect runtime paths |
| Conditional branches | 31 boundary hints; 95 branches к 19 целям проверены по XEX, 18 двухинструкционных возвратов и 0x82500F14 из 3 инструкций | У целей нет .pdata owner; ownership не восстановлен |
| Диск | 26 файлов совпадают с XDVDFS по SHA-256 | system:/dlc_* не являются отдельными top-level файлами |
| Исполнение | Проходит NOW LOADING; меню, intro, HUD/эффекты и 3D draw submissions наблюдались; пользователь подтвердил исправление чёрных текстур мехов | Регрессия миссии, прочие материалы/освещение, ввод и полная совместимость ещё не приняты |
| Input | 61 host SDK integration checks, F8/Escape, внутренний SDL→XInput→guest menu | Gameplay camera parity и полный физический input тест открыты |
| RenderDoc | X11 создаёт swapchain и RDC; Wayland capture был несовместим с включёнными surface extensions | Сам факт present/capture не подтверждает игровой кадр |
| SQLite | 4 DB и execution logger работают, история сохранена | Необёрнутые операции автоматически не фиксируются |

Codegen evidence: [conditional audit](../logs/acv-rexglue-unresolved-20261021/conditionals/).
Runtime evidence: `logs/acv-native-linux/`, `logs/acv-renderdoc/`.
Предыдущие утверждения «не собран/не запускался» в старых источниках заменены
этим статусом; полная совместимость, audio/network/save и длительная игра не приняты.

## Разобранные проблемы и результаты экспериментов

- **Чёрный/тёмный кадр.** FSI дал HUD/эффекты и часть сцены. Run
  `20261021T175546161473Z`: в 70 s — 1975 indexed draws / 2.17 млн indices /
  464 passes; в 90 s — 866 indexed draws. Отсутствие геометрии нельзя выводить
  из отдельных 1-draw captures: это может быть загрузка или повторный UI present.
  Пустые color attachments на FSI ожидаемы для storage-buffer EDRAM; это не
  доказательство неисправного resolve. Нужны shader/EDRAM/resolve/present данные.
- **Protect zero.** Без protect_zero=false наблюдались тысячи записей guest 0
  и fault-loop abort; с false последние 150-секундные пробы обходились без них.
  Прямое retail назначение NULL writes не установлено. Первичный источник Xenia:
  [issue 280](https://github.com/xenia-project/game-compatibility/issues/280)
  (4E4D084F; наш title 4E4D0853 — учитывать различие).
- **UTF-8 Setup.** Core backtrace в `logs/acv-asset-io/` ведёт в
  rex::string::to_utf16 из XamShowKeyboardUI. SDK xam_ui.cpp:384/390 берёт весь
  fixed buffer, :457 использует throwing conversion. Нужен корректный размер
  UTF-8 буфера/границы символов и строка до NUL; tolerant decoder требует решения
  по контракту. headless=true обходит диалог; исправление SDK не подтверждено.
  Само имя User не доказано причиной зависания Setup.
- **Readback.** FSI + vulkan_readback_resolve=true и readback_resolve=full
  не устранили тёмную сцену в наблюдённых прогонах. Legacy boolean выбирает fast,
  не full. Сравнения не frame-for-frame; default не менять по этим опытам.
- **Invalid texture fetch.** cache.cpp:903–924 отклоняет kInvalidTexture без
  gpu_allow_invalid_fetch_constants. Run `20261021T170902456991Z` убрал warnings,
  но не темноту. Повтор `20261021T211711835573Z` дал только 1-draw capture и
  закончился раньше лимита; по нему нельзя судить о gameplay shading.
- **Gamma.** SDK xboxkrnl_video.cpp:38,150–157 возвращает type 2 (BT.709).
  Opt-in import shim поддерживает ACV_DISPLAY_GAMMA_TYPE / --display-gamma-type.
  В `20261021T211435257956Z` два вызова 2→1 подтверждены, затемнение осталось.
  Guest call 0x82BB6C20 → import 0x83537294; r3=sp+0x54, r4=sp+0x50.
- **Physical allocation.** Два запроса sizes 0x40000/0x800000 с protect
  0x20000400 не проходят SDK READONLY/READWRITE check. --compat-mm-wc-rw
  добавляет READWRITE; 2 перехвата без bad-protection diagnostics наблюдались,
  render effect не доказан. Retail поведение не установлено. С 2026-10-23 включено в acv.config
  (`[compat] mm_wc_rw = true`) по решению пользователя: обе аллокации проходят, `bad protection bits` 0 (PROGRESS, шаги 24–25).
- **VFS/текст.** Read-only `dlc_image:` теперь отдаёт четыре runtime-requested
  TPF-файла из retail dvdbnd; 70s smoke прошёл child0 без failed dlc_image opens.
  Остаются неизвестные DLC-запросы/отсутствующие банки `dlc_sound:` и tutorial
  glyph decoding/font path. VFS fix не является доказанной причиной texture shading.
- **Capture timing.** FBO run `20261021T212231576790Z` на 110/140 s содержал
  по одному выводу, не пригоден для gameplay comparison. Новый explicit
  --renderdoc-window-ms=100 в `20261021T212603387176Z` завершился device lost,
  exit -6, без RDC; SQL operation edad4a2c-7d4b-445e-9f49-447b7ee1b88c.
  Этот вариант не принят и удалён из host-кода; ненулевой CLI interval теперь
  отвергается до запуска. Incremental rebuild operation
  5f0c239e-76a4-4eea-b093-6dea3880f9ec прошла; input/config guards проверены
  operation214f3d6b-e316-42e3-bba3-33f8917f7157. Обычный capture восстановлен: run20261021T215316118397Z, 4 RDC, child exit0 после150 s; ошибок VFS/protect всё ещё45, gameplay не принят. Инструмент
  [summarize_capture.py](../tools/native-rexglue/summarize_capture.py) считает
  команды XML; regression согласуется с 235 draws / 1184 indexed / 18 dispatches.

## Статус DLC и GPU readback

Состояние после подтверждения texture fix пользователем:

Обычная пересборка уже защищена от возврата старого GPU: retained manifest/hash
проверены тестом и реальной сборкой. Исторические replay probes74/79 не имели
одинакового входа; они не используются как основание для активной задачи.

1. DLC/VFS: локальная проверка завершена. 122 доступных sound-ресурса
   сопоставлены; пять имён остаются отсутствующими в текущем retail ISO, включая
   `acv_talk_jp.fev` после корректного разрешения `VoiceLang=jp`. Для реального
   исправления этих пяти запросов нужен образ/пакет DLC с payload; не подменять
   банки нулевыми файлами или неподтверждёнными alias. `system.properties` —
   optional loader fallback; четыре `dlc_image:` контейнера восстановлены.
2. GPU MEMEXPORT/readback: исправлены range validation, alias по layout,
   повторное использование slot только после submission ticket, invalidate
   error handling и deferred destruction при ошибке wait; ошибки доходят до
   caller. Actual-source ASan/UBSan regressions, SDK Release/native package и
   75s startup smoke прошли. Smoke не заходил в gameplay и не вызывал
   Resolve/MEMEXPORT, поэтому end-to-end readback надо подтвердить в игровом

**Triage decision (user, 2026-10-24): пропускать эти остаточные сигналы.** Не
держать план открытым из-за пяти отсутствующих DLC sound banks, optional
`system.properties`, отсутствия gameplay readback в startup smoke или отсутствия
forced queue-pressure eviction. Не возвращаться к ним без нового запроса или
новых данных; это не меняет факты о неполученном DLC и непроверенных runtime
сценариях.

Отложено по текущему запросу пользователя: задачи прежних пунктов1 (FPS),2
(ввод/диалог) и4 (save/load, переходы, звук/soak). Возобновлять их только после
завершения DLC и lifetime аудита. Текущий пересобранный GPU `c2c9ae72` сохранять;
предыдущий texture-fix checkpoint `90d8ef1a` не удалён.

Причины уже установлены: выключенный Resolve readback давал пустой CPU atlas;
неверный dword для texture exp_adjust уменьшал цвет65536 раз. Не начинать
диагностику этих причин заново. Подробная историческая постановка ниже остаётся
для evidence/acceptance; приоритет задаёт эта текущая очередь.


План уточнён 2026-10-22 по [runtime evidence](../logs/acv-runtime-triage-20261022/findings.json),
[проверке сборки](../logs/acv-test-build-20261022/verification.json) и
[аудиту пользовательских ссылок](../logs/acv-user-references-20261022/assessment.json).
Предыдущая версия раздела целиком сохранена в
[копии PLAN](../logs/acv-critical-plan-20261022/PLAN.before.md),
[SHA-256](../logs/acv-critical-plan-20261022/manifest.json).
Планирование завершено; перечисленные ниже исправления ещё требуют выполнения.

### 1. P0 — воспроизводимый сценарий и граница потери текстуры

**Принято 2026-10-24:** пользователь подтвердил улучшение рендера текстур
после исправления Vulkan `exp_adjust` (dword4→dword3).
[Checkpoint](NATIVE_REXGLUE_LINUX.md#texture-rendering-fix-confirmed-by-user-2026-10-24).
Эту причину считать установленной; fixed plugin90d8ef1a… сохранять при сборках.

**Исторический приоритет пользователя 2026-10-23:** исправить чёрные модели
gameplay при рабочем full readback. Пользователь подтвердил исправление
2026-10-24; этот defect закрыт. Остаточная проверка миссии относится к
регрессии прочих материалов/эффектов, а не к повторному поиску причины чёрных
текстур мехов.


Сохранить текущий рабочий ELF и его manifest; использовать штатный config,
одну миссию, профиль сохранения, одинаковый ввод и явно записанное состояние
shader cache. Перед тестами с записью сохранений делать копию профиля.
Для каждого сравнения фиксировать ELF/SDK/plugin/config hashes, overrides,
сцену, последовательность ввода и события draw/resolve. Номер кадра или секунды
после запуска сами по себе не доказывают одинаковое состояние сцены.

Первый эксперимент: ограниченная opt-in диагностика CPU physical bytes перед
передачей текстуры и GPU mirror после соответствующего upload. Связать данные
через fetch descriptor, physical range, размер, generation ресурса и порядок
submission/completion. Не сравнивать произвольный поздний CPU dump со старым RDC;
учесть конкурентные записи и доказать момент снимка. Сохранять raw bytes,
хеши, первый отличающийся offset и ID события, ограничить объём журналирования.

Исходный кандидат: FBO frame77730 draw3790, texture0, BC3 tiled/endian8-in-16,
0x1F6B3000, 524288 bytes. Адрес действителен для этого capture; при новом запуске
найти ресурс по фактическому fetch, не предполагать постоянство адреса.
GPU mirror содержит одинаковые transparent-black блоки.
В run20261022T160513877989Z уже сняты CPU-origin staging bytes после memcpy:
submission2889,524288 bytes; они совпали с GPU mirror frame58287/event13126
(SHAe4428980…). [Сравнение](../logs/acv-gpu-sync-20261022/cpu-gpu-comparison.json).
Сам copy event не попал в RDC. В FSI frame58287 найден draw5940 с тем же
fetch и реально объявленным shader slot0; texture RGBA0, post-VS конечен.
Writer trace привёл к DLTextureConverter_Xbox360Dx.cpp:0x82C83DE8 →
0x82C83100 →0x830D54A8. Выход конвертера совпадает с CPU upload;
входной staging после conversion тоже пуст. Before-copy run20261022T165910395240Z
подтвердил тот же пустой исходный буфер0xFE111000 (physical0x1E112000).
Writer исходного physical-буфера — copy результата DLTextureCompressor.
Парный before/after probe0x82C82090/0x82C82130 подтвердил2MiB нулей уже до
compression и их сохранность. Дальнейший A/B подтвердил причину для этого ресурса: при выключенном readback
CPU RGBA source0xFE191000 остаётся нулевым, при full содержит2066418 ненулевых
bytes; native/resource headers совпадают. ACV profile теперь включает
--readback_resolve=full. Frame34282/event75 уже семплирует непустую BC3;
GPU bytes равны same-run CPU upload. [Принятый fix](../logs/acv-readback-origin-20261022/accepted-fix.json).

Пользователь подтвердил исправление чёрных текстур мехов; повторная задача
scene/material regression снята по его просьбе. Следующий активный шаг — измерить
FPS/frame-time и p95/p99 с этим профилем. Не возвращаться к гипотезе пустого
входа: для запечённой текстуры причина уже установлена и исправлена.
Точный Resolve ID можно уточнить для оптимизации; потеря CPU-данных уже доказана
переключением режима и последующим GPU sampling. Адресная async-оптимизация: отслеживать CPU-consumed dirty Resolve ranges,
allocation/GPU/CPU-write generation, ring staging и completion ticket; сохранять
семантику немедленного CPU consumer. [Проверенный host reference](../tools/native-rexglue/async_readback_reference.hpp)
не заменяет full в игре. Slot completion/reuse, invalidate guards и64KiB staging
уже интегрированы, ASan/UBSan и real material byte regression прошли. VFS import
correlation и allocation/free-request tracing подключены;177 reads/≈32MiB сверены
с файлами. [Milestone](NATIVE_REXGLUE_LINUX.md#integrated-resolve-completion-and-resource-tracing-2026-10-23).
CPU consumer уже связан с Resolve:0x0FCA7000/submission1298 → first copy
0x82C83918,2MiB побайтно равны; затем tiled→linear→RGBA0x1E192000→compressor.
[Chain proof](../logs/acv-resolve-consumer-20261023/complete-proof.json).
Следующий шаг — отложенная публикация до guest fence/CPU first-read boundary;
проверить PM4 EVENT_WRITE_SHD/read-pointer/interrupt ordering, lifetime и отсутствие
ожидания под memory/GPU locks. После этого реализовать pending-readback publication по
allocation/GPU/CPU-write generation, сравнить с full oracle и измерить
bytes/readback, время ожидания и p95/p99 кадра до переключения default.
Full readback может быть дорогим;
оптимизировать после функциональной проверки, без production hardcoded addresses.

**Развилка:** CPU/GPU различаются → upload, dirty pages, memory watches,
cache invalidation и видимость памяти; одинаково пусты → источник материала,
распаковка/заполнение либо GPU producer/readback, если доказан GPU writer;
оба содержат корректные данные → texture decode/descriptors, shader inputs.
Если данные пусты по замыслу, выбрать другой значимый opaque draw.
**Приёмка этапа:** связанная пара CPU/GPU dumps одного ресурса и поколения,
доказанный порядок доступа и конкретная граница первого расхождения либо
подтверждение равенства. Не требовать ненулевых данных от каждого материала.

### 2. P0 — physical allocation и источник ресурса

Caller trace уже установил sites0x82AEF00C/0x82AEF028: возвращённые0
попадают в поля+0x54/+0x5C блока параметров, передаваемого в0x82AF1D38.
[Raw PPC и trace](../logs/acv-gpu-sync-20261022/allocation/callers.json).
Далее установить downstream обработку, назначение полей и связь с ресурсом.
Новый override run20261022T161947900981Z дал ненулевые allocations и убрал
защиту из diagnostics, но захваты1/59 draws не совпали с gameplay baseline2661.
Это не подтверждает ни shading fix, ни регрессию; shim остаётся opt-in.

Для двух неуспешных MmAllocatePhysicalMemoryEx запросов (0x40000/0x800000,
protect0x20000400) установить caller, возвращённый результат, реакцию guest,
потребителей и связь с диагностируемой текстурой/буфером. Сверить семантику
флагов с проверенным источником; исчезновение сообщения после добавления
READWRITE не доказывает правильность исправления. Если эти allocations питают
проблемный ресурс, исправлять их раньше дальнейшей shader-диагностики.

**Приёмка:** подтверждённый контракт флагов/ошибок, корректное выделение,
доступ и освобождение; проверка допустимых и недопустимых сочетаний. Отдельно
зафиксировать влияние на тот же ресурс/сцену. До этого shim остаётся opt-in.

Для пустого CPU-ресурса проследить только фактические reads через BHD/BDT →
DCX/TPF → guest destination; проверить offsets, sizes, endian и tiled layout.
DSEventScriptTools уже совпал на одном DCX-образце; это не проверка всех данных.
PC TPF/BHD parsers из предложенных репозиториев напрямую не подставлять.
Отсутствующие system:/dlc_* классифицировать по caller и fallback: обязательный
ресурс исправлять по фактическому пути, необязательный запрос документировать.
**Приёмка:** байты конкретного запрошенного ресурса прослежены до потребителя;
нет фиктивных mounts и пустых файлов, маскирующих ошибку.

### 3. P0 — исправление подтверждённого дефекта рендеринга

Уже исправлен подтверждённый дефект SDK `kComputeWrite`: SHADER_READ заменён
на SHADER_WRITE в VulkanSharedMemory::GetUsageMasks. Host regression проходит
и отвергает старую реализацию; отдельный плагин собран и проверен в85/110 s
запусках. Последний содержит2661 draws/64 dispatches, но источник кадра тёмный.
Исправление контракта не закрывает shading-проблему. Исправленный plugin включён в тестовый пакет с manifest;
[evidence](../logs/acv-gpu-sync-20261022/verification.json).

Выбрать ветку по результату этапа1. Для upload/cache проверить диапазоны,
повторное использование адресов, invalidation, ownership и синхронизацию.
Для shader-ветки проверить texture samples, lighting constants, finite VS/PS
values, форматы и bindings; затем выход draw → EDRAM → resolve → present.
Использовать Vulkan skills и релевантные pinned references; PPC producers
исследовать через read-only Ghidra/raw instructions с проверкой ABI.

Вносить минимальное исправление в handwritten host/SDK или проверенный hook,
с отдельным targeted test на нарушенный контракт. Generated C++ не править
как первичный источник. Один кандидат на сравнительный прогон; gamma/readback
и invalid-fetch toggles не объединять в пакет предполагаемых исправлений.

**Приёмка:** исправлена установленная граница, эффект повторяется в той же
сцене минимум в двух запусках; проверены несколько opaque draws/материалов,
HUD и конечный вывод. Использовать RDC, raw bytes, числовой replay. Рост средней
яркости недостаточен для утверждения правильного shading; без retail/oracle
подтверждения явно сохранять предел проверки. FBO использовать как дополнительный
диагностический путь: его отдельная потеря front buffer не равна причине FSI.
Отсутствие validation messages при muted RenderDoc не считать чистой проверкой.

### 4. P1 — Setup, текст и фактическое управление

UTF-8 BOM в конфиге уже исправлен и проверен. Отдельный SDK дефект
XamShowKeyboardUI теперь ограничивает UTF-8 буфер по максимальной длине
UTF-16, завершает строки по NUL и декодирует результат с ограничением guest
capacity; нулевая и переполненная длины отклоняются. SDK runtime пересобран,
non-headless smoke дошёл до вызова для ARTEM и завершился без падения.
Проверки helper покрывают Unicode/границы/ошибочный ввод под ASan/UBSan.
Остаётся проверить фактический ввод ASCII/Cyrillic/multibyte и Cancel через UI;
этот smoke ввод в поле не подтверждает. Headless=true остаётся штатной настройкой.
Испорченные tutorial glyphs проследить по text/font path, не считать BOM-fix
их решением.

Проверить реальные keyboard/mouse → SDL → XInput → guest, включая F8/Escape,
движение, камеру и действия в миссии. Внутренний автоввод не заменяет этот тест.
**Приёмка:** нет исключений/выхода за buffer в строковых тестах, корректные
результаты ввода/отмены, пользователь может пройти выбранный сценарий управления.

### 5. P1 — устойчивость и сборка для теста

После принятого исправления выполнить incremental build, сохранить PCH/objects,
обновить manifest и проверить host revision в runtime log. Затем startup smoke,
меню → миссия → выход, save/load на копии профиля, звук и ограниченный сеанс
20–30 минут. Записать frame-time, рост памяти и ошибки; закончить процесс штатно.
Сборку и тесты привязывать к одному ELF hash. Текущий smoke12 s подтверждает
только старт: child0 при15 прежних VFS/allocation errors не является clean pass.

**Приёмка:** нет новых crash/device-lost/fault loops и блокирующих unresolved
calls; проверены выбранные переходы, управление, сохранение/загрузка и звук;
каждая оставшаяся ошибка классифицирована с evidence. Передать тестовый ELF,
launcher, manifest, перечень принятых fixes и открытых ограничений. Длительность
20–30 минут не является доказательством отсутствия всех утечек.

### 6. P2 — отложенное до устранения критичных сбоев

GapFill/codegen speed: сначала полное сравнение generated output и timing,
до него сохранять проверенный CLI и incremental objects. Дополнительные stubs,
thunk pairs0x824FEDC8/0x829C6A98 и rejected discovery candidates исследовать
только при runtime hit. Широкая RE, Windows/Wine и Unity не входят в критический
путь Linux. Статусы старых SQLite tasks сверять с evidence, а не принимать
историческое «running» за наличие активного процесса.

Для каждой итерации: гипотеза → один различающий эксперимент → проверка
контракта → минимальный patch → повтор сценария → evidence/PLAN/PROGRESS.
Если эксперимент не различает причины, уточнить измерение перед новым запуском;
не повторять прежние gamma/readback опыты без новых данных.

## Инструменты, история выполнения и рабочий цикл

- Ghidra RPC: `./scripts/ghidra-rpc.sh status`; daemon был запущен headless,
  существующий /default.xex открыт через load --no-analyze в ACV_RETRY.gpr.
  Перед новой работой снова проверить статус. Metadata: PowerPC/BE, pointers32,
  67 314 definitions. У 0x82BB6BF8 body всего 8 байт; listing с 0x82BB6C00
  перескакивает на 0x82BB6C08. Это неполная база; использовать raw PPC/.pdata.
  Ghidra MCP в последней сессии не экспонирован; RPC подтверждён. Не выдавать
  analysis_complete или decompiler guesses за полное восстановление.
- Прочитать применимый [360tools skill](../tools/360tools-skill/SKILL.md) при
  ISO/STFS/ReXGlue/config/stubs; проверять реальные CLI arguments. Ghidra RPC
  skill: ~/.agents/skills/ghidra-rpc/SKILL.md. Vulkan skills:
  ~/.agents/skills/vulkan/SKILL.md (6 pinned Khronos references) и
  ~/.codex/skills/vulkan/SKILL.md (implementation/review; PLAYBOOK отсутствует).
  Vulkan installation evidence: logs/vulkan-skill-install.json.
- Xenia Edge — возможный oracle, а не доказательство по сходству. Перед запуском
  проверять доступность; новые трассы не оправдывают остановку чужой игры.
  REA doctor/providers допустимы при наличии; поддержку PPC/XEX не предполагать,
  setup/install с изменением agent configs требует отдельного разрешения.
- Все анализ/сборка/тест/исправления через
  `node tools/decomp-logging/cli.mjs run --stage STAGE -- COMMAND ...`.
  Default timeout 120 s; для долгих операций задавать явный лимит.
  Перед retry смотреть errors; использовать --retry-of EVENT_ID --action TEXT.
  verify OPERATION_ID требует фактической проверки, task хранит work item.
  Сохранённый прямой MCP result — через ingest. Существующие общие scripts
  уже входят в logger. Политика: [TASK-SQL](../TASK-SQL.MD),
  [SQLITE_LOGGING](SQLITE_LOGGING.md).
- SQLite: logs/decomp.sqlite3 (операции/tasks/index), errors.sqlite3 (ошибки),
  output.sqlite3 (compressed streams), artifacts.sqlite3 (checksums/archive).
  Cross-file FK/atomicity не гарантируются. Невключённые в logger действия не
  наблюдаются автоматически; emergency JSONL и незавершённые streams учитывать.
  Cleanup — только архивированные checksum-verified unreferenced duplicates,
  с восстановлением; source media и уникальные доказательства не удалять.
- Кандидат → оригинальные инструкции/границы/call graph → dependency/field
  provenance → частичная C++ модель → соответствующий static/host/oracle check →
  evidence/confidence/blocker → следующий независимый кандидат. Stop candidate
  при отсутствии bytes/boundaries/dependency/oracle или protected-DB blocker.

## Отложенные исследования и открытые вопросы

Эти ветки не блокируют упорядочивание документации; их архивирование не закрывает
неизвестное. Короткие отчёты и полные источники: [fine/README](fine/README.md).

- **Timing:** step≈min(D/F,0.1) (integer F/60 и float order — в ACV_TIMING), перенос main→draw с двойной буферизацией и
  UI consumers прослежены по инструкциям/трассам; полная связь с физикой/боем
  не установлена. P+0x974, getter 0x82436FE0, consumer 0x82460848 и helper
  0x8245FA20 — [ACV_TIMING](ACV_TIMING.md). Старое view/surface толкование +0x30/+0x34
  снято; Unity timer push/pop и gameplay wiring не переносить без evidence.
- **FileManager.BindRead:** startup true; lookup bool/default после ${...};
  контейнер около object+0x104 глобала 0x838BB018; 14 direct setters.
  Parser 0x823E09B0 → 0x823E0600 → 0x82C26300, call 0x823E07DC строит
  Assembly.%s (0x8201EC0C), поэтому не пишет точный FileManager.BindRead.
  0x825A6800 читает +0xE4; unique pointer 0x82232848 и writer candidate
  0x825A6BB0 не доказывают alias/ownership. В BDT две comment-like строки
  #FileManager.BindRead=false и два BindReadCache prefixes; связь с parser
  не установлена. Другие indirect setter paths не исключены. Evidence:
  [lookup follow-up](../logs/acv-lookup-20261020/followup-20261021.md).
- **Размеры:** 0x83885E80+0x910/+0x914=1280/720; 0x82AF1D38 передаёт их как
  x1/y1 прямоугольников в 0x82AF5F50. Screen/camera смысл не доказан.
- **Bounded C++:** 0x8245FA20 (48 instructions, без .pdata owner) сравнен с
  generated leaf; 0x82303138 — 10 080 сравнений со stub 0x82302520.
  0x830D68F0/0x830D69D0 — 80 784 finite-domain split comparisons, sin/cos-like;
  fused/split различаются. 0x830D6AB0 tan-like candidate без .pdata;
  200 000 host cases, error 5.993e-13 при недоказанном RN-even — не Xenon parity.
- **Vector chain:** 0x82302520 пишет f1/f2/f3 в +64/+68/+72 и r8/r9 ratio
  в +192; .pdata wrapper 0x82300948 передаёт их в 0x82BC8980, результат —
  64-byte block +208. 0x82301958 читает четыре VMX128 слота; 13 direct callers
  инвентаризированы. Второй consumer caller 0x82302918; цепочка
  0x8275E4A0 → 0x82302520 → 0x82B0CF80 → 0x82C6F668 читает +512..+592,
  но последняя граница/семантика неизвестны. Provenance +336..+384/+480..+508,
  indirect owners 0x8278CE68 (+560) / 0x82570F10 и rounding oracle открыты.
  Offset scans 5111/219 кандидатов не подтверждают alias; 0x82B0F920,
  0x825F0E68, 0x82994B98, 0x82C27A40, 0x823D23C0 остаются несвязанными.
- **Альтернативный host:** XenonRecomp commit ddd128bcca99fe8bfbb99bea583c972351fa6ace;
  switch 0x82EF87B0 исправлен, 24 frsqrte sites требуют oracle. Memory/dispatch
  41 checks и другие clock probes не являются полным host. Windows/Wine
  evidence историческое, реальный Windows/parity открыт. См. [L3](fine/NATIVE_HOST_L3.md),
  [XEX inventory](fine/NATIVE_XEX_RUNTIME.md), [coverage](fine/XENON_MORE_INSTRUCTIONS.md).
- **Unity:** ветка отложена; 9 прежних Editor numerical cases — отдельная
  проверка от batch EditMode AcvTimingTests: подтверждены 13 passed,0 failed,
  0 skipped (logs/timing-followup-20261019/unity-tests.xml). Это не закрывает
  прежние GUI Package Manager errors и не доказывает gameplay parity. Не делать gameplay выводов из
  таймеров, file-management или resource loading.

## Критерии приёмки

1. Linux compile/link + воспроизводимый запуск подтверждены логами; нет
   достигнутых unresolved calls/fault loops, блокирующих выбранный сценарий.
2. Проверена именно игровая сцена с корректным shading/resolve/present,
   управление и существенные переходы; process alive/exit0/0 REX_FATAL недостаточно.
3. Каждое принятое исправление связано с retail VA или SDK source, областью
   проверки и доказательствами; гипотезы и ограничения остаются явными.
4. Результаты/ошибки в SQLite и evidence files; выполненные задачи отделены
   от открытых. Все изменённые системные настройки возвращены либо явно переданы
   пользователю; этой консолидацией системные настройки не менялись.
5. Нет обещаний процента полной декомпиляции по количеству generated files.
   Готовность Linux gameplay, semantic recovery и альтернативных host веток
   оценивать раздельно и только по фактически выполненным критериям.

## Архив и восстановление полного контекста

Здесь оставлен актуальный рабочий контекст; все исходные формулировки,
промежуточные числа, commands, hyperlinks и противоречивые исторические статусы
сохранены локально без потерь в `docs/fine/planning-originals-20261022.tar.gz`;
в GitHub опубликованы их Markdown-резюме и
[manifest](fine/planning-merge.json) с SHA-256 и исходными заголовками/ссылками.

```bash
tar -xOf docs/fine/planning-originals-20261022.tar.gz docs/PLAN_B.md
```

Это чтение полной записи без её возврата в активный набор документов.
Обновлять текущий статус и следующие действия только здесь, execution history —
в PROGRESS/SQLite, подробные доказательства — в профильных технических отчётах.
