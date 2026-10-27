# Отдельный Linux-пакет ACV — 2026-10-24

[English](LAUNCHER_PACKAGE.md)

Локальный результат: `../../ACV-Linux/`, точка входа `start.sh`.
Это статически перекомпилированная игра с ReXGlue, а не полностью восстановленный
исходный код ACV. Пакет содержит собственные копии игровых данных, Qt/ICU,
платформенные плагины Qt, host ELF, runtime, GPU и FidelityFX. Оригиналы не менялись.
Linux/glibc, Python 3, X11/XWayland, Vulkan-драйвер и остальные системные библиотеки
требуются от ОС; их список находится в manifest. Другие дистрибутивы не проверены.

## Сборка и пути

[standalone.py](../tools/acv-launcher/standalone.py) принимает `--out`,
`--runtime-dir`, `--include-game-data`. Существующий output не удаляется.
Файлы копируются в соседний staging, проверяются хеши, зависимости и CLI,
после чего staging переименовывается в output. Runtime, отличный от принятой
[RUNTIME_BASELINE](../tools/native-rexglue/RUNTIME_BASELINE.json), явно помечается
как кандидат; это не автоматическое принятие новой baseline.

[acv_paths.py](../tools/native-rexglue/acv_paths.py) распознаёт marker
`acv-package.json`: executable в `bin/game/`, данные в `data/game/`, профили в
`${XDG_DATA_HOME:-~/.local/share}/DEArmoredcore/acv/`, логи в
`${XDG_STATE_HOME:-~/.local/state}/DEArmoredcore/acv/logs/`.
`ACV_PROFILES_ROOT` позволяет выбрать отдельное хранилище. Лаунчер использует
те же пути. SDK RUNPATH больше не определяет выбор библиотек: запуск задаёт
каталог выбранного runtime первым в LD_LIBRARY_PATH. Конфиг ARTEM сохранён.

## Проверено

- [verification.json](../logs/acv-standalone-20261024/verification.json): 183 хеша в первой сборке,
  видимое окно Qt/X11, запуск game из cwd `/`, runtime/GPU/FidelityFX и Qt
  загружены из отдельной папки по фактическим process maps. Symlink на исходное
  дерево нет. Длительность game probe — 20 с; swapchain и pipelines созданы.
- Probe вернул 1 из-за известного отсутствующего `system.properties`;
  эту диагностику не скрывали. Других runtime errors в данном запуске нет.
  Сам runtime завершён штатным bounded probe. Это не gameplay acceptance.
- Уникальный XUID тестового профиля передан через окружение и отражён в launch
  report. Принадлежность реального сейва этому XUID пока не проверена.
- После запуска число shm orphan не увеличилось. До профилирования удалены
  17 уже не отображённых процессами xenia_memory-файлов (7 573 667 840 байт).
- Python: 15 settings, 9 profiles, 3 shm tests. Qt: ctest launcher прошёл.
  Новый regression проверяет сохранение flock дочерним процессом после закрытия
  descriptor родителем. Probe передаёт descriptor игре через pass_fds.
- [X11 startup samples](../logs/acv-native-linux/20261024T000238328506Z/samples.json):
  58 samples за 60,97 с при 0 orphan до/после, 25 139 380 224 free bytes до/после.
  Host main thread: ioctl 36/58; GPU Commands top frame libc address 41/58;
  DrawThread libc 31/58, sub_82BB58A8 9/58, __imp__sub_82AF5708 8/58.
  Это распределение sampled stacks, не распределение CPU time. GDB и присутствие
  RenderDoc в runtime искажают скорость; bottleneck и FPS из него не доказаны.

## Что не принято

Игровой save/load, восстановление настоящего сейва, изоляция двух игровых
профилей и XUID внутри сейва; визуальный CAS/FSR; темп/звук с лимитером;
адаптивные FOV/HUD, >60 FPS и визуальная точность 10-bit output. Патченный runtime находится в
отдельном пакете как кандидат, принятые артефакты development-сборки не заменены.
Текущая очередь и критерии: [PLAN_GUI](../PLAN_GUI.MD).

## Экран, цвет и подача кадров — 2026-10-24

[Матрица трёх запусков](../logs/acv-display-20261024/matrix.json) фиксирует
35s UltraWide/Bilinear, 35s 16:10/CAS/FIFO и 35s UltraWide/FSR/10-bit request.
Во всех трёх child exit0, без новых shm orphan; probe exit1 сохраняет известный
optional system.properties diagnostic. Изображения не снимались.

