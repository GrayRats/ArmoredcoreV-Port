# ACV timing — актуальные выводы и доказательства

Сводка на 2026-10-22. **[И]** инструкции/bytes; **[Т]** Xenia trace;
**[М]** guest memory; **[H]** ограниченный host test; **[Г]** гипотеза.
Это не спецификация Xenon и не доказательство влияния на бой/физику.
Полный прежний текст: [архив](../logs/docs-context-20261022/originals.tar.gz),
запись `docs/ACV_TIMING.md`; [manifest и индекс ссылок](../logs/docs-context-20261022/manifest.json).

## Формула и ожидание

**[И]** `0x82AEF208` (148 bytes) вычисляет elapsed D через наблюдаемый uint32 F;
caller ограничивает только сверху, затем масштабирует:

```text
N = floor(F / 60)               // integer division
P = float(float(D) * 100) / float(N)
P = min(P, 600)
step = float(float(P * 0.01) * float(1 / 60))
```

Практическая модель — `min(D/F, 0.1)` seconds. Это приближение, а не точное
тождество: N целочисленный, float rounding сохраняется. Отрицательные D
не ограничиваются снизу. Guard F<60 в C# utility — защита инструмента,
а не установленное поведение retail. NaN/overflow/rounding parity отдельно не приняты.
**[H]** 4 000 000 сравнений C# арифметики: max difference ≤1.34e-7,
нет >1e-6; это не исполнение PPC и не доказательство фиксированного FPS.

**[И]** Call path: entry `0x82B92114 → 0x82AEEF58`; повторный
`0x82AEF0AC → 0x82AF1BF0`; `0x82AEF0B4 → 0x82AEF208`.
`0x82B8F9C0` читает TBLr/mftb, сохраняет 64-bit value, возвращает 1.
`0x822F4E88 → 0x82B8FA70 → KeQueryPerformanceFrequency` инициализирует
low32 frequency global `0x83886944`, если тот нулевой.
`0x82AF1BF0` (328 bytes) вызывает предварительные routines, затем ждёт
при normalized elapsed <90, расчёт ожидания использует 95.
`0x82C27BE8` отбрасывает nonpositive input, передаёт `(input+999)/1000`;
`0x830F3288 → 0x82B93668` формирует negative relative interval из input*10000
и вызывает KeDelayExecutionThread. Sleep precision/retail frequency не установлены.

**[Т]** `0x82AEF26C`: r7=r8=0x000CB735 (=N), значит F в данном Xenia run
в [50 276 460, 50 276 519]. Это частота, видимая игрой в эмуляторе,
не hardware Xenon specification. D/f1 логгер r3–r8 не печатает.
Темп и запись шага наблюдались примерно раз на кадр, не доказан physics rate.

Evidence: [timing-index](../decompilation/evidence/acv/timing-index.json),
[follow-up index](../decompilation/evidence/acv/timing-followup-index.json),
`logs/acv-stdout.log`, `logs/acv-consumers-stdout.log`, `tmp/acv-timing-check`.

## Межпоточная передача: два P, не три

**[И,Т]** Singleton getter `0x82AEEF40` связан с global `0x8372BF80`,
объектом `0x8372BF88`. `0x82AEFC58` задаёт selector +0x38=0;
`0x82AF1D38` заполняет +0x30/+0x34 двумя результатами `0x82AF5F50`.
`0x82AF12F0` XOR selector с 1, выбирает pointer по +0x30+4*selector,
сохраняет в +0x28, пишет float +0x974, word +0x980=1 и +0x984=0.
При известной инициализации первым выбирается +0x34. Другие последующие
writers selector полностью не исключены. Stack temporary `0x82AF2F70` /
`0x82AF1B60` — другой объект; его selector не описывает singleton.

**[Т]** `Main XThread`, guest 01000028, tid6: `0x82AF1BF0` /
`0x82AF12F0`. `DrawThread`, guest F80013FC, tid0x18: исполнитель
`0x822D0E40`, callback return LR=0x822D0F28. Переданные P чередуются
`0x40586F00` / `0x405850D0` и совпадают с контекстами callbacks.
Это подтверждает handoff через те же два объекта; более ранняя гипотеза
«три буфера» заменена. Возможное кольцо `0x82AF5410` не равно этому handoff.

