# ACV-Linux 1.1 — release notes

## English

**Build and install:** [Linux/Windows x64-v2 guide](https://github.com/GrayRats/ArmoredcoreV-Port/blob/main/Install.MD) · [Build tools package](https://github.com/GrayRats/ArmoredcoreV-Port/pkgs/container/acv-build-tools). The guide distinguishes the Linux application, Windows SDK build and the remaining native Windows ACV work.

Updated build sources are attached as `ACV-Linux-1.1-build-sources.zip` and `.tar.gz`; `BUILD-SOURCE-COMMIT.txt` identifies their source commit and `ACV-Linux-1.1-build-sources.SHA256SUMS.txt` contains checksums. They include the installation guides, CMake templates and scripts; the guide lists additional generated/SDK inputs required for a full game rebuild.

This GitHub release publishes the source, scripts, configuration examples and
project documentation used to build and operate ACV-Linux. It also attaches a
split archive of the existing local `ACV-Linux-release-1.0` Linux package. That
package remains version 1.0: its game ELF, ReXGlue runtime/GPU libraries, game
data, DLC and bundled media were not rebuilt for this release. The uploader
attests that public redistribution is authorized; the provenance note records
that attestation and the package manifest scope.

GitHub requires each release asset to stay below 2 GiB, so the 3.16 GiB
compressed package archive is split into two parts. Download both
`ACV-Linux-release-1.0.tar.zst.part-00` and
`ACV-Linux-release-1.0.tar.zst.part-01`, then follow
`ACV-Linux-release-1.0-PROVENANCE.md` to verify and reassemble them. This is the
existing 1.0 build attached to release 1.1, not a new FPS build.

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

**Сборка и установка:** [инструкция Linux/Windows x64-v2](https://github.com/GrayRats/ArmoredcoreV-Port/blob/main/Install.RU.MD) · [пакет инструментов](https://github.com/GrayRats/ArmoredcoreV-Port/pkgs/container/acv-build-tools). В инструкции отдельно отмечены Linux-приложение, сборка Windows SDK и оставшаяся работа по нативному Windows-порту ACV.

Актуальные сборочные исходники приложены как `ACV-Linux-1.1-build-sources.zip` и `.tar.gz`; `BUILD-SOURCE-COMMIT.txt` указывает исходный коммит, а `ACV-Linux-1.1-build-sources.SHA256SUMS.txt` содержит хеши. Внутри находятся Install-инструкции, CMake-шаблоны и скрипты; дополнительные generated/SDK входы полной пересборки игры перечислены в инструкции.

Этот выпуск GitHub публикует исходники, скрипты, примеры настроек и документацию
ACV-Linux. Также приложен архив существующего локального Linux-пакета
`ACV-Linux-release-1.0`, разбитый на части. Это по-прежнему версия 1.0: игровой
ELF, библиотеки ReXGlue runtime/GPU, игровые данные, DLC и видео не пересобирались
для данного релиза. Публикатор подтвердил наличие разрешения на публичное
распространение; provenance-файл описывает это подтверждение и манифест пакета.

GitHub ограничивает каждый release asset размером менее 2 ГиБ, поэтому сжатый
архив 3,16 ГиБ разделён на две части. Скачайте
`ACV-Linux-release-1.0.tar.zst.part-00` и
`ACV-Linux-release-1.0.tar.zst.part-01`, затем следуйте инструкции проверки и
объединения в `ACV-Linux-release-1.0-PROVENANCE.md`. Это существующая сборка 1.0,
приложенная к релизу 1.1, а не новая FPS-сборка.

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