Исправлен SDK `GetConfiguredVideoModeWidth/Height`: host window больше не
подменяет гостевой видеорежим. Прежний `HasNonDefaultValue` считал явно заданные
1280/720 не переопределёнными и брал ширину/высоту host-окна. Actual-source
host test проверил defaults, независимость от host size, resolution preset и
явный guest mode. Изменения SDK воспроизводятся последовательным применением
прежних патчей и [launcher-display-output.patch](../tools/native-rexglue/patches/launcher-display-output.patch).
Предыдущие source bytes сохранены в logs/acv-display-20261024/source-before.

Подтверждённая геометрия:

| Host окно | Изображение | Отступ | Проверка |
|---|---|---|---|
| 1600×675 | 1200×675 | 200,0 | UltraWide, Bilinear |
| 1280×800 | 1280×720 | 0,40 | 16:10, CAS sharpen |
| 2560×1052 после resize | 1870×1052 | 345,0 | Bilinear/CAS/FSR, округление до пикселя |

Это вписывание исходных 16:9 с полосами. Расширенные FOV/HUD пока отсутствуют.
Host WM изменял размеры окна во время запусков; runtime отразил реальные
размеры и пересоздал swapchain с сохранением пропорций.

VSync On подтверждён как FIFO (2), Off как Immediate (0). Измеряются интервалы
обработки XE_SWAP, не скорость симуляции/звука и не физическая частота монитора.
Лимитер теперь хранит deadline на экземпляре CommandProcessor и сбрасывает
его при смене лимита. По окнам 300 samples: Bilinear 56,36–60,00 FPS;
CAS/FIFO 32,31–60,00; FSR 52,96–60,00. Просадки старта остаются, стабильные
60 gameplay не заявлены. 120/144/165 недоступны до guest timing validation.

CAS реально выбрал final effects 2/4; FSR — final effect7/RCAS, два passes.
Резкость RCAS теперь управляется отдельно (0…2 stops). Это подтверждение
исполнения shader path, не визуальная приёмка качества/HUD.

Выбор цвета использует фактическую surface. Здесь форматы44/50 и colorspace0:
BGRA8 UNORM/SRGB. 10-bit запрос дал явный fallback к8. Новый путь выбирает
только предложенный A2RGB10/A2BGR10 UNORM + SRGB_NONLINEAR, без HDR.
Запрос10 вместе с8-bit dithering отклоняется. 10-bit output precision ещё
не проверена; физическое кодирование HDMI/DP остаётся под управлением системы.
Исправлена также ошибочная ветка выбора произвольного формата при любом
единственном surface format; ошибку запроса форматов теперь нельзя принять
за разрешение произвольного формата.

Лаунчер показывает размер/глубину/режим вывода приложения, source/output rect
и cadence/p95. Probe сохраняет машинный display report. Проверены19 settings,
9 profile,3 shm,2 display parser tests и Qt ctest с проверкой фактических8 бит
при запросе10. Пакет с новой итерацией содержит184 файла; runtime ещё кандидат.

## UltraWide: подтверждение перспективы

Read-only GDB trace через `probe_linux.py --projection-trace LAYOUT_JSON`
собрал аргументы и выходные блоки без изменения гостевых регистров/памяти.
Layout получен компиляцией `offsetof(PPCContext, ...)` из SDK и привязан к SHA
исполняемого файла. [Исходник/ABI](../logs/acv-ultrawide-20261024/context_layout.cpp),
[layout](../logs/acv-ultrawide-20261024/context_layout.json),
[трасса](../logs/acv-native-linux/20261024T115046780620Z/projection.json).

Цепочка `0x82303138 → 0x82302520 → 0x82300948 → 0x82BC8980` наблюдается
при старте. В `0x82302520` r8/r9=1280/720; в `0x82BC8980`
f1=1.04719758, f2=1.77777779, f3=1, f4=1000. Все64 возвращённых блока
совпали с перспективой `y=1/tan(f1/2), x=y/f2, z=f4/(f4-f3)`,
последние строки `[0,0,z,1]`, `[0,0,-f3*z,0]` с максимальной нормированной
ошибкой3.285e-7 при пороге2e-6. Это обоснование интерпретации аргументов как
вертикальный FOV≈60°, aspect16:9 и near/far1/1000 для наблюдённых вызовов.
[Машинная проверка](../logs/acv-ultrawide-20261024/perspective-check.json),
[проверяющий скрипт](../tools/native-rexglue/analyze_projection_trace.py).