**[М]** P+8=1280, P+12=720. Первый замер P+0x974≈0.0333 s;
позже по 12 выборок/0.563 s: первый P 0.016492827–0.016702265,
второй 0.016626088–0.017455928 s. Это разные окна, не fixed 30/60 FPS.
Чтения асинхронны и не атомарны. Историческое отображение Xenia:
host address = 0x100000000 + guest VA; сверять с maps конкретного процесса.
Не путать с virtual base ReXGlue или physical mappings.

**[М,И]** Слот `0x8202411C` прочитан 12 раз: bytes 82 5d 41 00 →
`0x825D4100`. Он берёт target из record+8, r3 из record+4,
переносит r5→r4 и делает mtctr/bctr. Сам слот подтверждён, новый отдельный
trace indirect call через него не снимался. Callback `0x82AEF6C8` —
`mr r3,r4; b 0x822B51F0`, а не прыжок в середину 0x822B5190;
`0x82AEF6D0` вызывает `0x822E5DE8`, `0x822E52D0`, `0x822B5B90`
с сохранённым входным r4. Их прямое чтение +0x974 не найдено.

Evidence: [guest samples](../logs/timing-followup-20261019/guest-samples.json),
[summary](../logs/timing-followup-20261019/summary.json),
[maps](../logs/timing-followup-20261019/xenia-maps.txt),
[slot instructions](../logs/timing-followup-20261019/slot0.asm).

## Читатель шага и режимы S

**[И]** `0x824609E0` вызывает `0x82460848` на `0x82460A54`.
Если byte(S+0)==0, consumer выходит — прежнее обратное условие было ошибкой.
`0x824608C8 → 0x82436FE0`: P=*(context+0x10), f1=*(float*)(P+0x974).
Затем r3=S и `0x824608D0 → 0x8245FA20`; S и P — разные роли.
VMX128 предварительные условия не декодировать по generic PWR7 мнемоникам.

**[И]** `0x8245FA20` — 48 instructions / 192 bytes,
[0x8245FA20,0x8245FAE0), без .pdata owner; live bytes совпали с .text.

| Поле | Наблюдаемая операция | Предел смысла |
|---|---|---|
| byte +0 | Ноль → return; иначе обновление | Активность — описательное имя |
| byte +1 | Выбор режима | Не recovered enum |
| u16 +2 | Modes1/3 increment saturating 65535; mode4 handler тоже пишет | Не универсальный «счётчик периодов» |
| float +4 | Сравнение/однократное вычитание | Длительность/период — гипотеза |
| float +8 | +=f1; далее mode-dependent изменения | Временной накопитель — гипотеза |

Modes2/4: при достижении +4 сбрасывается +0. Modes1/3: однократно
вычитается +4 и увеличивается +2; повторного цикла вычитания нет.
Другие режимы оставляют прибавление; NaN не проходит обычную less-ветку.
В consumer mode100 обнуляет +8; ветка101/102 восстанавливает +0.
Mode4 handler `0x82460788`, store `0x82460814`: floor((+8)/(+4)*N),
clamp 0..N, low16→+2; при N сбрасывает +0 на 0x82460824.
N/NaN/zero period отдельно динамически не проверены.

**[Т]** Consumer/getter/update path исполнялся на DrawThread F80013FC.
В завершённом run: LR 0x82460A58 — 15 635 входов; 0x82460850 и
0x824608D4 — по223; 222 пары совпали по context/frame marker.
Логгер не печатает PC: LR привязан к исходным call instructions и save helper
`0x830D4E9C`, а не принят за адрес текущей инструкции.

**[М]** 308 samples: S=0x40F738C4, mode4, +4=0.5;
+8 вырос 0.033505775→0.515492499, +0 сменился1→0.
S=0x40F73964/0x40F73B84: 0.033333134→0.502060354 и тот же сброс;
по14 разных последовательных состояний. Асинхронные reads/последний trace marker
не являются точной покадровой memory trace. Поздние первые адреса могли
переиспользоваться; modes1/3 в этом memory run не подтверждены.

**[И,Г]** N — результат wcslen-like `0x830D5550` строки из virtual slot+108,
которую 0x824609E0 копирует в буфер на1023 символа. `0x824603A8` /
`0x82460470` через 16-bit copy helper0x830D60C0 выбирают символ по S+2;
`0x8245FB38` сравнивает 10,32,0x3000,48..57. Это поддерживает timed
wide-string/UI interpretation. Источник/назначение строки и влияние на бой
не установлены; прежняя «таблица состояний» не подтверждена.

