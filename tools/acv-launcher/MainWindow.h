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
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(CliClient *cli, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildLaunchTab();
    QWidget *buildProfileTab();
    QWidget *buildOptionsTab(const QStringList &keys, const QHash<QString, QString> &titles);
    QWidget *buildGraphicsTab();
    QComboBox *optionCombo(const QString &key, bool editable = false);
    void populateBuilds();
    void refreshBuildInfo();
    void refreshShm();
    void cleanShm();
    void refreshProfileDetails();
    void loadCapabilities();
    void loadSettings();
    bool saveSettings();
    QJsonObject collectGraphics() const;
    QString settingsPath() const;
    void refreshProfiles(const QString &select = {});
    QString currentProfile() const;
    QString selectedListProfile() const;
    void updateAspectLabel();
    void applyPreset(const QString &name);
    void markCustomPreset();
    void setCombo(const QString &key, const QJsonValue &value);
    void setGameState(const QString &text, bool running);
    void startGame();
    void launchProcess(const QString &profile);
    void stopGame();
    void profileAction(const QStringList &args, const QString &okMessage, const QString &select = {});
    void log(const QString &line);

    CliClient *m_cli;
    QProcess *m_game = nullptr;
    bool m_buildOk = false;
    bool m_applyingPreset = false;
    QJsonObject m_capabilities;
    QHash<QString, QComboBox *> m_combos;
    QTabWidget *m_tabs;
    QComboBox *m_profileCombo;
    QComboBox *m_buildCombo;
    QComboBox *m_videoCombo;
    QComboBox *m_presetCombo = nullptr;
    QListWidget *m_profileList;
    QLabel *m_profileDetails;
    QLabel *m_buildInfo;
    QLabel *m_shmInfo;
    QPushButton *m_shmClean;
    qint64 m_shmFree = -1;
    QCheckBox *m_experimental;
    QPushButton *m_play;
    QPushButton *m_stop;
    QLabel *m_state;
    QLabel *m_aspect = nullptr;
    QPlainTextEdit *m_output;
    QJsonObject m_launcherState;
    QTimer *m_loadTimer = nullptr;
    qint64 m_startedMs = 0;
    QString m_lastRuntimeLog;
};
