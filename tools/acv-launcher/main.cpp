#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>

#include "CliClient.h"
#include "MainWindow.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("acv-launcher");
    QApplication::setOrganizationName("DEArmoredcore");

    const QString root = CliClient::findProjectRoot(QCoreApplication::applicationDirPath());
    if (root.isEmpty()) {
        QMessageBox::critical(nullptr, QObject::tr("ACV Launcher"),
                              QObject::tr("Не найден каталог проекта (tools/native-rexglue/probe_linux.py). "
                                          "Задайте переменную окружения ACV_ROOT."));
        return 2;
    }
    // Development layout (PLAN_GUI.MD B1). An installed package would use QStandardPaths instead.
    const QString profiles = qEnvironmentVariable("ACV_PROFILES_ROOT", root + "/tmp/acv-user-data");
    CliClient cli(root, profiles);
    MainWindow window(&cli);
    window.show();
    return app.exec();
}
