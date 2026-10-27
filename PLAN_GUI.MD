# ACV — план реализации сохранений, профилей и лаунчера

Дата: 2026-10-26. Статус реализации (проверено тестами и запусками по логам; картинка и геймплей визуально не проверялись, скриншоты запрещены правилами проекта):

- **A — частично.** `tools/native-rexglue/acv_settings.py`: версионируемая схема, приоритеты слоёв, миграция из `acv.config` (файл не меняется), маппинг в cvar SDK, `capabilities()` со статусами available/experimental/unavailable, `dedupe_options`, `runtime_features`. Runtime теперь сообщает реальные Vulkan surface formats/present modes, SDL modes, применённый swapchain, геометрию и интервалы кадров; данные сохраняются в launch.json и отображаются в лаунчере. Предстартовый запрос возможностей без запуска игры ещё не сделан.
- **B — сделано, кроме проверки в игре.** `acv_profiles.py`, `acv_profile_cli.py`, `probe_linux.py --profile`: отдельные `userdata`/`cache`, flock-блокировка (наследуется процессом игры и удерживается, пока жив хотя бы один writer), backup с проверкой и атомарной публикацией, restore с карантином оригинала, импорт, смена gamertag/имени пилота без смены UUID/XUID. Профиль ARTEM мигрирован. XUID профиля передаётся через `ACV_XUID` только в сборках с патчем; принятая сборка использует SDK XUID `B13EBABEBABEBABE`. **Проверка сохранений (2026-10-27, по файлам):** игра записала сейв под XUID профиля `00094AF2A434841E` (`APGD.DAT` 230 672 B, `PAINT.DAT` 116 248 B, 24.10 22:16), значит `ACV_XUID` работает в принятом рантайме. В изолированной копии `tmp/acv-user-data-name-trial/` `APGD.DAT` перезаписан 26.10 02:31 (тот же размер, другой хеш), `PAINT.DAT` не менялся: игра загрузила скопированный сейв и записала новый. Backup/restore на копии настоящих данных: backup проверен (16 файлов), порча → restore возвращает исходный хеш, повреждённая копия отвергнута, живой сейв цел. **Не проверено:** что именно изменилось в сейве между хешами (прогресс миссии) и перенос сейва на другой XUID; регрессия других миссий.
- **C — реализован TRG-кандидат; приёмка частичная.** `tools/acv-trg-launcher/`: C++17, Ember, шесть страниц, русский шрифт, асинхронный CallbackSettings → существующий Python backend, профили/backup/restore, пресеты, журнал решений, проверка перед PLAY и `/dev/shm` preflight. Реальный X11 тест подтвердил запуск выбранной сборки и профиля, обычную сессию без лимита, продолжение игры и удержание writer lock после закрытия GUI, освобождение после выхода. ISO импорт через TRG Task проверен на реальном образе: Title ID, 26 SHA-256, staging/cancel/no-replace. Пакет-кандидат `../ACV-Linux-TRG-20261025/start.sh`, без Qt/ICU; [финальный запуск](logs/acv-trg-final-20261025-r2/checkpoint.json) из cwd `/`: child0/fatal0, XUID совпал, новых shm-сирот нет; прежний `../ACV-Linux/start.sh` сохранён. Исторический запуск Qt `20261023T234857821326Z` остаётся подтверждён. Полная приёмка контроллера/HiDPI и игровой save/load открыта. [Подробности](docs/LAUNCHER_PACKAGE.md#trg-frontend-and-installation--2026-10-25).
- **D — частично, с проверкой вывода (2026-10-24, `logs/acv-ultrawide-2560x1080-20261024/`).** Вывод 2560×1080 в полном прямоугольнике, без полос подтверждён (`corrected-window-check.json`: x=0, y=0, 2560×1080, FIFO, код выхода 0, новых утечек `/dev/shm` нет). Экспериментальная проекция 64:27 (aspect 2.370) включается и срабатывает (4 записи в логе), но геометрия, HUD, прицел и отсечение (culling) визуально не оценены — это нужна проверка человеком. В тяжёлой сцене частота подачи кадров падала до ≈15,5 FPS при лимите 60; источник этой метрики (`swap_cadence_*` в `result.json`) в `runtime.log` не найден, поэтому величину надо перепроверить до принятия. Разделены размер host-окна и guest-видеорежим 1280×720. Вписывание 16:9 без обрезки/растяжения подтверждено для UltraWide и 16:10 и после resize. VSync On выбирает FIFO, Off — Immediate/Mailbox при доступности. Лимитер 60 измерен по 300 swap intervals: стабильные окна около 60 FPS; на старте есть просадки. Скорость игрового времени/звука не подтверждена. 120/144/165 FPS, расширенный FOV и адаптивный HUD ещё не реализованы/не приняты.
- **E — частично.** Добавлен экспериментальный запрос 10-битного SDR swapchain только из реально предложенных пар format/colorSpace. Текущая X11-поверхность предлагает лишь BGRA8 UNORM/SRGB: запрос 10 явно возвращается к 8, лаунчер показывает результат. Проверка 10-битной картинки требует подходящей поверхности/экрана. Физическое RGB/Y′CbCr-кодирование HDMI/DP системой не опрошено и не управляется.
- **F — частично.** Анизотропия, FXAA, дизеринг, масштаб рендера (экспериментально) подключены через cvar. CAS/FSR1 — экспериментально, нужен рантайм с FidelityFX (`tools/native-rexglue/build_sdk_ffx.py`; сборка проходит; подтверждены исполняемые CAS sharpen/resample и FSR EASU/RCAS passes, картинка не проверена); FSR2/FSR3 недоступны. Резкость CAS — `sharpness`; FSR RCAS — `fsr_sharpness` (0…2 stops). Bilinear явно передаётся для возврата к базовому эффекту.
- **Отдельная папка игры и лаунчера:** `../ACV-Linux/`, запуск `./start.sh`. Сборщик `tools/acv-launcher/standalone.py`: 184 файла, локальные игровые данные, Qt/ICU, runtime/GPU/FidelityFX, manifest с хешами. Сохранения — XDG data, логи — XDG state, вне каталога программы. Проверены видимое окно лаунчера, X11 game startup из cwd `/`, локальные библиотеки по `/proc/maps`, отсутствие новых shm orphan. **Runtime с патчами включён как кандидат; принятая baseline не заменена.** Исходный `package.py` остаётся сборщиком development-layout пакета. [Проверки и ограничения](docs/LAUNCHER_PACKAGE.md).
- **Профиль старта X11 получен:** `20261024T000238328506Z`, 58 samples / 60,97 с, `/dev/shm` без orphan до и после. Основной host thread — `ioctl` в 36/58 samples; это наблюдение под GDB, не замер FPS и не установленная причина задержки. Анализ/оптимизация старта остаются открыты.

**Найденные и исправленные проблемы запуска (2026-10-24)**

1. Повторный `--user_language` (профиль + `acv.config`) ломал разбор аргументов, GPU-плагин не загружался → `dedupe_options`.
2. Утечка `/dev/shm/xenia_memory_*` по 4,8 ГБ при жёстком выходе (58 файлов, 19 ГБ) → SIGBUS в `Memory::Initialize`. Патч SDK `patches/launcher-runtime-limiter-xuid-shm.patch` (`shm_unlink` сразу после создания; проверено `kill -9`: утечки нет), для принятых сборок — `acv_shm.py` и кнопка «Очистить» с подтверждением, блокировка старта при <6 ГБ свободно.
3. Под SDL Wayland окно игры не показывается и не закрывается, процесс висит → по умолчанию X11 (XWayland), выбор в лаунчере. Причина на стороне Wayland не исследована.

**FPS, диагностика только после90с (2026-10-26):** разделены причины ожиданий.
В full ожидание текущего результата≈57%, memcpy≈13%; ожиданий занятого write-slot нет.
В fast основная задержка — существующее, но незавершённое прошлое поколение:
9217 ожиданий против7 отсутствующих результатов. Исправлен выбор поколения в fast;
ASan/UBSan проверяет ticket и фактические байты. Отдельный кандидат45dbaab6,
180с child0/fatal0, но всё ещё15–17FPS. Эксперимент неблокирующей отправки fast-копий
(d2404533) завершён:180с child0/fatal0, пользователь вошёл в gameplay; поздние окна22–25FPS,
GPU88,4%. Это наблюдаемые сессии, не одинаковый A/B; картинка fast не принята.
TimerQueue переведён на ожидание публикации/дедлайна: host1ms CPU10,03%→1,19%,
callback/cancel/join tests PASS. Runtime7b1f69a0 проверен в подтверждённом
пользователем gameplay: timer CPU9,33%→0,64% ядра,90 rows/6393 IPs без потерь;
запись с90,00036с. Игра оставлена открытой без3-минутного лимита; stable60FPS открыт.
[Timer checkpoint](docs/NATIVE_REXGLUE_LINUX.md#timerqueue-blocking-wait-and-deadline-validation-2026-10-26). Full остаётся обычным режимом, стабильные60FPS и корректность
картинки fast не приняты. [Детали](docs/NATIVE_REXGLUE_LINUX.md#resolve-wait-attribution-and-fast-generation-correction-2026-10-26).

**Текущий приоритет пользователя:** UltraWide/16:10 → FPS → экран → цвет → CAS/FSR. Подтверждённое вписывание не означает расширенный игровой обзор. [Результаты текущей итерации](docs/LAUNCHER_PACKAGE.md#экран-цвет-и-подача-кадров--2026-10-24).

**Миссия 2 принята пользователем:** исправлен повторный pthread_join при ожидании завершённого потока. Прежняя гипотеза submit/present не подтвердилась; подробная история сохранена в NATIVE и [снимке MD](logs/acv-trg-docs-20261025/manifest.json).

**Выбор миссии — подтверждён пользователем 2026-10-25:** после загрузки изолированной копии профиля миссия работает. Обычный запуск без GDB `20261025T165411391172Z` завершился штатно по лимиту180с (child0/fatal0); переход и последующая работа подтверждены пользователем. Runtime `f3600aca…`, экспериментальная копия `tmp/acv-user-data-name-trial/`. Оригинальный сейв не заменён; полноценная миграция и цикл save/load остаются открыты.

**Последний вылет диагностического запуска:** SIGABRT был у GDB PID36997, не у ELF игры; GDB упал на `breakpoint.c:9182, thr != nullptr` при работе thread-scoped return breakpoints. Наблюдатели переведены на явное сопоставление native return PC / ptid / SP; тест160 завершающихся потоков и3840 callbacks прошёл. Опрос ввода каждый кадр выключен по умолчанию. Предупреждение игры о6MB отдельно записано: XAM фактически вернул3GiB свободного места, поэтому недостаток места на HDD не установлен как причина.

**FPS — исправлена лишняя загрузка CPU в аудио:** WaitMultiple округлял паузу1мс до нуля. Host test99,5%→1,1%; в игре Audio Worker78,05%→0,56% ядра. Runtimeca04c1c6,190с без GDB, child0/fatal0. Новый отдельный пакет ../ACV-Linux-TRG-WaitFix-20261025/start.sh (185 файлов). Тяжёлая сцена всё ещё16–21FPS: стабильные60 не достигнуты. Следующий шаг — GPU command/guest scene work и компиляция новых pipeline. Затем UltraWide/FOV/HUD, экран/цвет и CAS/FSR по критериям ниже. [Технический checkpoint](docs/NATIVE_REXGLUE_LINUX.md#mission-accepted-and-debugger-failure-2026-10-25).

**`system.properties` (2026-10-24):** файла нет ни в индексе dvdbnd (1560 вариантов пути, контроль проходит), ни среди 64 153 распакованных файлов; в XEX найдены ключи движка Dantelion2 (`Core.System.*`, `Xenon.IO.CacheDrive.*`, `Core.IO.*`) — это необязательный файл разработчика, при отсутствии действуют встроенные значения. После запроса идут заглушки `XFileFsDeviceInformation`/`IoDismountVolumeByFileHandle` (реальных сведений о томе нет, сбоя после них не видно). Подробно: `docs/NATIVE_REXGLUE_LINUX.md`.
**`system:` (2026-10-24):** устройство смонтировано как пустое, ошибка `ResolvePath(system:\)` исчезла, `system.properties` по-прежнему отсутствует со статусом `0xc000000f` (поведение игры не меняется). DLC-пропуски оставлены по просьбе.
**История прежних зависаний миссии2:** сохранена в NATIVE и [снимке плана](logs/acv-mission-accepted-docs-20261025/manifest.json). Актуальное исправление — однократный join; переход принят пользователем.
**Новый пакет (2026-10-27):** `../ACV-Linux-TRG-WideView-20261027/start.sh` (185 файлов, TRG, без Qt/ICU, с обновлённым рантаймом из текущего дерева SDK и хуками `wide_view`). `verify_package.py` и `verify_frontend.py` проходят (лаунчер→игра→блокировка профиля), пользователь подтвердил, что игра работает. Прежние пакеты не тронуты. Подробно: `docs/LAUNCHER_PACKAGE.md`.
**16:10 (2026-10-27):** хуки `wide_view` поддерживают и вывод уже 16:9 (горизонтальный обзор сохраняется, вертикальный FOV растёт, HUD расширяется сверху/снизу). Запуск по настройкам 1728×1080 (`20261026T195018601039Z`): хуки сработали, пользователь подтвердил, что работает. В окне геометрия неточна из-за рамки (1,665 против 1,6), точна в fullscreen. Переключатель «Расширенный обзор» добавлен на страницу «Экран» TRG-лаунчера (сборка и 2 теста проходят). Отсечение по краям, прицел и ролики отдельно не проверялись.
**`wide_view` (2026-10-27):** экспериментальная настройка `wide_view=auto` в `acv_settings.py` включает расширение обзора и центрирование HUD по выбранному разрешению (аспект выводится из WxH, нужен явный размер; для вывода не шире 16:9 ничего не меняется). Хуки встроены в принятый ELF (выключены по умолчанию), базовая линия обновлена. Проверено запуском только по настройкам (`20261026T194525066353Z`). В интерфейсе TRG пока нет переключателя.
**UltraWide (2026-10-27):** перепись вызовов проекции показала два места перспективной камеры (`0x822B4510`, `0x82300978`) и HUD-ортографию `0x822B5368`; эксперимент расширен на оба места камеры и центрирование HUD (по умолчанию выключен, `ACV_EXPERIMENTAL_SCENE_ASPECT` / `ACV_EXPERIMENTAL_HUD_ASPECT`). Сессия `20261026T194121365223Z` на 2560×1080 без полос; пользователь принял картинку как экспериментальный режим. Не проверено: отсечение по краям, прицел, ролики/меню, другие миссии. Детали: `docs/NATIVE_REXGLUE_LINUX.md`.
**Открыто (на 2026-10-24, после ультраширокого теста):** лаунчер после закрытия игры пишет итог сессии (`acv_profile_cli.py last-run`: вывод, FPS по `[acv-display] pacing`, фатальные ошибки). Замер лимитера 60 FPS на тяжёлой сцене не выполнен: нужен контролируемый gameplay-сценарий; автономные bounded tests уже разрешены пользователем.
**Открыто:** проверка сейвов в игре; проверка картинки CAS/FSR и темпа с лимитером; включение патченного рантайма в принятый набор (с новой базовой линией); анализ полученного профиля старта X11 и измерения без GDB; XUID в сейвах; реализация D/E/F-пунктов, помеченных недоступными.
Итоговый артефакт по уточнению пользователя: **игра и лаунчер в отдельной папке**, без зависимости от дерева разработки/SDK; обновление программы не затрагивает сохранения. Локальные игровые данные входят в эту папку, в GitHub не отправляются.
Платформа первого выпуска: Linux, текущая native ReXGlue/Vulkan-сборка.

**Оценка остатка:** около 35% плана (погрешность порядка 10 п.п.), выполнено около 65%. Это экспертная оценка функций и приёмки A–F, не процент восстановленного retail source. Основной остаток: полный цикл сейвов и регрессия других миссий, FOV/HUD/culling, timestep/>60 FPS, визуальная проверка CAS/FSR и цветовых режимов.

## 1. Объём и порядок работы

1. Реализовать постоянный профиль и штатные игровые сохранения.
2. Заменить Qt-интерфейс лаунчером на trg-launcher, сохранив backend профилей, настроек и запуска игры.
3. Подключить разрешение, режим окна, VSync и настройки кадровой частоты.
4. Добавить настоящий адаптивный рендер для 16:9, 16:10 и UltraWide 21:9.
5. Реализовать выбор доступного цветового вывода: RGB Full Range и Y′CbCr
   4:4:4 / 4:2:2 / 4:2:0, глубина 8 / 10 бит на компонент.
6. Добавить проверенные настройки улучшения графики и собрать пакет для теста.

Повторённый в запросе 21:9 считается одним режимом UltraWide. Два независимых
параметра — разрешение рендера и разрешение вывода — должны иметь отдельные поля.
Предыдущие задачи по отсутствующим DLC и отдельным readback stress-проверкам
остаются пропущенными по решению пользователя; они не входят в этот план.

## 2. Исходная основа и технические границы

Лаунчер управляет настройками, профилями и процессом игры. Сохранение игровой
сессии выполняет сама игра через runtime. Смена FOV, поддержка высокой частоты
игровых кадров и новые форматы вывода требуют изменений runtime/guest hooks,
которые нельзя заменить одним переключателем в интерфейсе.

Подтверждённые точки подключения в текущем дереве:

| Возможность | Текущая точка подключения | Что требуется |
|---|---|---|
| Каталог пользователя | [probe_linux.py](tools/native-rexglue/probe_linux.py), `--user-root`, `--user_data_root` | Постоянный выбор на профиль вместо нового каталога на каждый обычный запуск |
| Имя пилота и gamertag | [acv.config](tools/native-rexglue/acv.config), [acv_compat_shims.cpp](tools/native-rexglue/acv_compat_shims.cpp) | Добавить постоянный ID/XUID и согласовать его с сохранениями |
| Пользователь и контент | Локальный SDK: `src/kernel/xam/xam_user.cpp`, `xam_content.cpp` | Проверить реальные вызовы сохранения ACV и устранить задействованные пробелы |
| Размер окна | Локальный SDK: `src/ui/window.cpp`, `resolution` | Проверить семантику параметра и подключить выбор экрана/режима |
| VSync | Локальный SDK: `src/graphics/command_processor.cpp`, `vsync` | Согласовать с Vulkan present mode и лимитером |
| Масштаб рендера | Локальный SDK: `src/graphics/pipeline/texture/cache.cpp`, `resolution_scale`, `draw_resolution_scale_x/y` | Проверить реальную поддержку Vulkan-пути ACV и лимиты устройства |
| Анизотропия | Там же, `anisotropic_override` | Корректно сопоставить пользовательские значения с кодами SDK |
| Финальный вывод | Локальный SDK: `src/ui/presenter.cpp`, `src/ui/vulkan/vulkan_presenter.cpp` | Расширить согласование форматов и подключить доступные эффекты |

Изменения SDK сохранять воспроизводимыми патчами в репозитории. Generated C++
не использовать как основной слой исправлений. Принятый texture fix должен
оставаться в выбранном GPU-артефакте при каждой сборке лаунчера/игры.

## 3. Этап A — контракт настроек и возможностей

- Определить одну версионируемую схему: общие настройки лаунчера, настройки
  графики и запись профиля. Разделить UUID профиля, XUID, имя пилота и gamertag.
- Задать приоритет: встроенные значения → сохранённые настройки → явный выбор
  в лаунчере → параметры конкретного запуска. Не допускать разных правил у
  лаунчера и CLI.
- Реализовать миграцию существующих ARTEM/ARTEM и пользовательской конфигурации
  без потери исходных файлов. Имя пилота, записанное внутри сейва, не считать
  автоматически изменяемым через переименование профиля лаунчера.
- Подготовить запрос возможностей runtime: экран, размеры, частоты, Vulkan
  surface formats/color spaces, present modes, доступная память и графические
  функции конкретной сборки.
- Разделить запрошенные и фактически применённые параметры. Runtime возвращает
  итоговый режим, а лаунчер показывает причину отклонения или fallback.

Готово, когда одинаковая конфигурация одинаково трактуется CLI и лаунчером,
неподдерживаемые значения отклоняются, а текущие настройки корректно импортируются.

## 4. Этап B — постоянный профиль и Save

### B1. Хранилище и идентичность

- Создавать постоянную запись профиля: UUID, стабильный XUID, отображаемое имя,
  gamertag, имя пилота, язык и привязанный каталог игровых данных.
- Использовать отдельный `user_data_root` для каждого профиля. Для разработки
  хранить данные в `tmp/acv-user-data/profiles/<uuid>/`; при подготовке отдельного
  установленного пакета сохранить постоянные XDG-каталоги через существующий Python backend, без QStandardPaths.
- Передавать выбранный XUID согласованно в XAM user/profile/content manager;
  проверить, как именно SDK сейчас создаёт пользовательский профиль.
- При переименовании сохранять UUID/XUID и владельца сохранений.
- Отделить сейвы от shader cache и журналов. Сброс настроек графики не должен
  затрагивать прогресс игры.
- Блокировать одновременную запись двух экземпляров игры в один профиль.

### B2. Штатный путь сохранения игры

- Проследить реально используемые ACV вызовы: выбор устройства, перечисление,
  создание/открытие контента, чтение/запись/flush/close и overlapped completion.
- Проверить привязку контента к XUID, title ID и профилю, восстановление mount
  при повторном запуске, доступное место и обработку ошибок записи.
- Реализовать только задействованные недостающие участки runtime. Например,
  наличие незавершённого `XamContentResolve` в SDK само по себе ещё не доказывает,
  что этот вызов блокирует сохранения ACV.
- Сохранять родной формат ACV и его правила проверки целостности. Игровые сейвы
  должны восстанавливаться после полного завершения и повторного запуска игры.
- Снимок памяти процесса не включать в эту функцию: нужен игровой прогресс,
  который читает сама ACV.

### B3. Управление сохранениями

- Добавить создание/выбор/переименование профиля, открытие его каталога,
  резервное копирование и восстановление сейвов.
- Делать согласованный backup после завершённой записи игры либо после выхода;
  временный backup публиковать атомарно после проверки файлов.
- Поддержать импорт существующего собственного сохранения с проверкой формата
  и владельца. Миграцию владельца реализовывать только после установления
  структуры; не менять XUID внутри файлов вслепую.
- При повреждении сохранять проблемный оригинал и предлагать проверенный backup.
  Восстановление выполнять при остановленной игре.

Готово, когда прогресс/сборка/настройки игры переживают перезапуск, два
профиля изолированы, переименование сохраняет доступ, backup восстанавливается,
а отказ записи не уничтожает последнее рабочее сохранение.

## 5. Этап C — переход на trg-launcher вместо Qt

Основа: [TekRantGaming/trg-launcher](https://github.com/TekRantGaming/trg-launcher),
локально `tools/trg-launcher/`; ревизия закреплена в
[manifest внешних исходников](tools/external-references.json).
Исследованные API: `include/trg/launcher.h`, `settings.h`, `standalone.h`,
`xbox360.h`, а также `docs/INTEGRATION.md` и `CMakeLists.txt`.
C1 собран, основные C2/C3 реализованы и проверяются отдельными evidence; критерии ниже сохраняются до полной приёмки. Проверка Qt сама по себе не подтверждает TRG.

### C1. Сборка и размещение

- Сделать отдельный ACV frontend C++17/CMake на Dear ImGui 1.92+;
  использовать `TRG_LAUNCHER_STANDALONE=ON` и `trg::launcher_standalone`
  (SDL2/OpenGL 3.3), отключить upstream demo. OpenGL нужен окну лаунчера;
  игровой процесс продолжает использовать существующий Vulkan backend.
- Добавлять upstream через `add_subdirectory` из локального checkout;
  фиксировать ревизии TRG, ImGui и SDL2. Текущие upstream defaults:
  ImGui v1.92.9b, SDL2 release-2.32.10. Не предполагать совместимость
  встроенного в SDK ImGui с API 1.92; standalone отделяет эти зависимости.
- ACV-код держать отдельно от upstream, например `tools/acv-trg-launcher/`.
  Существующий `tools/acv-launcher/` сохранять до приёмки замены как рабочий
  checkpoint и источник требований, Qt-зависимости в новый пакет не включать.

### C2. Интерфейс и контракт backend

- Использовать штатные header, sidebar и footer Save/PLAY; тема Ember,
  заголовок Armored Core V, русский текст и шрифт с кириллицей.
  Страницы: «Запуск», «Профиль и сохранения», «Экран», «Цвет», «Графика», «О проекте».
- `Choice`, `Toggle`, `Combo`, `SliderInt/Float`, `InputText`, `FolderRow`
  привязать через `CallbackSettings` к текущей схеме `acv_settings.py`.
  Загружать снимок значений асинхронно; чтение виджетов не запускает CLI каждый кадр.
  Save отправляет проверенные изменения в существующий backend с атомарной записью;
  `TextFileSettings` не должен перезаписывать `acv.config` или создавать вторую схему.
- Повторно использовать `acv_profile_cli.py`: UUID/XUID, XDG-пути, profile lock,
  backup/restore, capabilities, presets и last-run. Reset сбрасывает только
  настройки, сохраняя профили и сейвы. Показать requested/applied и причины fallback.
- Заменить QProcess асинхронным POSIX запуском с argv без shell, раздельными
  stdout/stderr, кодом выхода и сбором завершившихся процессов. Сохранить счётчик
  загрузки, остановку, `/dev/shm` preflight и журнал решений старого лаунчера.
- PLAY запускает обычную сессию без лимита probe. Исключить повторный writer,
  удерживать блокировку пока жива игра; закрытие лаунчера не обрывает сохранение.
  Проверку состояния игры и CLI выполнять вне UI thread; завершение launcher
  loop по `kPlay` согласовать с наблюдением/last-run и жизненным циклом ребёнка.
- Возможности экрана получать от игрового backend; DPI окна — от SDL2/ImGui.
  Проверить resize, клавиатуру, контроллер, HiDPI, кириллицу и понятные ошибки.

### C3. Установка и отдельный пакет

- Статус «Ready / Game files needed» связать с проверкой реального ACV manifest.
  Импорт собственного ISO выполнять фоновой `Task` с progress/cancel и проверкой
  title ID. Оценить встроенные `ReadXbox360TitleId`/`ExtractXbox360Disc`;
  `tools/extract-xiso/` — отдельный CLI-кандидат, не обязательный второй extractor.
- Исходный ISO не менять, извлекать в staging; публиковать готовый каталог только
  после проверки, отмена/ошибка не повреждает существующую установку.
  Внешние downloads/shader packs автоматически не включать.
- Адаптировать упаковку в отдельную папку игры/лаунчера: frontend, необходимые
  SDL2/OpenGL зависимости, Python backend, runtime/GPU и manifest с хешами.
  Сохранить XDG data/state и запуск из произвольного cwd; исключить Qt/ICU,
  если другие компоненты их не требуют. Игровые данные не публиковать в GitHub.
- Приёмка: frontend собирается без Qt; запускает выбранную сборку и профиль;
  настройки/сейвы прежнего пакета доступны; ошибки CLI не зависают в UI;
  запрет двух writers, backup/restore и last-run проходят проверки.
  Отдельно проверить реальный запуск/выход и упаковку, не считать upstream demo
  подтверждением интеграции ACV. После приёмки переключить `start.sh` на новый GUI.

Для последующих runtime/input доработок использовать
[ReAnswered как проверяемый пример](TOOLS.MD), с проверкой
адресов и поведения на ACV. Чужие guest hooks не являются готовыми исправлениями ACV.

## 6. Этап D — экран, соотношения сторон и кадровая частота

### D1. Разрешение и режим экрана

- Выбор монитора, оконного режима, окна без рамки и полноэкранного режима там,
  где это поддерживает текущая оконная система.
- Получать геометрию и DPI окна лаунчера через SDL2/ImGui, а поддерживаемые display modes и возможность
  смены частоты уточнять через игровой оконный backend. Геометрия окна лаунчера не
  является полным списком доступных видеорежимов.
- Добавить Auto/Native и проверенные разрешения, в том числе 1280×720,
  1920×1080, 2560×1440, 3840×2160; 1920×1200/2560×1600 для 16:10 и
  2560×1080/3440×1440 для UltraWide при наличии поддержки.
- Определять точное соотношение сторон по ширине/высоте; «21:9» использовать
  как пользовательское название семейства UltraWide-разрешений.
- Обрабатывать resize, смену монитора, DPI и пересоздание swapchain.
  Для опасной смены видеорежима предусмотреть автоматический откат без подтверждения.

### D2. Адаптивность изображения

- Сначала обеспечить корректное вписывание исходного изображения с сохранением
  пропорций для 16:9, 16:10 и UltraWide.
- Для настоящего расширенного игрового обзора найти матрицы проекции/камеру,
  изменить aspect ratio и FOV по подтверждённым guest hooks; проверить culling
  объектов по краям экрана.
- Подтверждён perspective helper `0x82BC8980` и отдельный вызов из `0x822B4428`.
  Выключенный по умолчанию native experiment64:27 проверен:6 матриц изменены,
  58 остальных сохранили16:9,30 host boundary cases прошли. Изолированная
  сборка в `tmp/acv-ultrawide-test/`; пользовательский пакет не заменён.
  Следующая работа: согласовать viewport/output и все связанные проекции/culling,
  проверить основную gameplay-камеру, HUD и прицел до включения в лаунчер.
  [Доказательства и границы](docs/LAUNCHER_PACKAGE.md#ultrawide-эксперимент-отдельного-вызова--2026-10-24).
- Привязать HUD, прицел, меню и области кликов к подходящим anchors/safe area.
  Для видеороликов и сцен с фиксированной композицией сохранять правильные пропорции.
- Нативный UltraWide считать готовым только при расширении обзора без растяжения
  геометрии и смещения прицела. Вписывание с полосами — отдельный режим.

### D3. Ограничение FPS и VSync

- Добавить значения лимитера **60 / 120 / 144 / 165 FPS** с точным таймером и
  контролем frame pacing. Частоту монитора показывать отдельно от лимита FPS.
- Сначала реализовать лимитер представления кадров; отдельно установить родной
  timestep ACV, ограничения VdSwap и зависимость движения, физики, анимации,
  таймеров, видео и звука от количества игровых кадров.
- Для реальных 120/144/165 FPS исправить только доказанные зависимости от
  timestep и проверить небинарные относительно 60 частоты 144/165. Простое
  снятие ожидания или повтор кадров не считать поддержкой высокой игровой FPS.
- Если безопасная развязка ещё не завершена, отмечать высокие режимы как
  недоступные/экспериментальные с причиной; базовый режим сохраняет штатную скорость.
- VSync On согласовать с доступным FIFO; VSync Off — с поддерживаемым
  MAILBOX/IMMEDIATE и фактическим поведением композитора. Не обещать снятие
  синхронизации там, где его принудительно задаёт окружение.
- Приоритеты лимитера и VSync определить явно: 165 FPS cap на 60 Hz экране
  не гарантирует 165 отображаемых кадров.

Готово, когда режимы корректно применяются, 16:10/UltraWide не растягивают сцену,
HUD и прицел совпадают с геометрией, а доступные 60/120/144/165 не меняют скорость
игры и звука. Для каждого высокого режима отдельно подтвердить фактическую FPS.

## 7. Этап E — RGB / Y′CbCr, 8 / 10 бит

В интерфейсе разделить кодирование цвета, chroma subsampling и глубину. Для RGB
предоставить Full Range; 4:4:4 / 4:2:2 / 4:2:0 относятся к Y′CbCr. Диапазон
Y′CbCr и матрицу преобразования задавать согласованно с реальным выходным трактом.

- Получать форматы и color spaces с **фактической Vulkan surface игры** через
  `vkGetPhysicalDeviceSurfaceFormatsKHR`; Данные дисплея лаунчера и поддержка GPU YCbCr-текстур
  сами по себе не подтверждают доступность вывода на монитор.
- Базовый режим: RGB Full Range, 8 бит. Затем добавить 10-битный RGB swapchain
  при наличии подходящей пары format/colorSpace и корректного shader-пути.
- Проверить 10-битную точность финального прохода, dithering при выводе в 8 бит
  и корректность чёрного/белого. 10 бит не считать автоматическим включением HDR
  или восстановлением точности из исходных 8-битных текстур.
- Для Y′CbCr 4:4:4 / 4:2:2 / 4:2:0 с 8/10 бит составить матрицу доступности
  GPU → драйвер → X11/Wayland → монитор/кабель. Показывать доступные сочетания.
- Реализовать backend управления выходным кодированием только для окружений,
  где существует проверенный API. На обычном desktop Vulkan физическое
  кодирование HDMI/DP может выбирать драйвер/композитор; такая настройка должна
  показываться как управляемая системой, если приложение не может её применить.
- Если потребуется отдельное преобразование изображения в Y′CbCr, реализовать
  GPU-проход с правильной матрицей, диапазоном и chroma siting. Само это
  преобразование не доказывает, что физический выход стал Y′CbCr.
- Проверять запрошенный и реально подтверждённый режим. При неподдерживаемом
  сочетании объяснять ограничение и предлагать RGB 8 бит; не писать «применено»
  на основании одной сохранённой настройки.

Готово, когда каждое доступное сочетание реально применяется и подтверждается
соответствующим уровнем API/выходного тракта; недоступные сочетания имеют
конкретную причину. Универсальную поддержку всех Y′CbCr-режимов на любом Linux
окружении не считать достижимой без подходящего системного backend.
Основание: [Khronos: swapchain и format/colorSpace](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_swapchain.html),
[Khronos: Y′CbCr conversion](https://docs.vulkan.org/guide/latest/extensions/VK_KHR_sampler_ycbcr_conversion.html).

## 8. Этап F — улучшение графики из доступного кода

| Настройка | Реализация | Условие включения |
|---|---|---|
| Масштаб внутреннего рендера | 1× / 2× / 3× / 4× через существующий SDK scale | Vulkan-путь, VRAM и bake/resolve корректны для выбранного масштаба |
| Анизотропная фильтрация | Игра / Off / 2× / 4× / 8× / 16× | Корректное сопоставление с `anisotropic_override`, лимитами и исключениями текстур |
| Масштабирование вывода | Bilinear; CAS/FSR по доступности | В текущем SDK расширенные эффекты ограничены `REX_HAS_FIDELITYFX_SDK`; проверить конкретный Vulkan backend |
| Резкость | Управляемая сила CAS/RCAS | Подключён реально работающий эффект, без чрезмерных ореолов |
| Сглаживание | Начать с проверяемого FXAA/SMAA финального прохода | Отдельный рабочий shader-путь и приемлемое качество HUD |
| Дизеринг | Опция финального вывода | Проверенный эффект для выбранной глубины цвета |
| Пресеты | «Совместимость», «Качество», «Пользовательский» | Каждый пресет содержит только прошедшие проверку настройки |

Настройки теней, дальности прорисовки, LOD и штатного постпроцессинга добавлять
только после нахождения соответствующих параметров или доказанных guest hooks.
FSR2/FSR3/TAA рассматривать отдельно после проверки motion vectors, depth и
интеграции истории кадров; наличие имени в cvar не доказывает готовность функции.
Сохранять принятую коррекцию `exp_adjust` и рабочую семантику Resolve/readback.

Готово, когда каждая опубликованная настройка меняет реальный рендер ожидаемым
образом, имеет проверенные значения и возвращается в базовый режим. Показать
фактические ограничения VRAM и изменение времени кадра для тяжёлых режимов.

## 9. Итоговая приёмка и пакет

- Профиль A: создать прогресс, сохранить, закрыть процесс, открыть заново и
  загрузить. Повторить для B; проверить изоляцию, rename и восстановление backup.
- Лаунчер: корректный запуск/завершение, отсутствие зависания UI, сохранение
  настроек, обработка неверных путей и запрет двух writers одного профиля.
- Экран: доступные 16:9 / 16:10 / UltraWide, HiDPI, resize и смена монитора;
  корректный FOV/HUD для реализованных адаптивных режимов.
- Частота: все доступные лимиты и комбинации с VSync; проверить темп игры,
  таймеры и звук. Отдельно указать экспериментальные или заблокированные режимы.
- Цвет: доступные сочетания RGB/Y′CbCr и 8/10 бит; проверить диапазон, градиенты,
  читаемость текста и фактический режим вывода на подходящем оборудовании.
- Графика: применимость пресетов, сохранение исправленного рендера мехов,
  корректность изменённых функций и возврат к базовому режиму.
- Собрать Linux-пакет: лаунчер, runtime, проверенный GPU plugin, конфигурация и
  краткая инструкция. Хеши артефактов и реальные результаты сохранить в manifest.
  Игровые данные, личные сейвы, cache и логи в GitHub не включать.

Порядок зависимостей: A → B → C → D → E → итоговый пакет.
Готовность этапа подтверждать его критериями; неподдерживаемую аппаратно функцию
фиксировать как недоступную, а ещё не реализованную — как незавершённую.

#### FPS checkpoint — FBO retest, 2026-10-26

- FBO/full on current fixes still produces black gameplay (user confirmed).
  Capture at100s proves source1280x720 and output2560x1080 both have zero RGB;
  only3 presentation draws captured, so the failing scene resolve is not located.
- FBO excluded from working configuration; FSI/full relaunched. Stable60FPS
  remains open. Next: scene-containing FBO capture/resolve diagnosis and accurate
  deferred readback; increasing the FPS cap alone does not address measured waits.
- Evidence: [FBO retest](logs/acv-fbo-retest-20261026/checkpoint.json).

- FSI/full restoration accepted by user: black screen gone, gameplay entered.
  New60s sample after137s: ~15FPS, GPU command thread37.36% core; copy/wait
  optimization remains open. [Measurement](logs/acv-fsi-full-confirmed-20261026/summary.json).

#### General deferred readback — implementation started, 2026-10-26

- Host queue implemented and ASan/UBSan tested: ordered publication, one wait per
  flushed prefix, staging retirement before reuse, bounded capacity and failure handling.
- Runtime integration pending: read-access providers across physical aliases,
  lock release/revalidation, host accesses, free/protect/teardown and GPU worker waits.
- Acceptance: exact current-generation bytes versus full, allocation/reuse/concurrent
  CPU regression, correct gameplay/textures, then after90s same-scene FPS measurement.
  No runtime mode switch or FPS improvement yet.
  [Checkpoint](logs/acv-deferred-readback-core-20261026/checkpoint.json).

#### Readback integration checkpoint — 2026-10-26

- Built/host-tested: thread-safe GPU callbacks and restricted readback completion
  during WAIT_REG_MEM. Candidate tmp/acv-readback-service-fix/, GPU48994593.
- Still open: memory ownership generations and CPU read providers across aliases;
  unlocked wait/revalidation; host direct writes and free/decommit ordering; Vulkan
  staging integration. Existing callbacks run after file writes/parent release and
  cannot simply be reused to publish delayed GPU data.
- General deferred readback remains disabled; final candidate not runtime-tested;
  stable60FPS remains open. [Checkpoint](logs/acv-readback-dispatch-20261026/checkpoint.json),
  [memory analysis](logs/acv-readback-dispatch-20261026/memory-analysis.json).

#### SDK physical read providers — 2026-10-26

- Implemented and tested against compiled SDK: CPU read/write page faults across
  A/C/E aliases, unlocked completion waits, ordered single publication and partial
  CPU writes. Physical alias free/decommit/protect/fixed commit synchronize first;
  per-4KB generations reject stale frees after same-address allocation reuse.
- Fixed provider watch rearming after Protect/fixed commit and writable private
  publication mapping when guest aliases are read-only. Durable SDK patch included.
- Candidate tmp/acv-memory-provider-fix/ uses runtime a9125a31, GPU48994593,
  accepted FFX0933159a. 75s X11/full/FSI smoke child0/fatal0; accepted package untouched.
- Open before enabling general deferral: host direct physical access (file IO/XMA),
  direct parent-heap allocator paths, reset/save/restore, staging ownership and GPU
  teardown integration. Then current-generation byte oracle, gameplay and after90s FPS.
  General deferred producer remains off; no measured FPS gain.
  [Checkpoint](logs/acv-memory-provider-bridge-20261026/checkpoint.json).

- Follow-up: physical file IO now reserves its range across unlocked ReadSync, drains
  pending GPU data before partial writes and invalidates after success/partial failure.
  Actual XFile body + compiled SDK memory regression PASS, alias regressions PASS.
  New runtime built/preserved, not launched; XMA/multi-range and Vulkan integration open.
  [Checkpoint](logs/acv-host-physical-io-20261026/checkpoint.json).

- XMA follow-up: input/output/context buffers now acquired atomically; no partial
  reservation is retained during GPU waits. Decode runs without global lock and
  output invalidation precedes release. Actual Work/SDK memory test plus file/alias
  regressions PASS. Runtime5b126405 passed75s X11/full/FSI child0/fatal0.
  General Vulkan producer still off; next staging/queue/teardown integration.
  GPU writeback mirror7F000000 covers physical0..16MiB outside A/C/E watches:
  exclude this range from deferral until covered. No gameplay FPS result.
  [Checkpoint](logs/acv-xma-physical-access-20261026/checkpoint.json).

- Graphics CPU coherence follow-up: guarded physical words/snapshots now cover GPU
  packet/constant/shader/resolve/occlusion CPU consumers. Packet wrapping and inline
  shader addresses preserved; upload submissions reopen after a provider wait.
  Completed-provider retirement and queue admission tested. Runtime131b829f /
  GPUf2f3ee56 passed75s X11/full/FSI startup, session20261026T181420117779Z,
  child0/fatal0. No post90s gameplay/FPS acceptance; general deferral still off in
  this candidate. General Vulkan producer integration is now in progress behind
  an explicit experimental flag; original accepted package preserved.
  [Checkpoint](logs/acv-graphics-cpu-coherence-20261026/checkpoint.json).

#### General Vulkan readback — experimental integration, 2026-10-26

- Built opt-in producer: exact current-generation staging copies, ordered CPU
  publication, bounded pool (64 jobs/64MiB), slot retirement and device-live teardown.
  Full/native FSI only; physical0..16MiB excluded for7F mirror. No default change.
- Real Vulkan + compiled SDK Memory byte oracle on GTX1660 SUPER PASS:
  11 copies, overlapping generations,8 guest faults, slot reuse and partial CPU
  writes. Actual-source teardown ASan/UBSan passes8 fence/device-loss cases.
  Page-rounded host IO reservations prevent neighboring same-page dependency cycles;
  graphics, XMA, file IO and A/C/E allocator regressions PASS.
- First180s runtime20261026T183953558552Z ended child0/fatal0/runtime_errors0.
  After90s:52709 general resolves,52700 publications,3217 prefix fence waits.
  Timers are inclusive: draw includes Resolve; do not add nested percentages.
  No verified FPS increase or image/gameplay acceptance.
- Latest source adds unconditional submission drain before pool destruction,
  even when a failed copy never entered the bridge. Built in isolated
  tmp/acv-general-readback-teardown-fix/ (runtimeb0bf9b46/GPU06400c82);
  second gated CPU/GPU profile running. Next optimize the measured hot path,
  then identical-scene/game-time and image/save/mission regression.
  Accepted standalone package preserved; overall estimate remains65% done/35%
  remaining (±10pp), because gameplay acceptance and60FPS are still open.
  [Checkpoint](logs/acv-general-vulkan-readback-20261026/checkpoint.json).

- Coherent CPU read fast path completed: no temporary token/generation allocations
  if the memory lock stays held and no IO/provider overlaps. Conflicts retain the
  original wait path. Committed-page/private-mapping validation is shared.
  Same-input host benchmark:6→0 allocations/read,107.546→22.275ns median
  (4.83x at this boundary, not FPS). Actual SDK IO/hole/readonly/packet,
  real GPU, file/XMA and alias/ABA tests PASS. Candidate runtimee12818a0 /
  GPU06400c82 passed180s X11/full/FSI, session20261026T185840508042Z ended
  child0/fatal0. Profile after90s:GPU-worker66.47% core vs prior81.21% observed,
  different scenes; no exact FPS improvement claim. Next remove measured provider
  list scan cost and distinguish CPU-demand waits from pool-pressure waits.
  [Checkpoint](logs/acv-physical-read-fastpath-20261026/checkpoint.json).

#### Current progress and remaining work — 2026-10-27 local

**Estimate:**65% completed /35% remaining (±10pp), functions and acceptance
of A–F; not percentage of reconstructed retail source.

Completed this follow-up: experimental current-generation GPU readback with
A/C/E and host IO/XMA coherence, bounded staging ownership/teardown, zero-allocation
ordinary CPU reads and conservative provider range rejection. Independent model,
ASan/UBSan, compiled SDK and real Vulkan byte regressions PASS. The64-provider
host read benchmark improved median89.258→22.3419ns (4.0x at this boundary only).
Matched runtimeebf338a1/GPU14154063 with accepted FFX0933159a passed180s
X11/full/native FSI, session20261026T191748089182Z ended child0/fatal0.
No live game/build remains from this test. Accepted package unchanged.

**Next, in priority order:**

1. Reduce pool-pressure stalls: after90s the latest capture shows4019 general
   waits, all classified as pool-related (~31.6% inclusive GPU-thread wall time);
   CPU-demand waits0. PrepareForEnqueue currently drains all pending copies.
   Retire only the oldest prefix needed for admission, then validate ordered
   bytes, allocation lifetime and real GPU behavior. Not implemented yet.
2. Verify stable60FPS and game-time pacing on comparable gameplay, then >60 FPS
   timestep; submitted-frame cadence is not this acceptance.
3. Complete mission transitions and full in-game save/load regression.
4. Finish native UltraWide FOV/HUD/culling, CAS/FSR image, color modes/limiter
   acceptance and remaining launcher controller/HiDPI checks.
5. Promote only the verified runtime into the separate game/launcher package;
   current general producer remains opt-in/full/native FSI, low16MiB mirror
   excluded. Retain accepted package and protected original saves/config.

[Latest tested checkpoint](logs/acv-provider-range-rejection-20261026/checkpoint.json).

- Minimal-prefix admission implemented and built: byte/job caps now retire only
  the oldest jobs needed for admission, preserving later staged copies.
  Queue/retirement/bridge ASan/UBSan and real GPU12-copy/8-fault oracle PASS.
  Reproducible same-input GPU pressure test512x4MiB: median296.900→228.750ms,
  throughput1.298x; component result only. Candidate runtimeebf338a1/GPUc558cb37
  passed180s X11/full/native FSI20261027T043019725818Z, child0/fatal0, ended.
  This run stayed in menu;60Hz is not gameplay60FPS. Mission choice pending
  user input. Next independently remove allocator full drains when an older
  fitting occupied slot can be retired; no such slot-selection change yet.
  [Checkpoint](logs/acv-readback-minimal-admission-20261026/checkpoint.json).
