#include "RuntimeLocator.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace ZaloNative;

class RuntimeTest : public QObject
{
    Q_OBJECT

private slots:
    void locatesCheckedInRuntime();
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
    QCOMPARE(json.value(QStringLiteral("entryHtml")).toString(), QStringLiteral("index.html"));
    QCOMPARE(json.value(QStringLiteral("nativeModules")).toArray().size(),
             manifest->nativeModules.size());
    QVERIFY(QJsonDocument(json).toJson().size() > 0);
}

void RuntimeTest::reportsMissingRuntime()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString error;
    const auto manifest = RuntimeLocator::locate(dir.path(), &error);
    QVERIFY(!manifest.has_value());
    QVERIFY2(error.contains(QStringLiteral("no app.asar")), qPrintable(error));
    QCOMPARE(RuntimeLocator::candidateAsars(dir.path()).size(), 0);
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