Ограничение:64 samples имеют один набор входов, получены из перекомпилированного
кода, а не оригинальной консоли. Главная gameplay-камера, HUD и culling ещё
не привязаны к этой цепочке; `0x82A0CA28` (Versus/free-camera assertion path)
не встретился. Breakpoints ограничены64 samples на entry; это стартовая выборка,
не покрытие всего25s интервала. GDB child exit0, probe exit1 из-за прежнего
system.properties; shm orphan0 до/после. Нативный UltraWide этим не включён.
Отдельный пакет ACV-Linux остаётся ранее проверенной display-сборкой;
новая диагностическая опция пока находится в дереве разработки.

## UltraWide: эксперимент отдельного вызова — 2026-10-24

Временная read-only трасса60s со стеками охватывает восемь окон по8s,
до8 samples на entry в окне: [trace](../logs/acv-native-linux/20261024T115603115026Z/projection.json).
После диагностических Return/Space появились другие входы: FOV около36–55°,
near1, far450–13000. Все64 матрицы согласованы с формулой перспективы,
максимальная нормированная ошибка3.853e-7:
[проверка](../logs/acv-ultrawide-20261024/temporal-perspective-check.json).
Это расширяет прежнюю стартовую выборку, но не доказывает полную игровую семантику.

В retail `0x822B4428` при нулевом поле входа+0x48 берёт aspect из объекта+0x4C0,
FOV из входа+0x40 с преобразованием единиц и вызывает `0x82BC8980` по адресу
`0x822B450C` (LR=`0x822B4510`). Near/far берутся из объекта+0x444/+0x448.
Ненулевая ветка вызывает другой helper `0x82BC8CC0`; она не изменена.
[Raw PPC/.pdata](../logs/acv-ultrawide-20261024/822B4428.txt),
[caller](../logs/acv-ultrawide-20261024/822B59C8.txt).

Добавлен [scene_aspect_experiment.cpp](../tools/native-rexglue/scene_aspect_experiment.cpp):
strong override `sub_82BC8980` через существующую weak-alias схему с вызовом
оригинального `__imp__sub_82BC8980`. По умолчанию полный pass-through.
Только явная переменная `ACV_EXPERIMENTAL_SCENE_ASPECT` (конечное число1…4)
включает эксперимент: LR должен быть0x822B4510, исходный aspect≈16:9,
FOV должен находиться в(0,π). Меняется только f2 на время оригинального helper;
после вызова исходное f2 восстановлено. Остальные callers не изменяются.

Изолированная сборка: `tmp/acv-ultrawide-test/`, host SHA`e3ab37044dc3037c9aa1ed7dae8fc13424a1e1b0e37dbae48a6a35b8b604355b`.
[Build manifest](../tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv-test-build.json)
фиксирует выбранные display runtime/GPU и неизменный config SHA98d1c2ed….
Прежний development host сохранён в `tmp/acv-ultrawide-before/`.
Отдельный пользовательский `../../ACV-Linux/` не заменён этой сборкой.

При запросе64:27 в60s запуске шесть измеренных вызовов0x822B4428 получили
aspect2.37037037; остальные58 сохранили16:9. Все64 выходные матрицы прошли
проверку, max error3.285e-7. Относительно16:9 коэффициент горизонтальной
проекции уменьшается до0.75, математический горизонтальный охват растёт в4/3;
это ещё не доказательство корректного широкого изображения на экране.
[Трасса](../logs/acv-native-linux/20261024T120037853774Z/projection.json),
[машинный результат](../logs/acv-ultrawide-20261024/aspect-trial-check.json).
GDB child exit0; probe exit1 сохраняет прежний optional system.properties error;
shm orphan0 до/после. Два запуска не используются как FPS benchmark.

Actual hook object прошёл30 host boundary cases: выключенная/валидная/невалидная
переменная окружения, выбранный/посторонний caller, исходный aspect и область FOV.
Проверены один вызов оригинала, восстановление f2 и сохранение возвращённого r3.
[Test source](../logs/acv-ultrawide-20261024/hook_boundary_test.cpp),
SQLite operation `d6fd7504-54ba-438a-8db6-424f891e74db`.
Это проверка границы wrapper со stub оригинала, а не parity всей игры.

**Не принято:** полная принадлежность основной gameplay-камере; согласование
culling, прицела, HUD, меню, вспомогательных проекций и широкого viewport/output.
Эксперимент меняет одну проекцию при прежней геометрии вывода, поэтому не
публикуется как готовый UltraWide. Настройка в лаунчере ещё не включена.