Evidence: [consumer trace report](../logs/timing-followup-20261019/consumer-early-report.json),
[samples](../logs/timing-followup-20261019/consumer-early-samples.json),
[mode4 instructions](../logs/timing-followup-20261019/mode4-handler.asm),
[getter](../logs/timing-followup-20261019/getter.asm),
[update](../logs/timing-followup-20261019/8245FA20.asm).
Повторный отчёт читает сохранённые данные: `tools/acv_consumer_report.py TRACE SAMPLES`.

## Start/end timer, scopes и ownership

**[И]** `0x82AF1BF0` передаёт singleton+0x74 как r3 в
`0x822F4908` и `0x822F4ED0`. `0x822F4908 → 0x822F47E8`
сначала reset byte+8, qword+0x18/+0xA0, word+0x48/+0xB0;
затем пишет fresh counter в +0x10 и active+8=1. Это повторный start,
а не lazy init. End `0x822F4ED0` при active пишет now-start в +0x18
и сбрасывает +8, после drain loop. 220 code bytes / 224 with padding
проверены неизменными; Ghidra body оставался8 bytes.

**[И]** Drain `0x822F4950`: count=word(obj+0x48),
S=obj+0x20+(count-1)*0x28, id=S+0; R=obj+id*0x70.
Если R+0x50!=0 либо id=0, закрывает активный S timer, прибавляет elapsed
к R+0xA0, увеличивает R+0xB0; signed max R+0xA8 считается от running total,
не от одного elapsed. Затем decrement count. Callers0x822F4A48,
0x822F4BF0,0x822F4ED0; `0x822F5308` возвращает base+idx*0x28.

Push `0x822F4678 → 0x822F52C8` копирует5 qwords в +0x20+count*0x28,
increments count. Slot1 начинается на+0x48, поверх count; record0 пересекает
header. Это ограничивает уверенность layout/depth, не доказывает runtime nesting.
Scope `0x822F4AB8` и close0x822F4A48/0x822F4BF0 не найдены через direct
branch/address-construction/data-pointer patterns, кроме .pdata. Не объявлять
unused: другие addressing/indirect paths не исключены. 0x822F4BF0 обновляет
max per-id mean, capped at F*3/60 (около50ms только если F — ticks/s).
Назначение profiling/frame pacing полностью не установлено; push/pop не переносить.

`0x822F4A48` gate: singleton+0x58 сравнивает с результатом0x82B8F9E0
(`*(*(r13+0x100)+0x14C)`); thread identity — гипотеза.
`0x822F45A0` возвращает *0x83722018. `0x822F4CF0` создаёт
328-byte timer с inline slot+0x20 и count+0x48. `0x82AF2F70` строит
stack temporary внутри0x82AF1D38 и не является paced singleton.
Post-wait `0x82303DF0` обходит table0x83722168/count0x837221A8;
`0x82B49088` вызывает virtual slot+8 на input+0x84 с r4=-1.
Destination/смысл -1 и writers+0x84 не установлены.

.pdata bounds: 0x822F4950=248 bytes; 0x822F4ED0=220;
0x822F4BF0=256; 0x822F4A48=108; 0x82AEF208=148;
0x82AF1BF0=328; 0x82AF1B60=140; 0x82AF2F70=132;
0x82AF12F0=240; 0x826BC7A0=128. Не путать unwind records с call tables.
Evidence: [follow-up index](../decompilation/evidence/acv/timing-followup-index.json),
[integrity4950](../logs/acv-822F4950-integrity.json),
[integrity4ED0](../logs/acv-822F4ED0-listing-integrity.json).

## Rectangles и FileManager.BindRead: отдельная цепочка

**[И]** `0x82AF5F50` создаёт3008-byte object, ctor0x82AF7160,
rectangle differences/ratio →0x822B5A18. Startup1280/720 через
`0x82AF60C0` записывает0x83885E80+0x910/+0x914; они становятся x1/y1
двух rectangles. Lookup result не даёт этот pointer: r27 перезаписан
0x83885E80 в0x82AF23A0. View/surface/camera semantic labels сняты.
0x822B5A18 хранит incoming f1 в outer+0x4C0 без flag gate;
r6 pointer сохраняет в+0x370 и условно читает+0x77C →+0x374.

