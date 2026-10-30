#include "BridgeRouter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

using namespace ZaloNative;

class BridgeRouterTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesProtocolSpec();
    void answersPingAndInfo();
    void publishesAndSnapshotsState();
    void rejectsUnknownAndMalformedRequests();
    void enforcesPayloadLimit();
    void bridgesJsonThroughBridgeObject();
};

void BridgeRouterTest::exposesProtocolSpec()
{
    BridgeRouter router;
    const QJsonObject spec = router.spec();
    QCOMPARE(spec.value(QStringLiteral("protocol")).toString(),
             QStringLiteral("zalonative.bridge/1"));
    QCOMPARE(spec.value(QStringLiteral("maxPayloadBytes")).toInt(),
             BridgeRouter::kMaxPayloadBytes);

    const QJsonObject methods = spec.value(QStringLiteral("methods")).toObject();
    for (const QString &name : {QStringLiteral("app.ping"), QStringLiteral("state.publish"),
                                QStringLiteral("state.snapshot"), QStringLiteral("log.write")}) {
        QVERIFY2(methods.contains(name), qPrintable(name));
    }
}

void BridgeRouterTest::answersPingAndInfo()
{
    BridgeRouter router;
    const QJsonObject ping = router.dispatchJson(
        QStringLiteral(R"({"id":"1","kind":"call","path":"app.ping"})"));
    QVERIFY(ping.value(QStringLiteral("ok")).toBool());
    QCOMPARE(ping.value(QStringLiteral("id")).toString(), QStringLiteral("1"));
    QVERIFY(ping.value(QStringLiteral("value")).toObject()
                .value(QStringLiteral("pong")).toBool());

    const QJsonObject info = router.dispatchJson(
        QStringLiteral(R"({"path":"app.info"})"));
    QVERIFY(info.value(QStringLiteral("ok")).toBool());
    QCOMPARE(info.value(QStringLiteral("value")).toObject()
                 .value(QStringLiteral("protocol")).toString(),
             QStringLiteral("zalonative.bridge/1"));
}

void BridgeRouterTest::publishesAndSnapshotsState()
{
    BridgeRouter router;
    QSignalSpy published(&router, &BridgeRouter::statePublished);

    const QJsonObject state{
        {QStringLiteral("conversations"), QJsonArray{QJsonObject{
            {QStringLiteral("id"), QStringLiteral("a")},
            {QStringLiteral("title"), QStringLiteral("Lan")},
        }}},
        {QStringLiteral("messages"), QJsonObject{
            {QStringLiteral("a"), QJsonArray{QJsonObject{
                {QStringLiteral("text"), QStringLiteral("xin chào")},
            }}},
        }},
    };

    const QJsonObject request{
        {QStringLiteral("id"), QStringLiteral("publish")},
        {QStringLiteral("path"), QStringLiteral("state.publish")},
        {QStringLiteral("args"), QJsonArray{state}},
    };
    const QJsonObject response = router.dispatch(request);
    QVERIFY(response.value(QStringLiteral("ok")).toBool());
    QCOMPARE(published.count(), 1);
    QCOMPARE(response.value(QStringLiteral("value")).toObject()
                 .value(QStringLiteral("conversations")).toInt(), 1);
    QCOMPARE(router.lastState(), state);

    const QJsonObject snapshot = router.dispatchJson(
        QStringLiteral(R"({"path":"state.snapshot"})"));
    QVERIFY(snapshot.value(QStringLiteral("ok")).toBool());
    QCOMPARE(snapshot.value(QStringLiteral("value")).toObject(), state);

    const QJsonObject badConversations{
        {QStringLiteral("path"), QStringLiteral("state.publish")},
        {QStringLiteral("args"), QJsonArray{QJsonObject{
            {QStringLiteral("conversations"), QStringLiteral("nope")},
        }}},
    };
    const QJsonObject rejected = router.dispatch(badConversations);
    QVERIFY(!rejected.value(QStringLiteral("ok")).toBool());
    QCOMPARE(rejected.value(QStringLiteral("error")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("invalid_args"));
    QCOMPARE(published.count(), 1);
}

void BridgeRouterTest::rejectsUnknownAndMalformedRequests()
{
    BridgeRouter router;
    QSignalSpy logs(&router, &BridgeRouter::logLine);

    const QJsonObject unknown = router.dispatchJson(
        QStringLiteral(R"({"path":"process.spawn","args":["/bin/sh"]})"));
    QVERIFY(!unknown.value(QStringLiteral("ok")).toBool());
    QCOMPARE(unknown.value(QStringLiteral("error")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("unknown_method"));

    const QJsonObject badJson = router.dispatchJson(QStringLiteral("{not json"));
    QCOMPARE(badJson.value(QStringLiteral("error")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("invalid_json"));

    const QJsonObject empty = router.dispatchJson(QStringLiteral("[]"));
    QCOMPARE(empty.value(QStringLiteral("error")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("invalid_json"));

    const QJsonObject badArgs = router.dispatchJson(
        QStringLiteral(R"({"path":"state.publish","args":{"conversations":[]}})"));
    QCOMPARE(badArgs.value(QStringLiteral("error")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("invalid_args"));

    const QJsonObject badKind = router.dispatchJson(
        QStringLiteral(R"({"path":"app.ping","kind":"event"})"));
    QCOMPARE(badKind.value(QStringLiteral("error")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("unsupported_kind"));

    const QJsonObject emptyLog = router.dispatchJson(
        QStringLiteral(R"({"path":"log.write","args":[""]})"));
    QCOMPARE(emptyLog.value(QStringLiteral("error")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("invalid_args"));

    QVERIFY(!logs.isEmpty());
}

void BridgeRouterTest::enforcesPayloadLimit()
{
    BridgeRouter router;
    const QString huge = QString(2 * 1024 * 1024, QLatin1Char('x'));

    const QJsonObject request{
        {QStringLiteral("path"), QStringLiteral("log.write")},
        {QStringLiteral("args"), QJsonArray{huge}},
    };
    const QJsonObject response = router.dispatch(request);
    QVERIFY(!response.value(QStringLiteral("ok")).toBool());
    QCOMPARE(response.value(QStringLiteral("error")).toObject()
                 .value(QStringLiteral("code")).toString(),
             QStringLiteral("payload_too_large"));
}

void BridgeRouterTest::bridgesJsonThroughBridgeObject()
{
    BridgeRouter router;
    BridgeObject bridge(&router);

    const QString response = bridge.requestJson(
        QStringLiteral(R"({"id":"x","path":"app.ping"})"));
    const QJsonObject parsed = QJsonDocument::fromJson(response.toUtf8()).object();
    QVERIFY(parsed.value(QStringLiteral("ok")).toBool());
    QCOMPARE(parsed.value(QStringLiteral("id")).toString(), QStringLiteral("x"));

    const QString failure = bridge.requestJson(QStringLiteral("nonsense"));
    QVERIFY(!QJsonDocument::fromJson(failure.toUtf8()).object()
                 .value(QStringLiteral("ok")).toBool());
}

QTEST_GUILESS_MAIN(BridgeRouterTest)
#include "test_bridge.moc"
