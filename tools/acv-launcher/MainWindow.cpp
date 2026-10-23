#include "MainWindow.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
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
#include <QUrl>
#include <QVBoxLayout>

#include "CliClient.h"

namespace {
constexpr int kSchemaVersion = 1;
const QStringList kResolutionPresets{"auto",      "1280x720",  "1920x1080", "2560x1440", "3840x2160",
                                     "1920x1200", "2560x1600", "2560x1080", "3440x1440"};

QString describe(const QJsonValue &v) {
    if (v.isBool()) return v.toBool() ? QObject::tr("Вкл") : QObject::tr("Выкл");
    if (v.isDouble()) return QString::number(v.toInt());
    const QString s = v.toString();
    static const QHash<QString, QString> names{
        {"auto", "Авто"}, {"game", "Как в игре"}, {"off", "Выкл"}, {"windowed", "В окне"},
        {"fullscreen", "Полный экран"}, {"rgb_full", "RGB Full Range"}, {"ycbcr444", "Y′CbCr 4:4:4"},
        {"ycbcr422", "Y′CbCr 4:2:2"}, {"ycbcr420", "Y′CbCr 4:2:0"}, {"bilinear", "Билинейное"},
        {"fxaa", "FXAA"}, {"fxaa_extreme", "FXAA (экстремальный)"}};
    return names.value(s, s);
}
}  // namespace

MainWindow::MainWindow(CliClient *cli, QWidget *parent) : QMainWindow(parent), m_cli(cli) {
    setWindowTitle(tr("Armored Core V — лаунчер"));
    resize(900, 640);
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    m_tabs = new QTabWidget;
    layout->addWidget(m_tabs, 1);
    m_state = new QLabel;
    m_state->setObjectName("state");
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
                                   {{"color_encoding", tr("Кодирование цвета")}, {"color_depth", tr("Глубина, бит на компонент")}}),
                   tr("Цвет"));
    m_tabs->addTab(buildOptionsTab({"render_scale", "anisotropy", "antialiasing", "dither", "output_effect"},
                                   {{"render_scale", tr("Масштаб внутреннего рендера")},
                                    {"anisotropy", tr("Анизотропная фильтрация")}, {"antialiasing", tr("Сглаживание")},
                                    {"dither", tr("Дизеринг")}, {"output_effect", tr("Масштабирование вывода")}}),
                   tr("Графика"));
    loadSettings();
    refreshProfiles(m_launcherState.value("profile").toString());
    updateAspectLabel();
    setGameState(tr("Готов к запуску"), false);
}

QString MainWindow::settingsPath() const { return m_cli->profilesRoot() + "/settings.json"; }

void MainWindow::log(const QString &line) {
    m_output->appendPlainText(line);
}

QWidget *MainWindow::buildLaunchTab() {
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    auto *form = new QFormLayout;
    m_profileCombo = new QComboBox;
    form->addRow(tr("Профиль"), m_profileCombo);
    form->addRow(tr("Сборка"), new QLabel(m_cli->probeScript()));
    m_experimental = new QCheckBox(tr("Разрешить экспериментальные настройки"));
    form->addRow(QString(), m_experimental);
    v->addLayout(form);
    m_play = new QPushButton(tr("Играть"));
    m_play->setObjectName("play");
    m_play->setMinimumHeight(48);
    connect(m_play, &QPushButton::clicked, this, &MainWindow::startGame);
    v->addWidget(m_play);
    m_output = new QPlainTextEdit;
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(5000);  // bounded so a long session cannot grow memory
    v->addWidget(m_output, 1);
    return w;
}