**[И]** Ключ FileManager.BindRead: literal0x820E96BC, helper0x822B2320
строит temporary wide-string. `0x830D5550` считает16-bit units до NUL.
`0x82AF1E88 →0x82C25350(default=0)` возвращает bool/default:
search0x82C25210/0x82C24F38, text expansion ${...} через0x82C24EB8,
0x82C27148/0x82C26270; true только точные true/TRUE/yes/YES.
Неполные markers/missing keys/recursion limit не проверены.
Node links+0/+8, key+0x0C, marker+0x4D не доказывают named tree type.
Container path0x82C237B0→0x82C24398→global0x838BB018;
vtable0x8210FE70 slot+0x10→0x82C22D20; aligned embedded region
около object+0x104, sentinel init0x82C26458→0x82C24FB8.

Startup setter0x82AFA5A4 регистрирует true; 14 direct setters просмотрены.
Dynamic site0x823E07DC строит Assembly.%s (literal0x8201EC0C), поэтому
не пишет точный FileManager.BindRead. Reader0x825A6800(+0xE4),
unique pointer0x82232848 и writer0x825A6BB0 через0x82586130 остаются
без доказанного alias/dispatch. В BDT две #FileManager.BindRead=false
строки и два BindReadCache prefixes; # semantics/asset-parser связь неизвестны.
Другие indirect setters не исключены. Evidence:
[lookup summary](../logs/acv-lookup-20261020/summary.md),
[latest follow-up](../logs/acv-lookup-20261020/followup-20261021.md),
[ISO hits](../logs/acv-lookup-20261020/iso-key-hits-20261021.json).

## Проверки, исправленные ошибки и границы

- [H] Unity Editor: 9 synthetic cases. Затем отдельный Unity6000.6.6f1 batch
  EditMode AcvTimingTests: **13 passed,0 failed,0 skipped**, exit0:
  [XML](../logs/timing-followup-20261019/unity-tests.xml),
  [log](../logs/timing-followup-20261019/unity-tests.log).
  Нет compile/ScriptableSingleton errors именно в этом batch; прежние GUI
  ошибки этим не закрыты. Utility не подключена к gameplay; Unity отложена.
- [H] 0x8245FA20 reconstruction:69 152 state checks+10 transitions;
  scalar0x830D68F0/0x830D69D0:80 784 finite split comparisons,
  fused differs3825/3772; это не Xenon oracle. [Archive](fine/README.md).
- Ghidra save-helper noreturn/ABI guesses могут обрезать тела. Старые
  bounded repairs0x82AEF208/0x82AF1BF0 и helpers сохранены в истории,
  bytes проверены; не повторять изменения живой базы. Metadata base0
  не заменяет guest image0x82000000 / .text0x822B0000. analysis_complete
  не означает достоверный pseudocode. Generic PWR7 VMX names не oracle.
- Старый disassembler пропускал неизвестные words; исправленный ppc_dis.py
  печатает .long на каждый4-byte word. Не использовать старые shifted listings.
- Позднее pointer sampling, переставленные zero-active условия, slot=50%
  hypothesis, три вместо двух P и lazy-init interpretation заменены выше.
- ptrace_scope после разрешённого sampling был возвращён1, Xenia config
  восстановлен byte-for-byte: [cleanup](../logs/timing-followup-20261019/cleanup-and-hashes.json).
  При более поздней установке наблюдалось0: [observation](../logs/xenonrecomp-install-20261020/ptrace-observed.txt).
  Это разные даты; текущее sysctl здесь не утверждается. Новых изменений нет.
- REA6.7.0 doctor/providers: healthy=false, Ghidra provider имеется;
  штатный PowerPC profile рассчитан на BE ELF32 ET_EXEC, XEX/Xenon support
  не доказан. setup/engine install/ACV recovery через REA не выполнялись.

Открыты gameplay связь, VMX conditions, indirect scope callers, post-wait target,
проверка прочих modes/runtime edge cases и доверенный rounding oracle.
Очередь работ — [PLAN](PLAN.md); новые доказательства заменяют соответствующую
строку/раздел здесь, а не создают ещё один хронологический пересказ.
