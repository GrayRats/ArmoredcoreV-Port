#include "MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "CliClient.h"

namespace {
constexpr int kSchemaVersion = 1;
const QStringList kResolutionPresets{"auto",      "1280x720",  "1920x1080", "2560x1440", "3840x2160",
                                     "1920x1200", "2560x1600", "2560x1080", "3440x1440"};
// Presets contain only settings that are available (not experimental) in the pinned runtime.
const QJsonObject kCompatibility{{"render_scale", 1}, {"anisotropy", "game"}, {"antialiasing", "off"},
                                 {"dither", false},   {"output_effect", "bilinear"}, {"vsync", true}};
const QJsonObject kQuality{{"render_scale", 1}, {"anisotropy", 16}, {"antialiasing", "off"},
                           {"dither", true},    {"output_effect", "bilinear"}, {"vsync", true}};

QString describe(const QJsonValue &v) {
    if (v.isBool()) return v.toBool() ? QObject::tr("Вкл") : QObject::tr("Выкл");
    if (v.isDouble())
        return v.toDouble() == v.toInt() ? QString::number(v.toInt()) : QString::number(v.toDouble());
    const QString s = v.toString();
    static const QHash<QString, QString> names{
        {"auto", "Авто"}, {"game", "Как в игре"}, {"off", "Выкл"}, {"default", "По умолчанию"},
        {"windowed", "В окне"}, {"fullscreen", "Полный экран"}, {"rgb_full", "RGB Full Range"},
        {"ycbcr444", "Y′CbCr 4:4:4"}, {"ycbcr422", "Y′CbCr 4:2:2"}, {"ycbcr420", "Y′CbCr 4:2:0"},
        {"bilinear", "Билинейное"}, {"cas", "CAS (резкость)"}, {"fsr", "FSR 1"}, {"fsr2", "FSR 2"},
        {"fsr3", "FSR 3"}, {"fxaa", "FXAA"}, {"fxaa_extreme", "FXAA (экстремальный)"}};
    return names.value(s, s);
}

QString screenLabel(int number, const QScreen *screen) {
    return QObject::tr("Монитор %1: %2 (%3×%4, %5 Гц, масштаб %6)")
        .arg(number).arg(screen->name()).arg(screen->geometry().width()).arg(screen->geometry().height())
        .arg(qRound(screen->refreshRate())).arg(screen->devicePixelRatio());
}
}  // namespace

MainWindow::MainWindow(CliClient *cli, QWidget *parent) : QMainWindow(parent), m_cli(cli) {
    setWindowTitle(tr("Armored Core V — лаунчер"));
    resize(980, 700);
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    m_tabs = new QTabWidget;
    layout->addWidget(m_tabs, 1);
    m_state = new QLabel;
    m_state->setObjectName("state");
    m_state->setWordWrap(true);
    layout->addWidget(m_state);
    setCentralWidget(central);

    loadCapabilities();
    m_tabs->addTab(buildLaunchTab(), tr("Запуск"));
    m_tabs->addTab(buildProfileTab(), tr("Профиль и сохранения"));
    m_tabs->addTab(buildOptionsTab({"window_mode", "monitor", "resolution", "vsync", "fps_limit"},
                                   {{"window_mode", tr("Режим окна")}, {"monitor", tr("Монитор")},
                                    {"resolution", tr("Разрешение")}, {"vsync", tr("Вертикальная синхронизация")},
                                    {"fps_limit", tr("Лимит кадров в секунду")}}),
                   tr("Экран"));
    m_tabs->addTab(buildOptionsTab({"color_encoding", "color_depth"},
                                   {{"color_encoding", tr("Кодирование цвета")},
                                    {"color_depth", tr("Глубина, бит на компонент")}}),
                   tr("Цвет"));
    m_tabs->addTab(buildGraphicsTab(), tr("Графика"));
    populateBuilds();
    loadSettings();
    for (auto it = m_combos.begin(); it != m_combos.end(); ++it)
        connect(it.value(), &QComboBox::currentIndexChanged, this, &MainWindow::markCustomPreset);
    refreshProfiles(m_launcherState.value("profile").toString());
    refreshBuildInfo();
    refreshShm();
    updateAspectLabel();
    setGameState(tr("Готов к запуску"), false);
}

QString MainWindow::settingsPath() const { return m_cli->profilesRoot() + "/settings.json"; }

void MainWindow::log(const QString &line) {
    m_output->appendPlainText(line);
    // Persistent trace of launcher decisions (launches, checks, exits) for diagnosing "nothing happened".
    QDir().mkpath(m_cli->projectRoot() + "/logs");
    QFile f(m_cli->projectRoot() + "/logs/acv-launcher.log");
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write((QDateTime::currentDateTime().toString(Qt::ISODateWithMs) + " " + line + "\n").toUtf8());
}

