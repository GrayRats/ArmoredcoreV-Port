# Запуск ACV Windows-сборок в Wine и Faugus

Инструкция для smoke-проверки Windows-сборок на Linux. Это не нативная проверка
Windows: WSI, драйверы и игровой процесс нужно проверять на Windows с GPU.

## Что нужно

- `wine` (проверено: wine-staging 11.21)
- `faugus-launcher` (проверено: `/usr/bin/faugus-launcher`)
- Собранные exe:
  - лаунчер: `tmp/acv-trg-win-vulkan-build/acv-launcher.exe`
  - ReXGlue: `tools/rexglue-sdk/out/win-amd64/rexglue.exe`
- Windows-Python для бэкенда лаунчера. Используем официальный embeddable
  zip **Python 3.15.2** (последний патч линии 3.15 на python.org; на момент
  проверки опубликован также 3.16.0):
  `https://www.python.org/ftp/python/3.15.2/python-3.15.2-embed-amd64.zip`
  sha256 `d012f3aa29cf07436dae8aea1fec0fd195c2612a0b059b1f915ae0afb9ad1f62`.

Сборка лаунчера (каталог уже сконфигурирован под
`clang++ --target=x86_64-w64-mingw32`, Release):

```sh
cmake --build tmp/acv-trg-win-vulkan-build --parallel 4
```

### Подготовка Python в префиксе Wine (один раз)

```sh
mkdir -p tmp/acv-wine-vulkan-prefix/drive_c/python315
cd tmp/acv-wine-vulkan-prefix/drive_c/python315
python3 -c "import zipfile;zipfile.ZipFile('<путь>/python-3.15.2-embed-amd64.zip').extractall('.')"
```

Проверка: `WINEPREFIX=... wine 'C:\python315\python.exe' --version` → `Python 3.15.2`.

### Шрифты в префиксе (один раз)

Windows-ветка лаунчера ищет `C:\Windows\Fonts\segoeui.ttf`, затем `arial.ttf`
и `tahoma.ttf`. В свежем префиксе `Fonts` может быть пуст, и тогда интерфейс
показывает `??????` вместо кириллицы. Копируем шрифты Wine:

```sh
cp /usr/share/wine/fonts/{arial.ttf,tahoma.ttf,tahomabd.ttf} \
   tmp/acv-wine-vulkan-prefix/drive_c/windows/Fonts/
```

## 1. Smoke лаунчера (Vulkan, встроенный загрузчик Wine)

Отдельный каталог прогона и префикс Wine из `tmp/`. DLL `vulkan-1.dll` **не
копируем**, чтобы тестировать встроенный загрузчик Wine.

```sh
mkdir -p tmp/acv-wine-run-YYYYMMDD
cp tmp/acv-trg-win-vulkan-build/acv-launcher.exe tmp/acv-wine-run-YYYYMMDD/
cp tmp/acv-wine-run/libgcc_s_seh-1.dll tmp/acv-wine-run/libstdc++-6.dll \
   tmp/acv-wine-run/libwinpthread-1.dll tmp/acv-wine-run-YYYYMMDD/
cd tmp/acv-wine-run-YYYYMMDD
WINEPREFIX=$PWD/../acv-wine-vulkan-prefix WINEDEBUG=err+module \
  ACV_PYTHON='C:\python315\python.exe' \
  timeout 400 wine ./acv-launcher.exe --renderer vulkan --smoke-frames 1500 \
  > wine-vulkan-smoke.log 2>&1
echo "exit=$?"
```

Ожидаемый результат (проверено 2026-11-05):

- `launcher_ready ... renderer=vulkan`;
- `launcher_exit game_pid=0 backend_ready=1`;
- код выхода `0`.

Почему `--smoke-frames 1500`: бэкенд Python в Wine отвечает за секунды, а
80 кадров проходят быстрее. При 80 кадрах лаунчер выходит раньше `snapshot` и
показывает `backend_ready=0` (код `2`). Это не ошибка рендерера.

Почему `-X utf8`: лаунчер вызывает Python с `-X utf8 -I`. Без UTF-8 режима
Windows-Python читает файлы в `cp1252` и падает на кириллице
(`'charmap' codec can't encode`).

Линуксовая сборка проходит тот же smoke: `acv-trg-build/acv-launcher
--renderer {opengl,vulkan} --smoke-frames 80`, код `0`, `backend_ready=1`.

## 2. Запуск rexglue.exe

```sh
cd tmp/acv-wine-run-YYYYMMDD
cp ../../tools/rexglue-sdk/out/win-amd64/rexglue.exe \
   ../../tools/rexglue-sdk/out/win-amd64/librexruntime.dll \
   ../../tools/rexglue-sdk/out/win-amd64/librexgpu-xenos.dll .
WINEPREFIX=$PWD/../acv-wine-vulkan-prefix WINEDEBUG=-all \
  timeout 60 wine ./rexglue.exe --version
echo "exit=$?"
```

Наблюдено (2026-11-05):

- Без `librexruntime.dll` рядом с exe процесс не стартует: код `53`
  (`c0000135`, STATUS_DLL_NOT_FOUND). Нужны `librexruntime.dll` и
  `librexgpu-xenos.dll` из `out/win-amd64/`, плюс MinGW-рантайм
  (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`).
- С DLL процесс печатает версию (`0.10.0.2-dev.gc94f5eb`) и справку
  (`--help`), но **не завершается**: `timeout` убивает его с кодом `124`.
  Трасса (`WINEDEBUG=+thread,+process`) показывает, что после вывода версии
  идёт teardown потока (`RtlProcessFlsData`, `NtSetInformationThread`) и
  дальше ничего не происходит.
- Linux-сборка `out/linux-amd64/rexglue --version` завершается с кодом `0`,
  значит зависание специфично для Windows-сборки или для Wine.
- `acv-launcher.exe` в Wine завершается нормально, значит проблема не в
  самом Wine в целом.

Причина зависания не найдена. Для запуска в скриптах используйте `timeout` и
не полагайтесь на код выхода `--version`/`--help`.

## 3. Faugus

`faugus-run` в системе не установлен (`command not found`). Есть `umu-run` и
GUI `faugus-launcher`. Через GUI:

1. Запустить `faugus-launcher`.
2. Добавить `acv-launcher.exe` как приложение с runner Wine (wine-staging).
3. Указать префикс `tmp/acv-wine-vulkan-prefix`, тот же, что в шагах 1.
4. В переменные окружения добавить `ACV_PYTHON=C:\python315\python.exe`.
5. Аргументы: `--renderer vulkan --smoke-frames 1500`; если Faugus не даёт
   задать аргументы, запустить без smoke и закрыть окно вручную.

Этот вариант ещё не проверен: GUI Faugus и его названия полей нужно сверить
на месте.

## Чего эта проверка не доказывает

- Нативный WSI, presentation и resize на Windows.
- Работу Windows-драйверов и GPU.
- Запуск игры: кнопка Play в лаунчере намеренно заблокирована, пока не
  портирован runtime-мост.

См. также [PLAN_WINDOWS](kb:PLAN_WINDOWS) (раздел «Implementation
checkpoint — 2026-11-05»).
