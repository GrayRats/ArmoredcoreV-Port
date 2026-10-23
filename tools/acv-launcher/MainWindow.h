#pragma once
#include <QHash>
#include <QJsonObject>
#include <QMainWindow>
#include <QProcess>

class CliClient;
class QComboBox;
class QCheckBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(CliClient *cli, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    struct Profile { QString uuid, display, gamertag, pilot, xuid; };

    QWidget *buildLaunchTab();
    QWidget *buildProfileTab();
    QWidget *buildOptionsTab(const QStringList &keys, const QHash<QString, QString> &titles);
    QComboBox *optionCombo(const QString &key, bool editable = false);
    void loadCapabilities();
    void loadSettings();
    bool saveSettings();
    QJsonObject collectGraphics() const;
    QString settingsPath() const;
    void refreshProfiles(const QString &select = {});
    QString currentProfile() const;
    void updateAspectLabel();
    void setGameState(const QString &text, bool running);
    void startGame();
    void profileAction(const QStringList &args, const QString &okMessage, const QString &select = {});
    void log(const QString &line);

    CliClient *m_cli;
    QProcess *m_game = nullptr;
    QJsonObject m_capabilities;
    QHash<QString, QComboBox *> m_combos;
    QList<Profile> m_profiles;
    QTabWidget *m_tabs;
    QComboBox *m_profileCombo;
    QListWidget *m_profileList;
    QCheckBox *m_experimental;
    QPushButton *m_play;
    QLabel *m_state;
    QLabel *m_aspect;
    QPlainTextEdit *m_output;
    QJsonObject m_launcherState;
};