// ---------------------------------------------------------------------------------------------
// Tabs

QWidget *MainWindow::buildLaunchTab() {
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    auto *form = new QFormLayout;
    m_profileCombo = new QComboBox;
    form->addRow(tr("Профиль"), m_profileCombo);

    auto *buildRow = new QHBoxLayout;
    m_buildCombo = new QComboBox;
    m_buildCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    buildRow->addWidget(m_buildCombo, 1);
    auto *browse = new QPushButton(tr("Обзор…"));
    browse->setToolTip(tr("Другой acv_linux_probe; файл должен лежать внутри tmp/ проекта"));
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Исполняемый файл игры"),
                                                          m_cli->projectRoot() + "/tmp");
        if (file.isEmpty()) return;
        if (!QFileInfo(file).absoluteFilePath().startsWith(m_cli->projectRoot() + "/tmp/")) {
            QMessageBox::warning(this, tr("Сборка"), tr("Файл должен находиться внутри каталога tmp/ проекта."));
            return;
        }
        int i = m_buildCombo->findData(file);
        if (i < 0) {
            m_buildCombo->addItem(QFileInfo(file).dir().dirName(), file);
            i = m_buildCombo->count() - 1;
        }
        m_buildCombo->setCurrentIndex(i);
    });
    buildRow->addWidget(browse);
    form->addRow(tr("Сборка"), buildRow);
    m_buildInfo = new QLabel;
    m_buildInfo->setWordWrap(true);
    form->addRow(QString(), m_buildInfo);
    connect(m_buildCombo, &QComboBox::currentIndexChanged, this, &MainWindow::refreshBuildInfo);

    auto *shmRow = new QHBoxLayout;
    m_shmInfo = new QLabel;
    m_shmInfo->setWordWrap(true);
    shmRow->addWidget(m_shmInfo, 1);
    m_shmClean = new QPushButton(tr("Очистить"));
    m_shmClean->setToolTip(tr("Удаляет только ваши файлы xenia_memory_* в /dev/shm, которые не использует ни один запущенный процесс"));
    m_shmClean->setEnabled(false);
    connect(m_shmClean, &QPushButton::clicked, this, &MainWindow::cleanShm);
    shmRow->addWidget(m_shmClean);
    form->addRow(tr("Память игры"), shmRow);

    m_videoCombo = new QComboBox;
    // Observed 2026-10-24: under SDL's Wayland backend the game window is never shown and cannot be
    // closed (no frame is presented); through X11/XWayland it appears normally. X11 is therefore the default.
    m_videoCombo->addItem(tr("X11 (XWayland) — рекомендуется"), "x11");
    m_videoCombo->addItem(tr("Wayland (окно может не появиться)"), "wayland");
    m_videoCombo->addItem(tr("Автоматически (выбор SDL)"), "auto");
    form->addRow(tr("Видеодрайвер"), m_videoCombo);

    m_experimental = new QCheckBox(tr("Разрешить экспериментальные настройки"));
    form->addRow(QString(), m_experimental);
    v->addLayout(form);

    auto *buttons = new QHBoxLayout;
    m_play = new QPushButton(tr("Играть"));
    m_play->setObjectName("play");
    m_play->setMinimumHeight(48);
    connect(m_play, &QPushButton::clicked, this, &MainWindow::startGame);
    buttons->addWidget(m_play, 3);
    m_stop = new QPushButton(tr("Остановить"));
    m_stop->setObjectName("stop");
    m_stop->setMinimumHeight(48);
    m_stop->setEnabled(false);
    m_stop->setToolTip(tr("Корректно завершает игру (SIGTERM). Не нажимайте во время сохранения."));
    connect(m_stop, &QPushButton::clicked, this, &MainWindow::stopGame);
    buttons->addWidget(m_stop, 1);
    auto *logs = new QPushButton(tr("Каталог логов"));
    logs->setMinimumHeight(48);
    connect(logs, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_cli->projectRoot() + "/logs/acv-native-linux"));
    });
    buttons->addWidget(logs, 1);
    v->addLayout(buttons);
    auto *keys = new QLabel(tr("Клавиши (профиль клавиатура+мышь): Enter — Start, Пробел — A, Backspace/Esc — B, R — X, F — Y, "
                               "Tab — Back, WASD — левый стик, I/J/K/L — правый стик, ЛКМ/ПКМ — триггеры, "
                               "F8 — мышь-обзор вкл/выкл, Esc — освободить мышь. Если окно пустое или «NOW LOADING» — "
                               "подождите, затем нажмите Enter."));
    keys->setWordWrap(true);
    v->addWidget(keys);

    m_output = new QPlainTextEdit;
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(5000);  // bounded so a long session cannot grow memory
    v->addWidget(m_output, 1);
    return w;
}