QWidget *MainWindow::buildProfileTab() {
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    m_profileList = new QListWidget;
    h->addWidget(m_profileList, 1);
    auto *col = new QVBoxLayout;
    h->addLayout(col);
    auto add = [&](const QString &text, auto slot) {
        auto *b = new QPushButton(text);
        connect(b, &QPushButton::clicked, this, slot);
        col->addWidget(b);
    };
    auto selected = [this]() {
        auto *item = m_profileList->currentItem();
        return item ? item->data(Qt::UserRole).toString() : QString();
    };
    add(tr("Создать…"), [this] {
        bool ok;
        const QString name = QInputDialog::getText(this, tr("Новый профиль"), tr("Имя (gamertag ≤ 15 символов):"),
                                                   QLineEdit::Normal, {}, &ok);
        if (ok && !name.trimmed().isEmpty()) profileAction({"create", name.trimmed()}, tr("Профиль создан"));
    });
    add(tr("Переименовать…"), [this, selected] {
        const QString id = selected();
        if (id.isEmpty()) return;
        bool ok;
        const QString name = QInputDialog::getText(this, tr("Переименование"), tr("Новое имя:"),
                                                   QLineEdit::Normal, {}, &ok);
        if (ok && !name.trimmed().isEmpty()) profileAction({"rename", id, name.trimmed()}, tr("Профиль переименован"), id);
    });
    add(tr("Открыть каталог"), [this, selected] {
        if (!selected().isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_cli->profilesRoot() + "/profiles/" + selected()));
    });
    add(tr("Резервная копия"), [this, selected] {
        if (!selected().isEmpty()) profileAction({"backup", selected()}, tr("Резервная копия создана"), selected());
    });
    add(tr("Восстановить…"), [this, selected] {
        const QString id = selected();
        if (id.isEmpty()) return;
        m_cli->run({"backups", id}, [this, id](const QJsonDocument &doc, const QString &err) {
            if (!err.isEmpty()) return setGameState(err, false);
            QStringList paths;
            for (const auto &entry : doc.array())
                if (entry.toObject().value("valid").toBool()) paths << entry.toObject().value("path").toString();
            if (paths.isEmpty()) return setGameState(tr("Нет проверенных резервных копий"), false);
            bool ok;
            const QString pick = QInputDialog::getItem(this, tr("Восстановление"), tr("Копия (текущие сейвы будут сохранены в quarantine):"),
                                                       paths, paths.size() - 1, false, &ok);
            if (ok) profileAction({"restore", id, pick}, tr("Сохранения восстановлены"), id);
        });
    });
    add(tr("Импортировать сейвы…"), [this, selected] {
        const QString id = selected();
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
    auto addItem = [&](const QJsonValue &value, const QString &status, const QString &reason) {
        QString text = key == "monitor"
                           ? (value.toInt() == 0 ? tr("По умолчанию") : tr("Монитор %1").arg(value.toInt()))
                           : describe(value);
        if (status == "experimental") text += tr(" (экспериментально)");
        if (status == "unavailable") text += tr(" (недоступно)");
        combo->addItem(text, value.toVariant());
        auto *item = model->item(combo->count() - 1);
        if (status == "unavailable") item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        if (!reason.isEmpty()) item->setToolTip(reason);
    };
    if (key == "resolution") {
        for (const QString &r : kResolutionPresets) addItem(r, "available", {});
    } else if (key == "monitor") {
        const int screens = QGuiApplication::screens().size();
        for (const auto &o : cap.value("options").toArray())
            if (o.toObject().value("value").toInt() <= screens)
                addItem(o.toObject().value("value"), "available", {});
    } else {
        for (const auto &o : cap.value("options").toArray()) {
            const QJsonObject opt = o.toObject();
            addItem(opt.value("value"), opt.value("status").toString(), opt.value("reason").toString());
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
    }
    if (keys.contains("color_encoding"))
        form->addRow(new QLabel(tr("Y′CbCr и 10 бит недоступны: подтверждённого механизма вывода в текущем окружении нет.")));
    return w;
}

void MainWindow::updateAspectLabel() {
    QString text = m_combos.value("resolution")->currentText();
    // Editable text shows the translated label; the stored value is the combo data when it matches a preset.
    const int index = m_combos.value("resolution")->currentIndex();
    if (index >= 0 && m_combos.value("resolution")->itemText(index) == text)
        text = m_combos.value("resolution")->itemData(index).toString();
    static const QRegularExpression re("^(\\d+)x(\\d+)$");
    const auto m = re.match(text);
    if (!m.hasMatch()) {
        m_aspect->setText(tr("Размер окна определяется автоматически."));
        return;
    }
    const double ratio = m.captured(1).toDouble() / m.captured(2).toDouble();
    QString family = ratio >= 2.2 ? tr("UltraWide 21:9") : ratio >= 1.7 ? tr("16:9") : ratio >= 1.55 ? tr("16:10") : tr("другое");
    QString note = tr("Изображение вписывается с сохранением пропорций; расширенный обзор (FOV) для UltraWide не реализован.");
    m_aspect->setText(tr("Соотношение сторон %1 (%2). %3").arg(ratio, 0, 'f', 3).arg(family, note));
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
    const QJsonObject g = doc.value("graphics").toObject();
    for (auto it = g.begin(); it != g.end(); ++it) {
        QComboBox *c = m_combos.value(it.key());
        if (!c) continue;
        const QVariant want = it.value().toVariant();
        int index = -1;
        for (int i = 0; i < c->count(); ++i)
            if (c->itemData(i) == want && c->itemData(i).typeId() == want.typeId()) index = i;
        if (index >= 0 && (c->model()->flags(c->model()->index(index, 0)) & Qt::ItemIsEnabled))
            c->setCurrentIndex(index);
        else if (it.key() == "resolution" && want.typeId() == QMetaType::QString && c->isEditable())
            c->setEditText(want.toString());
    }
}

bool MainWindow::saveSettings() {
    QDir().mkpath(m_cli->profilesRoot());
    m_launcherState["profile"] = currentProfile();
    m_launcherState["allow_experimental"] = m_experimental->isChecked();
    QJsonObject doc{{"schema_version", kSchemaVersion}, {"launcher", m_launcherState}, {"graphics", collectGraphics()}};
    QSaveFile f(settingsPath());  // written to a temp file and renamed on commit
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(doc).toJson());
    return f.commit();
}

QString MainWindow::currentProfile() const {
    return m_profileCombo ? m_profileCombo->currentData().toString() : QString();
}

void MainWindow::refreshProfiles(const QString &select) {
    m_cli->run({"list"}, [this, select](const QJsonDocument &doc, const QString &err) {
        if (!err.isEmpty()) return setGameState(err, false);
        m_profileCombo->clear();
        m_profileList->clear();
        for (const auto &v : doc.array()) {
            const QJsonObject o = v.toObject();
            const QString label = tr("%1 (gamertag %2, XUID %3)")
                                      .arg(o.value("display_name").toString(), o.value("gamertag").toString(),
                                           o.value("xuid").toString());
            m_profileCombo->addItem(o.value("display_name").toString(), o.value("uuid").toString());
            auto *item = new QListWidgetItem(label, m_profileList);
            item->setData(Qt::UserRole, o.value("uuid").toString());
        }
        const int i = m_profileCombo->findData(select);
        if (i >= 0) m_profileCombo->setCurrentIndex(i);
        if (m_profileList->count() && i >= 0) m_profileList->setCurrentRow(i);
        m_play->setEnabled(m_profileCombo->count() > 0 && !m_game);
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

void MainWindow::setGameState(const QString &text, bool running) {
    m_state->setText(text);
    if (m_play) m_play->setEnabled(!running && m_profileCombo && m_profileCombo->count() > 0);
}

void MainWindow::startGame() {
    if (m_game) return;
    const QString profile = currentProfile();
    if (profile.isEmpty()) return;
    if (!saveSettings()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось сохранить настройки"));
        return;
    }
    m_game = new QProcess(this);
    m_game->setProgram("python3");
    m_game->setArguments(m_cli->sessionArguments(profile, settingsPath(), m_experimental->isChecked()));
    connect(m_game, &QProcess::readyReadStandardOutput, this, [this] {
        log(QString::fromUtf8(m_game->readAllStandardOutput()).trimmed());
    });
    connect(m_game, &QProcess::readyReadStandardError, this, [this] {
        log(QString::fromUtf8(m_game->readAllStandardError()).trimmed());
    });
    connect(m_game, &QProcess::started, this, [this] { setGameState(tr("Игра запущена"), true); });
    connect(m_game, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        setGameState(tr("Ошибка запуска: python3 не найден"), false);
        m_game->deleteLater();
        m_game = nullptr;
    });
    connect(m_game, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        setGameState(status == QProcess::NormalExit && code == 0
                         ? tr("Игра завершена")
                         : tr("Игра завершилась с ошибкой (код %1)").arg(code),
                     false);
        m_game->deleteLater();
        m_game = nullptr;
        m_play->setEnabled(m_profileCombo->count() > 0);
    });
    setGameState(tr("Запуск…"), true);
    m_game->start();
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
