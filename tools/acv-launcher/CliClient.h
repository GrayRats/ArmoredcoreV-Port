#pragma once
#include <QJsonDocument>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

// Locates the project and runs tools/native-rexglue/acv_profile_cli.py asynchronously.
class CliClient : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(const QJsonDocument &result, const QString &error)>;

    explicit CliClient(QString projectRoot, QString profilesRoot, QObject *parent = nullptr);

    // Walks up from `start` (and honours ACV_ROOT) until tools/native-rexglue/probe_linux.py exists.
    static QString findProjectRoot(const QString &start);

    QString projectRoot() const { return m_root; }
    QString profilesRoot() const { return m_profiles; }
    QString probeScript() const;
    QString cliScript() const;
    // Argument list for a normal play session; never joined into a shell string.
    QStringList sessionArguments(const QString &profileUuid, const QString &settingsFile,
                                 bool allowExperimental, const QString &executable = {},
                                 const QString &videoDriver = {}) const;

    void run(const QStringList &cliArguments, Callback callback);

    // Last JSON object of the output, or {"error": ...} text. Exposed for tests.
    static QJsonDocument parseOutput(const QByteArray &stdoutBytes, QString *error);

private:
    QString m_root;
    QString m_profiles;
};