QWidget *MainWindow::buildProfileTab() {
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    auto *left = new QVBoxLayout;
    m_profileList = new QListWidget;
    left->addWidget(m_profileList, 1);
    m_profileDetails = new QLabel;
    m_profileDetails->setWordWrap(true);
    m_profileDetails->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *box = new QGroupBox(tr("Сведения"));
    auto *boxLayout = new QVBoxLayout(box);
    boxLayout->addWidget(m_profileDetails);
    left->addWidget(box);
    h->addLayout(left, 1);
    connect(m_profileList, &QListWidget::currentRowChanged, this, &MainWindow::refreshProfileDetails);

    auto *col = new QVBoxLayout;
    h->addLayout(col);
    auto add = [&](const QString &text, auto slot) {
        auto *b = new QPushButton(text);
        connect(b, &QPushButton::clicked, this, slot);
        col->addWidget(b);
    };
    add(tr("Создать…"), [this] {
        bool ok;
        const QString name = QInputDialog::getText(this, tr("Новый профиль"), tr("Имя (gamertag ≤ 15 символов):"),
                                                   QLineEdit::Normal, {}, &ok);
        if (ok && !name.trimmed().isEmpty()) profileAction({"create", name.trimmed()}, tr("Профиль создан"));
    });
    add(tr("Переименовать…"), [this] {
        const QString id = selectedListProfile();
        if (id.isEmpty()) return;
        bool ok;
        const QString name = QInputDialog::getText(this, tr("Переименование"),
                                                   tr("Новое имя в лаунчере (gamertag и сохранения не меняются):"),
                                                   QLineEdit::Normal, {}, &ok);
        if (ok && !name.trimmed().isEmpty()) profileAction({"rename", id, name.trimmed()}, tr("Профиль переименован"), id);
    });
    add(tr("Изменить gamertag…"), [this] {
        const QString id = selectedListProfile();
        if (id.isEmpty()) return;
        bool ok;
        const QString name = QInputDialog::getText(this, tr("Gamertag"), tr("Новый gamertag (≤ 15 символов, действует со следующего запуска):"),
                                                   QLineEdit::Normal, {}, &ok);
        if (ok && !name.trimmed().isEmpty())
            profileAction({"set-names", id, "--gamertag", name.trimmed()}, tr("Gamertag изменён"), id);
    });
    add(tr("Изменить имя пилота…"), [this] {
        const QString id = selectedListProfile();
        if (id.isEmpty()) return;
        bool ok;
        const QString name = QInputDialog::getText(this, tr("Имя пилота"),
                                                   tr("Имя для нового ввода в игре (имя внутри существующего сейва не меняется):"),
                                                   QLineEdit::Normal, {}, &ok);
        if (ok && !name.trimmed().isEmpty())
            profileAction({"set-names", id, "--pilot", name.trimmed()}, tr("Имя пилота изменено"), id);
    });
    add(tr("Открыть каталог"), [this] {
        if (!selectedListProfile().isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_cli->profilesRoot() + "/profiles/" + selectedListProfile()));
    });
    add(tr("Резервная копия"), [this] {
        if (!selectedListProfile().isEmpty())
            profileAction({"backup", selectedListProfile()}, tr("Резервная копия создана"), selectedListProfile());
    });
    add(tr("Восстановить…"), [this] {
        const QString id = selectedListProfile();
        if (id.isEmpty()) return;
        m_cli->run({"backups", id}, [this, id](const QJsonDocument &doc, const QString &err) {
            if (!err.isEmpty()) return setGameState(err, false);
            QStringList paths;
            for (const auto &entry : doc.array())
                if (entry.toObject().value("valid").toBool()) paths << entry.toObject().value("path").toString();
            if (paths.isEmpty()) return setGameState(tr("Нет проверенных резервных копий"), false);
            bool ok;
            const QString pick = QInputDialog::getItem(this, tr("Восстановление"),
                                                       tr("Копия (текущие сейвы будут сохранены в quarantine):"),
                                                       paths, paths.size() - 1, false, &ok);
            if (ok) profileAction({"restore", id, pick}, tr("Сохранения восстановлены"), id);
        });
    });
    add(tr("Импортировать сейвы…"), [this] {
        const QString id = selectedListProfile();
        if (id.isEmpty()) return;
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Каталог user_data_root"));
        if (!dir.isEmpty()) profileAction({"import", id, dir}, tr("Сохранения импортированы"), id);
    });
    col->addStretch();
    return w;
}

