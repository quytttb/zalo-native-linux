#include "PcQrProtocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QUrlQuery>

using namespace ZaloNative::PcQrProtocol;

namespace {

RequestFields fixtureFields()
{
    RequestFields fields;
    fields.deviceId = QStringLiteral("fixture-device");
    fields.computerName = QStringLiteral("test host");
    fields.clientTimeSeconds = 1700000000;
    return fields;
}

QJsonObject fixtureData()
{
    // These are fictional values, not a copied live ticket or account data.
    return {
        {QStringLiteral("qr_cnt"), QStringLiteral("http://zaloapp.com/qr/l?tk=fictional")},
        {QStringLiteral("chk_wait_scan"), QStringLiteral("https://qr-id.zaloapp.com/polling/qr/waiting?src=pc&code=fictional")},
        {QStringLiteral("chk_wait_cfirm"), QStringLiteral("https://qr-id.zaloapp.com/polling/qr/pc-status?src=pc&code=fictional")},
        {QStringLiteral("qr_timeout"), 180},
        {QStringLiteral("qr_retry"), 3},
    };
}

QByteArray response(const QJsonObject &data, int errorCode = 0)
{
    return QJsonDocument(QJsonObject{
        {QStringLiteral("error_code"), errorCode},
        {QStringLiteral("data"), data},
    }).toJson(QJsonDocument::Compact);
}

} // namespace

class PcQrProtocolTest : public QObject
{
    Q_OBJECT
private slots:
    void buildsSignedRequest();
    void validatesRequestFields();
    void parsesSyntheticTicket();
    void rejectsInvalidTickets_data();
    void rejectsInvalidTickets();
    void rejectsInvalidEnvelope();
};

void PcQrProtocolTest::buildsSignedRequest()
{
    const auto url = buildRequest(fixtureFields());
    QVERIFY(url.has_value());
    QCOMPARE(url->scheme(), QStringLiteral("https"));
    QCOMPARE(url->host(), QStringLiteral("wpa.zaloapp.com"));
    QCOMPARE(url->path(), QStringLiteral("/api/login/reqqr"));
    const QUrlQuery query(*url);
    QCOMPARE(query.queryItemValue(QStringLiteral("client_time")), QStringLiteral("1700000000"));
    QCOMPARE(query.queryItemValue(QStringLiteral("client_version")), QStringLiteral("694"));
    QCOMPARE(query.queryItemValue(QStringLiteral("computer_name"), QUrl::FullyDecoded), QStringLiteral("test host"));
    QCOMPARE(query.queryItemValue(QStringLiteral("imei")), QStringLiteral("fixture-device"));
    QCOMPARE(query.queryItemValue(QStringLiteral("logged_uids"), QUrl::FullyDecoded), QStringLiteral("[]"));
    QCOMPARE(query.queryItemValue(QStringLiteral("self_g")), QStringLiteral("1"));
    QCOMPARE(query.queryItemValue(QStringLiteral("qr_form")), QStringLiteral("0"));
    QCOMPARE(query.queryItemValue(QStringLiteral("type")), QStringLiteral("30"));
    // Independent known answer for sorted fixture values, not a live capture.
    QCOMPARE(query.queryItemValue(QStringLiteral("signkey")), QStringLiteral("941758aeab01b7793f2212ff63ef914b"));
    QVERIFY(url->toEncoded().contains("computer_name=test%20host"));

    RequestFields special = fixtureFields();
    special.computerName = QStringLiteral("Máy & +/%");
    const auto encoded = buildRequest(special);
    QVERIFY(encoded.has_value());
    QCOMPARE(QUrlQuery(*encoded).queryItemValue(QStringLiteral("computer_name"), QUrl::FullyDecoded),
             special.computerName);
    QVERIFY(encoded->toEncoded().contains("%26"));
    QVERIFY(encoded->toEncoded().contains("%2B"));
}

void PcQrProtocolTest::validatesRequestFields()
{
    RequestFields fields = fixtureFields();
    fields.deviceId.clear();
    QVERIFY(!buildRequest(fields));
    fields = fixtureFields();
    fields.computerName = QString(257, QLatin1Char('x'));
    QVERIFY(!buildRequest(fields));
    fields = fixtureFields();
    fields.clientTimeSeconds = 0;
    QVERIFY(!buildRequest(fields));
    fields = fixtureFields();
    fields.language = QString(QChar::Null);
    QVERIFY(!buildRequest(fields));
}

