# ACV-Linux 1.1 — source release notes

## English

This GitHub release publishes the source, scripts, configuration examples and
project documentation used to build and operate the ACV-Linux standalone
package. It does **not** include the game executable, ReXGlue runtime/GPU
binaries, copyrighted game data, XEX/ISO files, keys, saves or user profiles.
The release tag is a repository source snapshot, not a replacement binary for
`ACV-Linux-release-1.0`.

### Launcher and package functionality

- TRG launcher with six pages for display, graphics, colour, controls, profiles
  and game/data paths, connected to the shared Python settings and profile backend.
- Profile UUID/XUID handling, writer locks, backup/restore and XDG storage;
  launcher exit does not terminate a running game or release its profile lock.
- Standalone Linux startup from any working directory, bundled runtime/library
  selection and game-data path validation.
- Resolution presets with aspect groups, 30/60/120 FPS swap caps, VSync,
  display and output-depth reporting, keyboard/mouse bindings, and installation
  from an Xbox ISO into a separate staging destination.
- Vulkan FSI/full rendering path, texture-cache limits, bounded archive
  read-ahead, UltraWide FOV/HUD support and the accepted side-frustum correction.
- Optional CAS/FSR1 controls are present, but image-quality effects have not
  been accepted. FSR2/3 and YCbCr output are unavailable. 10-bit output depends
  on the selected display surface.

### Source and diagnostics

This tag also contains the bounded Linux CPU/GPU/IP capture helper and opt-in
swap-timing trace patch, with English/Russian project status and FPS reports.
The 2026-10-29 sample recorded 29.02 guest swaps/s and 92.4% device-wide GPU
utilization. The mission and route were not identified, and FPS above 30 has not
been achieved or accepted. The diagnostic trace patch is source only; this
release does not claim it was integrated into or built with the packaged game.

See `README.md`, `README.RU.MD`, `docs/LAUNCHER_PACKAGE.md`,
`report-fps.md` and `perf/baseline.md` for setup, scope and verification limits.

## Русский

Этот выпуск GitHub публикует исходники, скрипты, примеры настроек и
документацию, использованные для сборки и работы отдельного пакета ACV-Linux.
В выпуск **не входят** исполняемый файл игры, бинарники ReXGlue runtime/GPU,
защищённые игровые данные, XEX/ISO, ключи, сейвы и профили пользователя.
Тег содержит снимок исходного репозитория и не заменяет бинарный пакет
`ACV-Linux-release-1.0`.

### Лаунчер и функции пакета

- TRG-лаунчер с шестью страницами: экран, графика, цвет, управление, профили,
  пути к игре и данным. Настройки и профили используют общую Python-схему.
- Профили UUID/XUID, блокировка записи, проверяемые backup/restore и хранение
  по XDG. Закрытие лаунчера не завершает игру и не снимает блокировку профиля,
  пока игра работает.
- Отдельный Linux-пакет запускается из любого рабочего каталога, проверяет
  пути к данным и выбирает библиотеки из комплекта.
- Группы готовых разрешений по соотношению сторон, лимиты swap 30/60/120 FPS,
  VSync, сведения о дисплее и глубине цвета, клавиатура/мышь, установка из Xbox
  ISO в отдельный staging-каталог.
- Vulkan FSI/full, лимиты кэша текстур, ограниченная фоновая подгрузка архивов,
  UltraWide FOV/HUD и принятая коррекция боковых границ frustum.
- Настройки CAS/FSR1 присутствуют, но качество изображения ими не принято.
  FSR2/3 и YCbCr недоступны. Вывод 10 бит зависит от формата поверхности.

### Исходники и диагностика

Тег также содержит инструмент ограниченного сбора Linux CPU/GPU/IP и opt-in
патч трассировки swap с отчётами на английском и русском языках. Замер
2026-10-29 показал 29,02 guest swap/с и 92,4% загрузки GPU по устройству.
Миссия и маршрут не указаны; FPS выше 30 не достигнут и не принят. Патч
диагностики опубликован как исходник; выпуск не утверждает, что он встроен или
собран с игровым пакетом.

Инструкции, границы проверки и известные ограничения описаны в `README.md`,
`README.RU.MD`, `docs/LAUNCHER_PACKAGE.md`, `report-fps.md` и `perf/baseline.md`.