QComboBox *MainWindow::optionCombo(const QString &key, bool editable) {
    auto *combo = new QComboBox;
    combo->setEditable(editable);
    const QJsonObject cap = m_capabilities.value(key).toObject();
    auto *model = new QStandardItemModel(combo);
    combo->setModel(model);
    auto addItem = [&](const QJsonValue &value, const QString &status, const QString &reason, const QString &text) {
        QString label = text;
        if (status == "experimental") label += tr(" (экспериментально)");
        if (status == "unavailable") label += tr(" (недоступно)");
        combo->addItem(label, value.toVariant());
        auto *item = model->item(combo->count() - 1);
        if (status == "unavailable") item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        if (!reason.isEmpty()) item->setToolTip(reason);
    };
    if (key == "resolution") {
        for (const QString &r : kResolutionPresets) addItem(r, "available", {}, describe(r));
    } else if (key == "monitor") {
        addItem(0, "available", {}, tr("По умолчанию"));
        // The SDK counts monitors from 1 with the primary first; list the primary screen first.
        QList<QScreen *> screens = QGuiApplication::screens();
        screens.removeAll(QGuiApplication::primaryScreen());
        screens.prepend(QGuiApplication::primaryScreen());
        for (int i = 0; i < screens.size(); ++i) addItem(i + 1, "available", {}, screenLabel(i + 1, screens[i]));
    } else {
        for (const auto &o : cap.value("options").toArray()) {
            const QJsonObject opt = o.toObject();
            addItem(opt.value("value"), opt.value("status").toString(), opt.value("reason").toString(),
                    describe(opt.value("value")));
        }
    }
    m_combos.insert(key, combo);
    return combo;
}

QWidget *MainWindow::buildOptionsTab(const QStringList &keys, const QHash<QString, QString> &titles) {
    auto *w = new QWidget;
    auto *form = new QFormLayout(w);
    for (const QString &key : keys) {
        QComboBox *combo = optionCombo(key, key == "resolution");
        form->addRow(titles.value(key), combo);
        if (key == "resolution") {
            connect(combo, &QComboBox::currentTextChanged, this, &MainWindow::updateAspectLabel);
            connect(combo, &QComboBox::currentIndexChanged, this, &MainWindow::updateAspectLabel);
        }
    }
    if (keys.contains("resolution")) {
        m_aspect = new QLabel;
        m_aspect->setWordWrap(true);
        form->addRow(QString(), m_aspect);
        auto *note = new QLabel(tr("Лимит 120/144/165 FPS недоступен: игровой шаг не отвязан от кадров. "
                                   "Лимит 60 FPS задаёт темп показа кадров и нужен при выключенном VSync."));
        note->setWordWrap(true);
        form->addRow(QString(), note);
    }
    if (keys.contains("color_encoding")) {
        auto *note = new QLabel(tr("Y′CbCr и 10 бит недоступны: подтверждённого механизма вывода в текущем окружении нет."));
        note->setWordWrap(true);
        form->addRow(note);
    }
    return w;
}

QWidget *MainWindow::buildGraphicsTab() {
    auto *w = new QWidget;
    auto *form = new QFormLayout(w);
    auto *presetRow = new QHBoxLayout;
    m_presetCombo = new QComboBox;
    m_presetCombo->setObjectName("preset");
    m_presetCombo->addItem(tr("Совместимость"), "compat");
    m_presetCombo->addItem(tr("Качество"), "quality");
    m_presetCombo->addItem(tr("Пользовательский"), "custom");
    m_presetCombo->setCurrentIndex(2);
    connect(m_presetCombo, &QComboBox::activated, this, [this](int i) {
        const QString name = m_presetCombo->itemData(i).toString();
        if (name != "custom") applyPreset(name);
    });
    presetRow->addWidget(m_presetCombo, 1);
    auto *reset = new QPushButton(tr("Сбросить графику"));
    reset->setToolTip(tr("Возвращает значения по умолчанию; прогресс игры не затрагивается"));
    connect(reset, &QPushButton::clicked, this, [this] { applyPreset("compat"); });
    presetRow->addWidget(reset);
    form->addRow(tr("Пресет"), presetRow);

    const QList<QPair<QString, QString>> rows{
        {"render_scale", tr("Масштаб внутреннего рендера")}, {"anisotropy", tr("Анизотропная фильтрация")},
        {"antialiasing", tr("Сглаживание")}, {"dither", tr("Дизеринг")},
        {"output_effect", tr("Масштабирование вывода")}, {"sharpness", tr("Резкость CAS")}};
    for (const auto &r : rows) form->addRow(r.second, optionCombo(r.first));
    auto *note = new QLabel(tr("«Экспериментально» — включается флажком на вкладке «Запуск». CAS и FSR требуют сборку с "
                               "FidelityFX; FSR 2/3 недоступны. Масштаб рендера и FXAA пока не проверены на картинке."));
    note->setWordWrap(true);
    form->addRow(note);
    return w;
}