## Полноэкранный тест2560×1080 — 2026-10-24

По запросу пользователя выполнен120s X11 запуск isolated hoste3ab3704…,
FIFO/VSync, cap60, Bilinear8-bit, `ACV_EXPERIMENTAL_SCENE_ASPECT=64/27`.
После исправления ошибочного `--letterbox=false` на существующий SDK cvar
`--present_letterbox=false` подтверждены видимое окно2560×1080 и output rect
`x=0,y=0,width=2560,height=1080`; source остаётся1280×720. В сцене сработал
ограниченный override0x822B450C. Первый запуск с полосами1920×1080,x320
отклонён и завершён перед повтором. Canonical config не редактировался.

Runtime завершился0 по лимиту120s; probe1 из-за прежнего optional
system.properties. Shm orphan0 до/после. Интервалы XE_SWAP дали окна около
15.5–60 FPS; поздняя тяжёлая сцена≈15.5–15.9. Это не контролируемый benchmark,
стабильные60 FPS не подтверждены. Визуальная оценка пользователя пока не
получена; HUD/прицел/culling и отсутствие искажений не приняты. ACV-Linux
этой экспериментальной сборкой не заменён.
[Результат](../logs/acv-ultrawide-2560x1080-20261024/result.json),
[запуск](../logs/acv-native-linux/20261024T120920332499Z/launch.json).

## TRG-кандидат — 2026-10-25

Отдельная папка `../../ACV-Linux-TRG-20261025/start.sh`,185 файлов manifest;
прежний Qt-пакет `../../ACV-Linux/` сохранён. TRG/C++17, ImGui1.92.9b,
SDL2 2.32.10, OpenGL3.3, Ember, шесть страниц и кириллица. Используется
прежний Python backend, асинхронные снимки и атомарная проверенная запись;
профили/UUID/XUID/backup/restore и данные XDG сохранены. Qt/ICU отсутствуют.
Реальный тест GUI→probe→game подтвердил выбранные библиотеки, обычную сессию
без таймера, запрет второго writer, продолжение игры и блокировки после закрытия
GUI, освобождение после выхода. Метрики подачи кадров не доказывают скорость игры.

Реальный ISO: проверены Title ID4E4D0853 и все26 хешей, отказ при существующей
установке, отмена/удаление staging; оригинал не изменён. Публикация атомарная,
без замены существующего каталога. Тест переноса выявил package/tmp/acv-system-root;
пустой system: теперь создаётся в кэше выбранного профиля. Исправлен также
счётчик fatal_error_count в наблюдателе probe; baseline тест падал, исправленный проходит.
Пакет включает шрифты/лицензии и не зависит от дерева SDK для runtime mappings.

Подробные ссылки и границы проверок: [English checkpoint](LAUNCHER_PACKAGE.md#trg-frontend-and-installation--2026-10-25).
Не приняты: игровой цикл save/load, выбор другой миссии, аппаратные controller/HiDPI,
интерактивная установка через UI, картинка CAS/FSR/10 бит, UltraWide HUD/culling,
высокая FPS и новый RUNTIME_BASELINE. Это static recomp, не полный восстановленный source.

Финальный артефакт проверен из cwd `/`: [checkpoint](../logs/acv-trg-final-20261025-r2/checkpoint.json).
Child0/fatal0, один user_language, XUID совпал, новых shm-сирот нет. Закрытие окна
через WM_DELETE_WINDOW оставляет игру и lock живыми до штатной остановки теста.
В оконном режиме рабочая область ограничила2560×1080 до2560×1052; изображение16:9
вписано1870×1052. Это не повтор fullscreen-теста21:9 и не проверка темпа gameplay.


### WaitFix-кандидат — 2026-10-25

Новый пакет: ../ACV-Linux-TRG-WaitFix-20261025/start.sh,185 файлов. Runtimeca04c1c6 исправляет дробную паузу WaitMultiple; Audio Worker78,05%→0,56% ядра.190с child0/fatal0. Стабильные60FPS ещё открыты. Манифест/зависимости проверены, Qt/ICU нет. Старые пакеты и сейвы сохранены.

WaitFix package X11 frontend/probe smoke PASS: cwd /, selected local libraries, no Qt/ICU, game and writer lock survive GUI exit, lock releases after game exit; child0/fatal0. [Checkpoint](../logs/acv-waitfix-package-smoke-20261025-r2/checkpoint.json).
