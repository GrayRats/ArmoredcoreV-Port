#include <QJsonObject>
#include <QComboBox>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

#include "CliClient.h"
#include "MainWindow.h"

class LauncherTest : public QObject {
    Q_OBJECT
private slots:
    void parsesOutput() {
        QString err;
        auto doc = CliClient::parseOutput("[{\"uuid\":\"a\"}]", &err);
        QVERIFY(err.isEmpty());
        QCOMPARE(doc.array().size(), 1);
        CliClient::parseOutput("{\"error\": \"profile is in use\"}", &err);
        QCOMPARE(err, QString("profile is in use"));
        CliClient::parseOutput("garbage", &err);
        QVERIFY(err.contains("Некорректный"));
    }
    void argumentsAreAList() {
        CliClient cli("/root dir", "/p; rm -rf /");
        const auto args = cli.sessionArguments("u u", "/s.json", true);
        QVERIFY(args.contains("/p; rm -rf /"));  // stays one argv entry, never shell-interpreted
        QVERIFY(args.contains("--allow-experimental"));
        QVERIFY(args.contains("--session"));
        QVERIFY(!args.contains("--seconds"));
    }
    void findsRoot() {
        const QString root = CliClient::findProjectRoot(QCoreApplication::applicationDirPath());
        QVERIFY(root.isEmpty() || QFileInfo::exists(root + "/tools/native-rexglue/probe_linux.py"));
        QVERIFY(CliClient::findProjectRoot("/nonexistent-dir").isEmpty() || !qEnvironmentVariable("ACV_ROOT").isEmpty());
    }
    // Drives the real window against a fake project root whose probe only records argv.
    void sessionEndToEnd() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString tools = root.path() + "/tools/native-rexglue";
        QVERIFY(QDir().mkpath(tools));
        for (const char *f : {"acv_profile_cli.py", "acv_profiles.py", "acv_settings.py", "acv_shm.py"})
            QVERIFY(QFile::copy(QString(NATIVE_DIR) + "/" + f, tools + "/" + f));
        const QString argvFile = root.path() + "/argv.txt";
        QFile probe(tools + "/probe_linux.py");
        QVERIFY(probe.open(QIODevice::WriteOnly));
        probe.write(("import sys\nopen(r'" + argvFile + "','w').write('\\n'.join(sys.argv[1:]))\n"
                     "sys.exit(int(open(r'" + root.path() + "/exit.txt').read()))\n").toUtf8());
        probe.close();
        QFile exitCode(root.path() + "/exit.txt");
        QVERIFY(exitCode.open(QIODevice::WriteOnly));
        exitCode.write("0");
        exitCode.close();

        // A usable build and game data, as the launcher requires before enabling Play.
        const QString build = root.path() + "/tmp/acv-rexglue-linux-indexed/out/build/linux-release";
        QVERIFY(QDir().mkpath(build));
        QVERIFY(QDir().mkpath(root.path() + "/tmp/iso_root"));
        for (const QString &f : {build + "/acv_linux_probe", build + "/librexruntime.so",
                                 root.path() + "/tmp/iso_root/default.xex"}) {
            QFile file(f);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("x");
        }
        qputenv("ACV_ROOT", root.path().toUtf8());
        const QString profiles = root.path() + "/profiles-root";
        CliClient cli(root.path(), profiles);
        QProcess create;
        create.start("python3", {"-I", cli.cliScript(), "--profiles-root", profiles, "create", "Tester"});
        QVERIFY(create.waitForFinished(10000));

        MainWindow window(&cli);
        window.show();
        auto *play = window.findChild<QPushButton *>("play");
        auto *state = window.findChild<QLabel *>("state");
        QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled(), 10000);
        QVERIFY(window.findChild<QPushButton *>("stop") != nullptr);
        QVERIFY(!window.findChild<QPushButton *>("stop")->isEnabled());  // nothing to stop yet
        auto *preset = window.findChild<QComboBox *>("preset");
        QVERIFY(preset != nullptr);
        preset->setCurrentIndex(preset->findData("quality"));
        emit preset->activated(preset->currentIndex());  // user-driven selection applies the preset
        QTest::mouseClick(play, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(state->text(), QString("Игра завершена"), 10000);
        QTRY_VERIFY(play->isEnabled());
        QFile argv(argvFile);
        QVERIFY(argv.open(QIODevice::ReadOnly));
        const QString recorded = QString::fromUtf8(argv.readAll());
        QVERIFY(recorded.contains("--session"));
        QVERIFY(recorded.contains("--profile"));
        QVERIFY(!recorded.contains("--seconds"));
        QFile settings(profiles + "/settings.json");
        QVERIFY(settings.open(QIODevice::ReadOnly));
        const auto doc = QJsonDocument::fromJson(settings.readAll()).object();
        QCOMPARE(doc.value("schema_version").toInt(), 1);
        QCOMPARE(doc.value("graphics").toObject().value("fps_limit").toInt(), 60);
        QCOMPARE(doc.value("graphics").toObject().value("anisotropy").toInt(), 16);  // from the Quality preset
        QCOMPARE(doc.value("graphics").toObject().value("dither").toBool(), true);

        // A failing start (e.g. profile busy, exit 2) reports the error and leaves Play usable.
        QVERIFY(exitCode.open(QIODevice::WriteOnly | QIODevice::Truncate));
        exitCode.write("2");
        exitCode.close();
        QTest::mouseClick(play, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(state->text().contains("код 2"), 10000);
        QTRY_VERIFY(play->isEnabled());
    }
};
QTEST_MAIN(LauncherTest)
#include "test_launcher.moc"