// ---------------------------------------------------------------------------------------------
// Builds, presets, settings

void MainWindow::populateBuilds() {
    const QString tmp = m_cli->projectRoot() + "/tmp";
    const QList<QPair<QString, QString>> builds{
        {tr("Принятая сборка"), tmp + "/acv-rexglue-linux-indexed/out/build/linux-release/acv_linux_probe"},
        {tr("Тест: лимитер кадров + XUID"), tmp + "/acv-limiter-test/acv_linux_probe"},
        {tr("Тест: FidelityFX (CAS/FSR)"), tmp + "/acv-ffx-test/acv_linux_probe"}};
    for (const auto &b : builds)
        if (QFileInfo::exists(b.second)) m_buildCombo->addItem(b.first, b.second);
    const QString saved = m_launcherState.value("build").toString();
    if (!saved.isEmpty() && QFileInfo::exists(saved)) {
        int i = m_buildCombo->findData(saved);
        if (i < 0) {
            m_buildCombo->addItem(QFileInfo(saved).dir().dirName(), saved);
            i = m_buildCombo->count() - 1;
        }
        m_buildCombo->setCurrentIndex(i);
    }
}

void MainWindow::refreshBuildInfo() {
    m_buildOk = false;
    const QString exe = m_buildCombo->currentData().toString();
    if (exe.isEmpty()) {
        m_buildInfo->setText(tr("Сборка не найдена. Выполните build_linux.py или выберите файл вручную."));
        if (m_play) m_play->setEnabled(false);
        return;
    }
    QStringList args{"features", "--executable", exe};
    m_cli->run(args, [this, exe](const QJsonDocument &doc, const QString &err) {
        if (exe != m_buildCombo->currentData().toString()) return;  // selection changed meanwhile
        if (!err.isEmpty()) {
            m_buildInfo->setText(err);
            return;
        }
        const QJsonObject o = doc.object();
        QStringList features;
        for (const auto &f : o.value("features").toArray()) features << f.toString();
        QStringList problems;
        if (!o.value("exists").toBool()) problems << tr("исполняемый файл отсутствует");
        if (!o.value("game_data").toBool()) problems << tr("нет игровых данных (tmp/iso_root/default.xex)");
        if (o.value("runtime_sha256").isNull()) problems << tr("рядом нет librexruntime.so");
        m_buildOk = problems.isEmpty();
        QString text = tr("Возможности сборки: %1.").arg(features.isEmpty() ? tr("базовые") : features.join(", "));
        if (!o.value("runtime_sha256").isNull())
            text += tr(" runtime SHA-256: %1…").arg(o.value("runtime_sha256").toString().left(12));
        if (!problems.isEmpty()) text += tr("\nНе хватает: %1.").arg(problems.join("; "));
        m_buildInfo->setText(text);
        if (!m_game) m_play->setEnabled(m_buildOk && m_profileCombo->count() > 0);
    });
}

void MainWindow::refreshShm() {
    m_cli->run({"shm-status"}, [this](const QJsonDocument &doc, const QString &err) {
        if (!err.isEmpty()) {
            m_shmInfo->setText(err);
            return;
        }
        const QJsonObject o = doc.object();
        m_shmFree = qint64(o.value("free").toDouble());
        const int orphans = o.value("orphans").toInt();
        const double gb = 1024.0 * 1024.0 * 1024.0;
        QString text = tr("/dev/shm: свободно %1 ГБ из %2 ГБ.").arg(m_shmFree / gb, 0, 'f', 1)
                           .arg(o.value("total").toDouble() / gb, 0, 'f', 1);
        if (orphans > 0)
            text += tr(" Осиротевших файлов игры: %1 (%2 ГБ) — их оставили аварийно завершённые запуски старого рантайма.")
                        .arg(orphans).arg(o.value("orphan_bytes").toDouble() / gb, 0, 'f', 1);
        m_shmInfo->setText(text);
        m_shmClean->setEnabled(orphans > 0 && !m_game);
    });
}

void MainWindow::cleanShm() {
    if (m_game) return;
    if (QMessageBox::question(this, tr("Очистка памяти игры"),
                              tr("Будут удалены только файлы xenia_memory_* в /dev/shm, принадлежащие вам и не используемые "
                                 "ни одним процессом. Прогресс, профили и сохранения не затрагиваются. Продолжить?")) != QMessageBox::Yes)
        return;
    m_cli->run({"shm-clean"}, [this](const QJsonDocument &doc, const QString &err) {
        if (!err.isEmpty()) {
            QMessageBox::warning(this, tr("Ошибка"), err);
            return;
        }
        log(tr("[лаунчер] очистка /dev/shm: удалено файлов %1").arg(doc.object().value("removed").toInt()));
        refreshShm();
    });
}

