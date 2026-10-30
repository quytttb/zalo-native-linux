#include "PreloadInventory.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace ZaloNative;

class PreloadInventoryTest : public QObject
{
    Q_OBJECT

private slots:
    void scansExposeAndChannels();
    void countsDynamicSyncCalls();
    void scansFilesAndFlagsTruncation();
    void reportsMissingScripts();
};

void PreloadInventoryTest::scansExposeAndChannels()
{
    const QString text = QStringLiteral(R"(
        const { contextBridge, ipcRenderer } = require('electron');
        contextBridge.exposeInMainWorld('$zapp', {
            path: () => ipcRenderer.sendSync('app:get-path'),
        });
        contextBridge.exposeInMainWorld("$zlogger", {
            write: (x) => ipcRenderer.send('log:write', x),
        });
        ipcRenderer.on('linux-theme-change', () => {});
        require('./build/Release/fake.node');
    )");

    const PreloadInventory inventory = PreloadInventory::scanText(text);
    QVERIFY(inventory.exposedApis.contains(QStringLiteral("$zapp")));
    QVERIFY(inventory.exposedApis.contains(QStringLiteral("$zlogger")));
    QVERIFY(inventory.syncChannels.contains(QStringLiteral("app:get-path")));
    QVERIFY(inventory.ipcChannels.contains(QStringLiteral("log:write")));
    QVERIFY(inventory.linuxChannels.contains(QStringLiteral("linux-theme-change")));
    QVERIFY(inventory.nativeRequires.contains(QStringLiteral("./build/Release/fake.node")));
    QCOMPARE(inventory.syncCallSites, 1);
    QCOMPARE(inventory.dynamicSyncCalls, 0);
    QVERIFY(!inventory.warnings.isEmpty());
}

void PreloadInventoryTest::countsDynamicSyncCalls()
{
    const QString text = QStringLiteral(R"(
        ipcRenderer.sendSync(channel, value);
        ipcRenderer.sendSync('literal:one');
        ipcRenderer.invoke(dynamicChannel);
    )");
    const PreloadInventory inventory = PreloadInventory::scanText(text);
    QCOMPARE(inventory.syncCallSites, 2);
    QCOMPARE(inventory.syncChannels.size(), 1);
    QCOMPARE(inventory.dynamicSyncCalls, 1);
    QVERIFY(inventory.warnings.join(QLatin1Char(' ')).contains(QStringLiteral("synchronous")));
}

void PreloadInventoryTest::scansFilesAndFlagsTruncation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/preload-render.js");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("contextBridge.exposeInMainWorld('$zfixture', {});\n");
    file.close();

    const PreloadInventory inventory = PreloadInventory::scanFiles({path}, 1024);
    QCOMPARE(inventory.scripts.size(), 1);
    QVERIFY(inventory.exposedApis.contains(QStringLiteral("$zfixture")));
    QVERIFY(!inventory.truncated);

    const PreloadInventory truncated = PreloadInventory::scanFiles({path}, 8);
    QCOMPARE(truncated.scripts.size(), 1);
    QVERIFY(!truncated.exposedApis.contains(QStringLiteral("$zfixture")));
    QVERIFY(truncated.truncated);
    QVERIFY(!truncated.warnings.isEmpty());
    QVERIFY(!truncated.toJson().isEmpty());
}

void PreloadInventoryTest::reportsMissingScripts()
{
    const PreloadInventory inventory = PreloadInventory::scanFiles({});
    QVERIFY(inventory.isEmpty());
    QVERIFY(inventory.warnings.join(QLatin1Char(' ')).contains(QStringLiteral("No preload")));
}

QTEST_GUILESS_MAIN(PreloadInventoryTest)
#include "test_preload.moc"
