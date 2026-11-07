# Исходные показатели ACV — 2026-10-29

Статус: историческое свидетельство просадки, **не контролируемый baseline миссии**.
Миссия/действие пока не указаны. Новую игру в этой сессии не запускали.

Источник: пользовательский запуск `20261028T183521948114Z`, сохранённые
[данные и SHA-256](../logs/acv-fps-audit-20261029/evidence.json),
[runtime.log](../logs/acv-fps-audit-20261029/runtime.log),
[эффективная команда](../logs/acv-fps-audit-20261029/launch.json).
Последние 8 непересекающихся окон, 2400 интервалов, суммарно107.967601с;
концы окон23:40:49.374–23:42:24.174 по часам лога, более90с после старта.
Окно выбрано как поздний участок записи, а не как подтверждённая одна сцена.

| Метрика | Значение |
|---|---:|
| Частота обработки guest swap |22.2289/с|
| Средний интервал |44.9865мс|
| Диапазон средних FPS отдельных окон |20.8913–22.7835|
| Диапазон p99 отдельных окон |50.1380–54.9354мс|
| 1% low |Неизвестно: сырые интервалы не сохранены|
| GPU/CPU по потокам в этом окне |Не измерены|

FPS = число интервалов / сумма длительностей, а не среднее FPS окон.
p99 окна нельзя усреднять в общий p99 или выдавать1000/p99 за 1% low.
Счётчик SDK находится перед IssueSwap; не доказывает число уникальных
показанных кадров, скорость симуляции или отсутствие пропусков async pipelines.

Настройки: FSI/full, vsync=true, frame_limit_fps=60, guest1280×720,
resolution_scale=1, output2560×1080, FIFO, A2BGR10_UNORM10bit по runtime telemetry,
present_effect=fsr, swap_post_effect=fxaa, anisotropic_override=3,
cache1024/1536MiB/180s, read-ahead16MiB. Это запись существующих настроек;
тест эффектов FSR не выполнялся. Запуск вернул0, но содержит2ошибки:
Unsupported resolve vertex buffer format и Failed in backend (PM4_DRAW_INDX).
Поэтому этот запуск не является проверкой корректности рендера.

Текущие ELF/runtime/GPU/FFX release-1.0 сверены полным SHA-256 с принятой
RUNTIME_BASELINE; значения сохранены в evidence.json. Проверка текущих файлов
не доказывает прошлые хеши загруженных .so: launch.json фиксирует только ELF.

Следующий замер: та же миссия, сохранение, маршрут/камера, настройки и прогретый
кэш; начало после120с,60с наблюдения, затем минимум3повтора baseline и кандидата.
Нужны CPU/GPU одновременно и отдельные покадровые интервалы. Определение 1% low:
1000 / среднее самых медленных ceil(N×0.01) интервалов в миллисекундах.
Инструкции: [report-fps](../report-fps.md).


## Первый одновременный профиль пользовательской сессии

2026-10-29,session161152,PID11835,age183.87s;60s CPU/GPU/IP.
1500интервалов из5полных окон внутри sample timestamps:29.0203swap/s,
34.4587ms; max802.88ms,1%low неизвестен. GPU92.43%device-wide;
CPU GPU Commands68.30%,DrawThread44.71%,Main21.56% одного ядра.
60CPU/GPU строк,5665IP samples,lost0. В sampling warning/error нет.
Пользователь уточнил: первая миссия, gameplay, мелкие зависания; маршрут не задан.
Это ещё не воспроизводимый baseline маршрута.
Cache2048/3072MiB/600s отличается от исторических настроек; A/B ускорение не заявлено.
[Session/settings/hashes](../logs/fps-baseline-20261029-session161152-02/session.json),
[расчёт/границы](../logs/fps-baseline-20261029-session161152-02/analysis.json).
Artifact hashes относятся к файлам; maps/stat device mismatch сохранён явно.

## Повтор первой миссии, та же сессия

После уточнения пользователя на471.15с снят ещё60с профиль:
29.3226swap/s,34.1034ms по1500интервалам5полных окон; max49.10ms.
GPU93.65%device-wide,5791IPs/lost0; warning/error в окне нет;
физические disk reads0байт.802.88ms пауза первого замера не повторилась.
[Analysis](../logs/fps-baseline-20261029-session161152-03/analysis.json),
[аннотация](../logs/fps-baseline-20261029-session161152-03/user-annotation.json).
Тот же PID/settings, разные действия/камеры не контролировались; это не FPS A/B.
Отдельный raw-swap кандидат built/host-tested, runtime-тест ещё предстоит:
[manifest](../tmp/acv-swap-trace-20261029/build.json), [отчёт](../report-fps.md).

## Поздний live memcpy-профиль, тот же процесс

По запросу пользователя без перезапуска/настройки PID11835:
30s,1527CPU samples,509backward-copy hits,lost0;477pool Copy/Worker,32ReadPhysicalCpu.
Два полных300swap окна24.7522/s,40.4004ms; GPU95.73%device-wide.
Поздний маршрут не подтверждён, не считать падение с29/s регрессией/A-B.
Следующий15s register sample:317/317disjoint,page-aligned source/destination.
Новый forward128 код в этом процессе не загружался.
[Live evidence](../logs/fps-live-copy-20261029-01/analysis.json),
[геометрия копий](../logs/fps-live-copy-20261029-02/geometry.json),
[кандидат и ограничения](kb:FPS_FORWARD_COPY_20261029).

## Forward128 runtime и GPU boundary

Candidate6090b52a forward1,session171732:240s child0/fatal0/runtime_errors0.
Raw120–180s1072intervals17.8587swap/s,55.9953ms,1%low14.3942,max83.3753ms.
GPU97.77%,60CPU/GPU rows,4439IPs/lost0. Control0 session172327 exited90.45s,
до120s gate; валидного A/B нет. GPU-written cached coherent staging test Astra6
прошёл bytes/guards, но copy medians Forward128≈3%медленнее libc.
Не принимать RAM speedup за GPU/FPS ускорение.
[Raw summary](../logs/fps-forward-test-20261029/on-01/swap-summary.json),
[контроль](../logs/fps-forward-test-20261029/comparison-status.json),
[GPU benchmark](../logs/fps-forward-gpu-20261029/result.json),
[границы](kb:FPS_FORWARD_COPY_20261029).