void MainWindow::setCombo(const QString &key, const QJsonValue &value) {
    QComboBox *c = m_combos.value(key);
    if (!c) return;
    const QVariant want = value.toVariant();
    for (int i = 0; i < c->count(); ++i) {
        if (c->itemData(i) == want && c->itemData(i).typeId() == want.typeId() &&
            (c->model()->flags(c->model()->index(i, 0)) & Qt::ItemIsEnabled)) {
            c->setCurrentIndex(i);
            return;
        }
    }
    if (key == "resolution" && want.typeId() == QMetaType::QString && c->isEditable()) c->setEditText(want.toString());
}

void MainWindow::applyPreset(const QString &name) {
    m_applyingPreset = true;
    const QJsonObject &p = name == "quality" ? kQuality : kCompatibility;
    for (auto it = p.begin(); it != p.end(); ++it) setCombo(it.key(), it.value());
    setCombo("sharpness", "default");
    m_applyingPreset = false;
    const int i = m_presetCombo->findData(name);
    if (i >= 0) m_presetCombo->setCurrentIndex(i);
}

void MainWindow::markCustomPreset() {
    if (m_applyingPreset || !m_presetCombo) return;
    // Any manual change that no longer equals a preset makes the profile "custom".
    const QJsonObject now = collectGraphics();
    for (const QString &name : {QString("compat"), QString("quality")}) {
        const QJsonObject &p = name == "quality" ? kQuality : kCompatibility;
        bool same = now.value("sharpness") == "default";
        for (auto it = p.begin(); same && it != p.end(); ++it) same = now.value(it.key()) == it.value();
        if (same) {
            m_presetCombo->setCurrentIndex(m_presetCombo->findData(name));
            return;
        }
    }
    m_presetCombo->setCurrentIndex(m_presetCombo->findData("custom"));
}

void MainWindow::updateAspectLabel() {
    if (!m_aspect) return;
    QComboBox *c = m_combos.value("resolution");
    QString text = c->currentText();
    const int index = c->currentIndex();
    // The editable box shows the translated label; the stored value is the item data when it matches.
    if (index >= 0 && c->itemText(index) == text) text = c->itemData(index).toString();
    static const QRegularExpression re("^(\\d+)x(\\d+)$");
    const auto m = re.match(text);
    if (!m.hasMatch()) {
        m_aspect->setText(tr("Размер окна определяется автоматически."));
        return;
    }
    const double ratio = m.captured(1).toDouble() / m.captured(2).toDouble();
    const QString family = ratio >= 2.2 ? tr("UltraWide 21:9") : ratio >= 1.7 ? tr("16:9")
                         : ratio >= 1.55 ? tr("16:10") : tr("другое");
    m_aspect->setText(tr("Соотношение сторон %1 (%2). Изображение вписывается с сохранением пропорций; "
                         "расширенный обзор (FOV) для UltraWide не реализован.")
                          .arg(ratio, 0, 'f', 3).arg(family));
}

void MainWindow::loadCapabilities() {
    // Synchronous at start-up only: a short local read of static data.
    QProcess p;
    p.start("python3", {"-I", m_cli->cliScript(), "capabilities"});
    p.waitForFinished(10000);
    QString err;
    const QJsonDocument doc = CliClient::parseOutput(p.readAllStandardOutput(), &err);
    if (doc.isObject()) m_capabilities = doc.object();
}

QJsonObject MainWindow::collectGraphics() const {
    QJsonObject g;
    for (auto it = m_combos.begin(); it != m_combos.end(); ++it) {
        QComboBox *c = it.value();
        QVariant data = c->currentData();
        if (it.key() == "resolution") {
            const QString t = c->currentText().trimmed();
            const int i = c->findText(t);
            data = i >= 0 ? c->itemData(i) : QVariant(t);
        }
        g.insert(it.key(), QJsonValue::fromVariant(data));
    }
    return g;
}

