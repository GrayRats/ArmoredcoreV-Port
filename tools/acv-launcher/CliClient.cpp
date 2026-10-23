#include "CliClient.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QProcess>

CliClient::CliClient(QString projectRoot, QString profilesRoot, QObject *parent)
    : QObject(parent), m_root(std::move(projectRoot)), m_profiles(std::move(profilesRoot)) {}

QString CliClient::findProjectRoot(const QString &start) {
    const QString env = qEnvironmentVariable("ACV_ROOT");
    QStringList candidates;
    if (!env.isEmpty()) candidates << env;
    candidates << start;
    for (const QString &candidate : candidates) {
        QDir dir(candidate);
        do {
            if (QFileInfo::exists(dir.filePath("tools/native-rexglue/probe_linux.py")))
                return dir.absolutePath();
        } while (dir.cdUp());
    }
    return {};
}

QString CliClient::probeScript() const { return m_root + "/tools/native-rexglue/probe_linux.py"; }
QString CliClient::cliScript() const { return m_root + "/tools/native-rexglue/acv_profile_cli.py"; }

QStringList CliClient::sessionArguments(const QString &profileUuid, const QString &settingsFile,
                                        bool allowExperimental) const {
    QStringList args{"-I", probeScript(), "--session", "--profile", profileUuid,
                     "--profiles-root", m_profiles};
    if (!settingsFile.isEmpty()) args << "--settings" << settingsFile;
    if (allowExperimental) args << "--allow-experimental";
    return args;
}

QJsonDocument CliClient::parseOutput(const QByteArray &out, QString *error) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(out, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        *error = QStringLiteral("Некорректный ответ инструмента профилей: %1").arg(parseError.errorString());
        return {};
    }
    if (doc.isObject() && doc.object().contains("error")) {
        *error = doc.object().value("error").toString();
        return {};
    }
    error->clear();
    return doc;
}

void CliClient::run(const QStringList &cliArguments, Callback callback) {
    auto *process = new QProcess(this);
    QStringList args{"-I", cliScript(), "--profiles-root", m_profiles};
    args += cliArguments;
    connect(process, &QProcess::finished, this, [process, callback](int, QProcess::ExitStatus status) {
        QString error;
        QJsonDocument doc;
        if (status != QProcess::NormalExit) {
            error = QStringLiteral("Инструмент профилей завершился аварийно");
        } else {
            doc = parseOutput(process->readAllStandardOutput(), &error);
            if (!error.isEmpty() && error.startsWith("Некорректный")) {
                const QString err = QString::fromUtf8(process->readAllStandardError()).trimmed();
                if (!err.isEmpty()) error += ": " + err;
            }
        }
        process->deleteLater();
        callback(doc, error);
    });
    connect(process, &QProcess::errorOccurred, this, [process, callback](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            process->deleteLater();
            callback({}, QStringLiteral("Не удалось запустить python3"));
        }
    });
    process->start("python3", args);
}
