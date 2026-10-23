# DEArmoredcoreV

Инструменты исследования и native Linux recompilation Armored Core V (Xbox 360).
Репозиторий содержит небольшие исходники tools, Python, shell scripts и Markdown.

- `tools/native-rexglue`: host adapters, Vulkan diagnostics, SDK patches, tests и build/probe scripts.
- `tools/native-decomp`: ограниченные ручные модели и проверки по retail evidence.
- `tools/decomp-logging`: SQLite execution logger; зависимости устанавливаются отдельно.
- `scripts`: обёртки запуска инструментов; `docs`: текущее состояние, план и результаты.

Игровые файлы, ISO/XEX, извлечённые ресурсы, generated translation, бинарники,
логи, RenderDoc captures, кеши, tmp/temp и сторонние SDK/recompiler checkouts
хранятся локально. В репозитории нет исполняемой игры или готового пакета данных.

Для продолжения читать [docs/README.md](docs/README.md) и
[docs/X360_PROJECT_STATE.md](docs/X360_PROJECT_STATE.md). Отчёты содержат ссылки
на локальные доказательства в logs/tmp и исторические абсолютные пути рабочей
машины; эти артефакты не публикуются, поэтому свежий clone не содержит их.

Для локального профиля скопировать `tools/native-rexglue/acv.config.example`
в `tools/native-rexglue/acv.config`. Личный профиль исключён из Git.
ReXGlue SDK 0.10.0 и остальные внешние инструменты устанавливаются отдельно;
проверять pinned revisions и пути из профильной документации. Это исследовательский
workspace, build scripts пока содержат привязки к локальному окружению.

Проверенный ACV profile использует `readback_resolve=full`. Private
`ACV_DEFER_RESOLVES_TEST` — эксперимент с явно заданным диапазоном; GPU-вариант
прошёл ограниченный игровой byte test; полные lifetime/coherency гарантии ещё не приняты.
Он выключен по умолчанию и не заменяет рабочий full readback.

Перед коммитом выполнить `python3 scripts/check_repository_payload.py`.
Ограничение отслеживаемого файла: 512 КиБ; разрешены только исходники tools,
scripts, docs и корневые Markdown/правила. Лицензии сторонних компонентов
сохраняются в соответствующих каталогах; общей лицензии на их код это не меняет.