void MainWindow::loadSettings() {
    QFile f(settingsPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject doc = QJsonDocument::fromJson(f.readAll()).object();
    if (doc.value("schema_version").toInt() != kSchemaVersion) {
        log(tr("Файл настроек имеет неподдерживаемую версию схемы; используются значения по умолчанию."));
        return;
    }
    m_launcherState = doc.value("launcher").toObject();
    m_experimental->setChecked(m_launcherState.value("allow_experimental").toBool());
    const int vi = m_videoCombo->findData(m_launcherState.value("video_driver").toString());
    if (vi >= 0) m_videoCombo->setCurrentIndex(vi);
    const QByteArray geometry = QByteArray::fromBase64(m_launcherState.value("geometry").toString().toLatin1());
    if (!geometry.isEmpty()) restoreGeometry(geometry);
    m_applyingPreset = true;
    const QJsonObject g = doc.value("graphics").toObject();
    for (auto it = g.begin(); it != g.end(); ++it) setCombo(it.key(), it.value());
    m_applyingPreset = false;
    markCustomPreset();
}

bool MainWindow::saveSettings() {
    QDir().mkpath(m_cli->profilesRoot());
    m_launcherState["profile"] = currentProfile();
    m_launcherState["build"] = m_buildCombo->currentData().toString();
    m_launcherState["allow_experimental"] = m_experimental->isChecked();
    m_launcherState["video_driver"] = m_videoCombo->currentData().toString();
    m_launcherState["geometry"] = QString::fromLatin1(saveGeometry().toBase64());
    QJsonObject doc{{"schema_version", kSchemaVersion}, {"launcher", m_launcherState}, {"graphics", collectGraphics()}};
    QSaveFile f(settingsPath());  // written to a temp file and renamed on commit
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(doc).toJson());
    return f.commit();
}

// ---------------------------------------------------------------------------------------------
// Profiles

QString MainWindow::currentProfile() const {
    return m_profileCombo ? m_profileCombo->currentData().toString() : QString();
}