void PcQrProtocolTest::parsesSyntheticTicket()
{
    const auto ticket = parseTicket(response(fixtureData()));
    QVERIFY(ticket.has_value());
    QCOMPARE(ticket->timeoutSeconds, 180);
    QCOMPARE(ticket->retryCount, 3);
    QCOMPARE(ticket->scanUrl.host(), QStringLiteral("qr-id.zaloapp.com"));
    QCOMPARE(ticket->confirmUrl.path(), QStringLiteral("/polling/qr/pc-status"));

    QJsonObject defaults = fixtureData();
    defaults.remove(QStringLiteral("qr_timeout"));
    defaults.remove(QStringLiteral("qr_retry"));
    const auto defaultTicket = parseTicket(response(defaults));
    QVERIFY(defaultTicket.has_value());
    QCOMPARE(defaultTicket->timeoutSeconds, 180);
    QCOMPARE(defaultTicket->retryCount, 3);
}

void PcQrProtocolTest::rejectsInvalidTickets_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QJsonValue>("value");
    QTest::newRow("missing QR") << QStringLiteral("qr_cnt") << QJsonValue(QString());
    QTest::newRow("QR foreign host") << QStringLiteral("qr_cnt") << QJsonValue(QStringLiteral("http://evil.example/qr/l?tk=x"));
    QTest::newRow("QR missing token") << QStringLiteral("qr_cnt") << QJsonValue(QStringLiteral("http://zaloapp.com/qr/l"));
    QTest::newRow("QR userinfo") << QStringLiteral("qr_cnt") << QJsonValue(QStringLiteral("http://user@zaloapp.com/qr/l?tk=x"));
    QTest::newRow("scan HTTP") << QStringLiteral("chk_wait_scan") << QJsonValue(QStringLiteral("http://qr-id.zaloapp.com/polling/qr/waiting?code=x"));
    QTest::newRow("scan foreign host") << QStringLiteral("chk_wait_scan") << QJsonValue(QStringLiteral("https://evil.example/polling/qr/waiting?code=x"));
    QTest::newRow("scan suffix") << QStringLiteral("chk_wait_scan") << QJsonValue(QStringLiteral("https://qr-id.zaloapp.com.evil.example/polling/qr/waiting?code=x"));
    QTest::newRow("scan port") << QStringLiteral("chk_wait_scan") << QJsonValue(QStringLiteral("https://qr-id.zaloapp.com:444/polling/qr/waiting?code=x"));
    QTest::newRow("scan missing code") << QStringLiteral("chk_wait_scan") << QJsonValue(QStringLiteral("https://qr-id.zaloapp.com/polling/qr/waiting"));
    QTest::newRow("confirm wrong path") << QStringLiteral("chk_wait_cfirm") << QJsonValue(QStringLiteral("https://qr-id.zaloapp.com/polling/qr/waiting?code=x"));
    QTest::newRow("negative timeout") << QStringLiteral("qr_timeout") << QJsonValue(-1);
    QTest::newRow("oversized timeout") << QStringLiteral("qr_timeout") << QJsonValue(301);
    QTest::newRow("fractional timeout") << QStringLiteral("qr_timeout") << QJsonValue(1.5);
    QTest::newRow("oversized retry") << QStringLiteral("qr_retry") << QJsonValue(11);
    QTest::newRow("bad retry type") << QStringLiteral("qr_retry") << QJsonValue(QStringLiteral("3"));
}

void PcQrProtocolTest::rejectsInvalidTickets()
{
    QFETCH(QString, key);
    QFETCH(QJsonValue, value);
    QJsonObject data = fixtureData();
    data.insert(key, value);
    QVERIFY(!parseTicket(response(data)));
}

void PcQrProtocolTest::rejectsInvalidEnvelope()
{
    QVERIFY(!parseTicket("{}"));
    QVERIFY(!parseTicket("[]"));
    QVERIFY(!parseTicket(response(fixtureData(), 1)));
    QVERIFY(!parseTicket(QByteArray(2 * 1024 * 1024 + 1, 'x')));
}

QTEST_GUILESS_MAIN(PcQrProtocolTest)
#include "test_pc_qr_protocol.moc"
