#include "RuntimeLocator.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace ZaloNative;

namespace {

void copyDirectory(const QString &source, const QString &destination)
{
    QDir().mkpath(destination);
    QDirIterator iterator(source,
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        const QString relative = QDir(source).relativeFilePath(path);
        const QString target = QDir(destination).absoluteFilePath(relative);
        if (info.isDir()) {
            QDir().mkpath(target);
        } else if (info.isFile()) {
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile::copy(path, target);
        }
    }
}

} // namespace

class RuntimeTest : public QObject
{
    Q_OBJECT

private slots:
    void locatesCheckedInRuntime();
    void locatesExtractedAppDirectory();
    void prefersExtractedAppOverLauncherAsar();
    void reportsMissingRuntime();
    void acceptsAnAsarPathDirectly();
    void honoursEnvironmentOverride();
};

void RuntimeTest::locatesCheckedInRuntime()
{
    const QString fixture = QStringLiteral(ZALO_TEST_FIXTURES "/sample.asar");
    QVERIFY2(QFileInfo::exists(fixture), qPrintable(fixture));

    QString error;
    const auto manifest = RuntimeLocator::locate(fixture, &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));

    QVERIFY(manifest->isValid());
    QVERIFY(manifest->isRuntimeCore());
    QCOMPARE(manifest->sourceKind, QStringLiteral("asar"));
    QCOMPARE(manifest->name, QStringLiteral("sample-zalo-runtime"));
    QCOMPARE(manifest->appVersion, QStringLiteral("9.9.9-demo"));
    QCOMPARE(manifest->electronVersion, QStringLiteral("39.0.0-demo"));
    QCOMPARE(manifest->mainEntry, QStringLiteral("main.js"));
    QCOMPARE(manifest->entryHtml, QStringLiteral("index.html"));
    QCOMPARE(manifest->runtimeRoot, QFileInfo(fixture).absolutePath());
    QVERIFY(manifest->nativeModules.contains(QStringLiteral("fake-native")));
    QVERIFY(manifest->warnings.isEmpty());

    const QJsonObject json = manifest->toJson();
    QVERIFY(json.value(QStringLiteral("valid")).toBool());
    QVERIFY(json.value(QStringLiteral("runtimeCore")).toBool());
    QCOMPARE(json.value(QStringLiteral("entryHtml")).toString(), QStringLiteral("index.html"));
    QCOMPARE(json.value(QStringLiteral("nativeModules")).toArray().size(),
             manifest->nativeModules.size());
    QVERIFY(!json.value(QStringLiteral("sourceVerified")).toBool());
    QVERIFY(QJsonDocument(json).toJson().size() > 0);
}

void RuntimeTest::locatesExtractedAppDirectory()
{
    const QString fixture = QStringLiteral(ZALO_TEST_FIXTURES "/app-runtime");
    QVERIFY2(QFileInfo::exists(fixture), qPrintable(fixture));

    QString error;
    const auto manifest = RuntimeLocator::locate(fixture, &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));

    QVERIFY(manifest->isValid());
    QVERIFY(manifest->isRuntimeCore());
    QCOMPARE(manifest->sourceKind, QStringLiteral("app-dir"));
    QCOMPARE(manifest->appVersion, QStringLiteral("9.9.9-fixture"));
    QCOMPARE(manifest->entryHtml, QStringLiteral("pc-dist/index.html"));
    QCOMPARE(manifest->mainEntry, QStringLiteral("bootstrap.js"));
    QCOMPARE(manifest->buildCommit, QStringLiteral("fixture"));
    QCOMPARE(manifest->appDir, QFileInfo(fixture).absoluteFilePath());

    QVERIFY(manifest->preloads.exposedApis.contains(QStringLiteral("$zapp")));
    QVERIFY(manifest->preloads.exposedApis.contains(QStringLiteral("$zlogger")));
    QCOMPARE(manifest->preloads.syncCallSites, 1);
    QCOMPARE(manifest->preloads.dynamicSyncCalls, 0);
    QVERIFY(manifest->preloads.syncChannels.contains(QStringLiteral("app:get-path")));
    QVERIFY(manifest->preloads.linuxChannels.contains(QStringLiteral("linux-theme-change")));
    QVERIFY(manifest->preloads.nativeRequires.contains(QStringLiteral("./build/Release/fake.node")));
    QVERIFY(manifest->nativeLibraries.contains(QStringLiteral("native/libfake.so")));

    const QJsonObject json = manifest->toJson();
    QCOMPARE(json.value(QStringLiteral("sourceKind")).toString(), QStringLiteral("app-dir"));
    QVERIFY(json.value(QStringLiteral("preloads")).toObject()
                .value(QStringLiteral("exposedApis")).toArray().size() >= 2);
}

void RuntimeTest::prefersExtractedAppOverLauncherAsar()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/AppDir")));
    QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/resources")));

    copyDirectory(QStringLiteral(ZALO_TEST_FIXTURES "/app-runtime"),
                  root.path() + QStringLiteral("/AppDir/app"));
    QVERIFY(QFile::copy(QStringLiteral(ZALO_TEST_FIXTURES "/sample.asar"),
                        root.path() + QStringLiteral("/resources/app.asar")));

    QString error;
    const auto manifest = RuntimeLocator::locate(root.path(), &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    QCOMPARE(manifest->sourceKind, QStringLiteral("app-dir"));
    QCOMPARE(manifest->appVersion, QStringLiteral("9.9.9-fixture"));
    QCOMPARE(manifest->entryHtml, QStringLiteral("pc-dist/index.html"));
}

void RuntimeTest::reportsMissingRuntime()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString error;
    const auto manifest = RuntimeLocator::locate(dir.path(), &error);
    QVERIFY(!manifest.has_value());
    QVERIFY2(error.contains(QStringLiteral("runtime app directory")), qPrintable(error));
    QCOMPARE(RuntimeLocator::candidateAsars(dir.path()).size(), 0);
    QCOMPARE(RuntimeLocator::candidateAppDirs(dir.path()).size(), 0);
}

void RuntimeTest::acceptsAnAsarPathDirectly()
{
    const QString fixture = QStringLiteral(ZALO_TEST_FIXTURES "/sample.asar");
    const QStringList candidates = RuntimeLocator::candidateAsars(fixture);
    QCOMPARE(candidates.size(), 1);
    QCOMPARE(candidates.first(), QFileInfo(fixture).absoluteFilePath());
}

void RuntimeTest::honoursEnvironmentOverride()
{
    const QByteArray previous = qgetenv("ZALO_RUNTIME_DIR");
    const QString expected = QStringLiteral("/tmp/zalo-native-test-runtime");
    qputenv("ZALO_RUNTIME_DIR", expected.toUtf8());
    QCOMPARE(RuntimeLocator::defaultRoot(), expected);

    qunsetenv("ZALO_RUNTIME_DIR");
    QVERIFY(RuntimeLocator::defaultRoot().endsWith(
        QStringLiteral("/.local/share/zalo-native/runtime")));

    if (!previous.isEmpty())
        qputenv("ZALO_RUNTIME_DIR", previous);
}

QTEST_GUILESS_MAIN(RuntimeTest)
#include "test_runtime.moc"