QString MainWindow::selectedListProfile() const {
    auto *item = m_profileList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void MainWindow::refreshProfileDetails() {
    auto *item = m_profileList->currentItem();
    if (!item) {
        m_profileDetails->setText(tr("Профиль не выбран"));
        return;
    }
    const QJsonObject o = item->data(Qt::UserRole + 1).toJsonObject();
    m_profileDetails->setText(
        tr("Имя в лаунчере: %1\nGamertag: %2\nИмя пилота: %3\nUUID: %4\nXUID профиля: %5\nЯзык: %6\n"
           "Каталог: %7/profiles/%4")
            .arg(o.value("display_name").toString(), o.value("gamertag").toString(), o.value("pilot_name").toString(),
                 o.value("uuid").toString(), o.value("xuid").toString())
            .arg(o.value("language").toInt())
            .arg(m_cli->profilesRoot()));
}

void MainWindow::refreshProfiles(const QString &select) {
    m_cli->run({"list"}, [this, select](const QJsonDocument &doc, const QString &err) {
        if (!err.isEmpty()) return setGameState(err, false);
        m_profileCombo->clear();
        m_profileList->clear();
        for (const auto &v : doc.array()) {
            const QJsonObject o = v.toObject();
            const QString label = tr("%1 (gamertag %2)")
                                      .arg(o.value("display_name").toString(), o.value("gamertag").toString());
            m_profileCombo->addItem(o.value("display_name").toString(), o.value("uuid").toString());
            auto *item = new QListWidgetItem(label, m_profileList);
            item->setData(Qt::UserRole, o.value("uuid").toString());
            item->setData(Qt::UserRole + 1, o);
        }
        const int i = m_profileCombo->findData(select);
        if (i >= 0) m_profileCombo->setCurrentIndex(i);
        m_profileList->setCurrentRow(i >= 0 ? i : (m_profileList->count() ? 0 : -1));
        refreshProfileDetails();
        if (!m_game) m_play->setEnabled(m_buildOk && m_profileCombo->count() > 0);
        if (m_profileCombo->count() == 0) setGameState(tr("Создайте профиль на вкладке «Профиль и сохранения»"), false);
    });
}

void MainWindow::profileAction(const QStringList &args, const QString &okMessage, const QString &select) {
    m_cli->run(args, [this, okMessage, select](const QJsonDocument &, const QString &err) {
        if (!err.isEmpty()) {
            QMessageBox::warning(this, tr("Ошибка"), err);
            return;
        }
        setGameState(okMessage, m_game != nullptr);
        refreshProfiles(select.isEmpty() ? currentProfile() : select);
    });
}

// ---------------------------------------------------------------------------------------------
// Game session

void MainWindow::setGameState(const QString &text, bool running) {
    m_state->setText(text);
    if (m_play) m_play->setEnabled(!running && m_buildOk && m_profileCombo && m_profileCombo->count() > 0);
    if (m_stop) m_stop->setEnabled(m_game != nullptr);
}

void MainWindow::startGame() {
    log(tr("[лаунчер] нажато «Играть»; игра уже запущена: %1; профиль: %2")
            .arg(m_game ? tr("да") : tr("нет"), currentProfile()));
    if (m_game) return;
    const QString profile = currentProfile();
    if (profile.isEmpty()) return;
    if (!saveSettings()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось сохранить настройки"));
        return;
    }
    // Validate the saved settings against the chosen build first, so an incompatible pair is
    // reported in plain words instead of as a probe usage error.
    QStringList check{"check", "--settings", settingsPath()};
    if (m_experimental->isChecked()) check << "--allow-experimental";
    if (!m_buildCombo->currentData().toString().isEmpty()) check << "--executable" << m_buildCombo->currentData().toString();
    if (m_shmFree >= 0 && m_shmFree < qint64(6) * 1024 * 1024 * 1024) {
        // The guest address space needs up to ~5 GB of tmpfs; without it the game aborts with SIGBUS.
        QMessageBox::warning(this, tr("Мало памяти в /dev/shm"),
                             tr("В /dev/shm свободно менее 6 ГБ — игра может завершиться аварийно при старте. "
                                "Нажмите «Очистить» на вкладке «Запуск» или освободите место."));
        return;
    }
    setGameState(tr("Проверка настроек…"), true);
    m_cli->run(check, [this, profile](const QJsonDocument &, const QString &err) {
        log(tr("[лаунчер] проверка настроек: %1").arg(err.isEmpty() ? tr("успешно") : err));
        if (!err.isEmpty()) {
            setGameState(tr("Настройки несовместимы с выбранной сборкой"), false);
            QMessageBox::warning(this, tr("Запуск невозможен"),
                                 tr("Выбранная сборка не поддерживает эти настройки:\n\n%1\n\n"
                                    "Выберите другую сборку или измените настройки.").arg(err));
            return;
        }
        launchProcess(profile);
    });
}

void MainWindow::launchProcess(const QString &profile) {
    if (m_game) return;
    m_game = new QProcess(this);
    m_game->setProgram("python3");
    m_game->setArguments(m_cli->sessionArguments(profile, settingsPath(), m_experimental->isChecked(),
                                                 m_buildCombo->currentData().toString(),
                                                 m_videoCombo->currentData().toString()));
    connect(m_game, &QProcess::readyReadStandardOutput, this, [this] {
        log(QString::fromUtf8(m_game->readAllStandardOutput()).trimmed());
    });
    connect(m_game, &QProcess::readyReadStandardError, this, [this] {
        log(QString::fromUtf8(m_game->readAllStandardError()).trimmed());
    });
    connect(m_game, &QProcess::started, this, [this] {
        m_startedMs = QDateTime::currentMSecsSinceEpoch();
        if (!m_loadTimer) {
            m_loadTimer = new QTimer(this);
            m_loadTimer->setInterval(1000);
            connect(m_loadTimer, &QTimer::timeout, this, [this] {
                if (!m_game) return;
                const qint64 s = (QDateTime::currentMSecsSinceEpoch() - m_startedMs) / 1000;
                // The first start is quiet for 1-2 minutes (observed ~75 s) before frames appear.
                setGameState(s < 150 ? tr("Игра запущена, идёт загрузка… %1 с (обычно 1–2 минуты, окно может быть пустым)").arg(s)
                                     : tr("Игра работает %1 мин %2 с").arg(s / 60).arg(s % 60),
                             true);
            });
        }
        m_loadTimer->start();
        setGameState(tr("Игра запущена, идёт загрузка… (обычно 1–2 минуты, окно может быть пустым)"), true);
    });
    connect(m_game, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        m_game->deleteLater();
        m_game = nullptr;
        setGameState(tr("Ошибка запуска: python3 не найден"), false);
    });
    connect(m_game, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        log(tr("[лаунчер] процесс завершён, код %1, %2").arg(code).arg(status == QProcess::NormalExit ? tr("штатно") : tr("аварийно")));
        if (m_loadTimer) m_loadTimer->stop();
        QTimer::singleShot(500, this, &MainWindow::refreshShm);
        m_game->deleteLater();
        m_game = nullptr;
        setGameState(status == QProcess::NormalExit && code == 0
                         ? tr("Игра завершена")
                         : tr("Игра завершилась с ошибкой (код %1)").arg(code),
                     false);
    });
    setGameState(tr("Запуск…"), true);
    log(tr("[лаунчер] запуск: python3 %1").arg(m_game->arguments().join(' ')));
    m_game->start();
}

void MainWindow::stopGame() {
    if (!m_game) return;
    if (QMessageBox::question(this, tr("Остановить игру"),
                              tr("Игра будет завершена принудительно. Если она сейчас сохраняется, "
                                 "сохранение может не завершиться. Продолжить?")) != QMessageBox::Yes)
        return;
    if (!m_game) return;  // finished while the dialog was open
    setGameState(tr("Остановка…"), true);
    m_game->terminate();  // SIGTERM: probe_linux.py forwards it to the game and releases the profile lock
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_game) {
        // Closing must not cut a running game: it may be writing a save.
        QMessageBox::information(this, tr("Игра запущена"), tr("Закройте игру, затем лаунчер."));
        event->ignore();
        return;
    }
    saveSettings();
    event->accept();
}
