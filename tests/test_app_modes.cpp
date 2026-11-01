#include "AppController.h"
#include "BridgeRouter.h"
#include "NativeQrLogin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

using namespace ZaloNative;

class AppModesTest : public QObject
{
    Q_OBJECT
private slots:
    void nativeRejectsDemoAndFakeSend();
    void demoRemainsLocalAndExplicit();
};

void AppModesTest::nativeRejectsDemoAndFakeSend()
{
    AppController::Options options;
    AppController app(options);
    QCOMPARE(app.appMode(), QStringLiteral("native"));
    QVERIFY(app.nativeMode());
    QCOMPARE(app.nativeQrLogin()->state(), QStringLiteral("idle"));
    QCOMPARE(app.conversationCount(), 0);
    app.reloadDemo();
    QCOMPARE(app.conversationCount(), 0);

    QVERIFY(!app.bridgeObject());
    auto *router = app.findChild<BridgeRouter *>();
    QVERIFY(router);
    const QJsonObject fixture{
        {QStringLiteral("conversations"), QJsonArray{
            QJsonObject{{QStringLiteral("id"), QStringLiteral("fictional")},
                        {QStringLiteral("title"), QStringLiteral("Fixture")}}}},
        {QStringLiteral("messages"), QJsonObject{}},
    };
    const QJsonObject request{
        {QStringLiteral("kind"), QStringLiteral("call")},
        {QStringLiteral("path"), QStringLiteral("state.publish")},
        {QStringLiteral("args"), QJsonArray{fixture}},
    };
    QVERIFY(router->dispatch(request).value(QStringLiteral("ok")).toBool());
    QCOMPARE(app.conversationCount(), 0);
    QVERIFY(!app.sendMessage(QStringLiteral("fictional message")));
    QCOMPARE(app.messageCount(), 0);
}

void AppModesTest::demoRemainsLocalAndExplicit()
{
    AppController::Options options;
    options.demo = true;
    AppController app(options);
    QCOMPARE(app.appMode(), QStringLiteral("demo"));
    QVERIFY(app.demoMode());
    QVERIFY(app.bridgeObject());
    app.reloadDemo();
    QVERIFY(app.conversationCount() > 0);
    QVERIFY(!app.currentId().isEmpty());
    const int before = app.messageCount();
    QVERIFY(app.sendMessage(QStringLiteral("fictional message")));
    QCOMPARE(app.messageCount(), before + 1);
    QCOMPARE(app.nativeQrLogin()->state(), QStringLiteral("idle"));
}

QTEST_GUILESS_MAIN(AppModesTest)
#include "test_app_modes.moc"
