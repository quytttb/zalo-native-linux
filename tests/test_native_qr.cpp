#include "NativeQrLogin.h"
#include "FakeQrNetwork.h"

#include <QBuffer>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QSignalSpy>
#include <QTest>

using ZaloNative::NativeQrLogin;

namespace {

QByteArray qrResponse()
{
    QImage image(21, 21, QImage::Format_RGB32);
    image.fill(Qt::white);
    image.setPixelColor(0, 0, Qt::black);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QJsonDocument(QJsonObject{
        {QStringLiteral("error_code"), 0},
        {QStringLiteral("data"), QJsonObject{
            {QStringLiteral("code"), QStringLiteral("fictional-sensitive-qr-code")},
            {QStringLiteral("image"), QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64())}}}
    }).toJson(QJsonDocument::Compact);
}

QList<Response> throughConfirmation()
{
    return {
        {200, "<script src=\"https://stc-zlogin.zdn.vn/main-1.2.3.js\"></script>"},
        {200, "{\"error_code\":0}"},
        {200, "{}"},
        {200, qrResponse()},
        {200, "{\"error_code\":0,\"data\":{}}"},
        {200, "{\"error_code\":0}"},
    };
}

NativeQrLogin::Timing fastTiming()
{
    NativeQrLogin::Timing timing;
    timing.requestTimeoutMs = 1000;
    timing.qrLifetimeMs = 1000;
    timing.pollIntervalMs = 5;
    return timing;
}

} // namespace

class NativeQrTest : public QObject
{
    Q_OBJECT
private slots:
    void offlineSuccessfulWebSession();
    void noSuccessWithoutVerifiedUserInfo();
    void cancelStopsRequestsAndClearsCookies();
    void reentrantCancelStopsBeforeNextRequest_data();
    void reentrantCancelStopsBeforeNextRequest();
    void expiryStopsPolling();
    void requestDeadlineStopsHangingServer();
    void rejectsUntrustedRedirects_data();
    void rejectsUntrustedRedirects();
    void rejectsPostRedirect();
    void limitsRedirectHops();
    void rejectsMalformedAndOversizedResponses();
    void rejectsNetworkFailure();
};

void NativeQrTest::offlineSuccessfulWebSession()
{
    FakeNetwork network;
    network.responses = throughConfirmation();
    network.responses.insert(4, {200, "{\"error_code\":8}"});
    network.responses.insert(6, {200, "{\"error_code\":8}"});
    network.responses.append({302, {}, QUrl(QStringLiteral("https://jr.zaloapp.com/session?fictional=1"))});
    network.responses.append({200, "ok"});
    network.responses.append({200, "{\"data\":{\"logged\":true,\"name\":\"fictional-private-name\"}}"});
    NativeQrLogin login(&network, fastTiming());
    QStringList states;
    bool sawQr = false;
    connect(&login, &NativeQrLogin::changed, &login, [&] {
        states.append(login.state());
        if (login.hasQr()) {
            sawQr = !login.qrImage().isNull();
            QVERIFY(!login.webSessionReady());
        }
        QVERIFY(!login.status().contains(QStringLiteral("fictional")));
    });

    QCOMPARE(login.state(), QStringLiteral("idle"));
    QCOMPARE(network.requests.size(), 0);
    login.start();
    QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("web_session"), 5000);
    QVERIFY(sawQr);
    QVERIFY(login.webSessionReady());
    QVERIFY(!login.busy());
    QVERIFY(!login.hasQr());
    QVERIFY(login.qrImage().isNull());
    QVERIFY(states.contains(QStringLiteral("waiting_scan")));
    QVERIFY(states.contains(QStringLiteral("waiting_confirm")));
    QVERIFY(states.contains(QStringLiteral("checking")));
    QCOMPARE(network.requests.size(), 11);
    QVERIFY(network.responses.isEmpty());
    QCOMPARE(network.operations.at(0), QNetworkAccessManager::GetOperation);
    QCOMPARE(network.operations.at(1), QNetworkAccessManager::PostOperation);
    QCOMPARE(network.requests.last().url().path(), QStringLiteral("/jr/userinfo"));
    for (const QNetworkRequest &request : network.requests) {
        QCOMPARE(request.url().scheme(), QStringLiteral("https"));
        QCOMPARE(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
                 int(QNetworkRequest::ManualRedirectPolicy));
        QCOMPARE(request.attribute(QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::AlwaysNetwork));
        QVERIFY(!request.attribute(QNetworkRequest::CacheSaveControlAttribute).toBool());
    }
    login.cancel();
    QCOMPARE(login.state(), QStringLiteral("idle"));
    QVERIFY(!login.webSessionReady());
}

void NativeQrTest::noSuccessWithoutVerifiedUserInfo()
{
    for (const QByteArray &userInfo : {
             QByteArray("{\"data\":{\"logged\":false}}"),
             QByteArray("{\"error_code\":1,\"data\":{\"logged\":true}}"),
             QByteArray("{\"error_code\":\"0\",\"data\":{\"logged\":true}}")}) {
        FakeNetwork network;
        network.responses = throughConfirmation();
        network.responses.append({200, "ok"});
        network.responses.append({200, userInfo});
        NativeQrLogin login(&network, fastTiming());
        login.start();
        QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("failed"), 5000);
        QVERIFY(!login.webSessionReady());
        QVERIFY(!login.hasQr());
        QCOMPARE(network.requests.size(), 8);
    }
}

void NativeQrTest::cancelStopsRequestsAndClearsCookies()
{
    FakeNetwork network;
    network.responses.append({200, {}, {}, QNetworkReply::NoError, true});
    NativeQrLogin login(&network, fastTiming());
    login.start();
    QCOMPARE(network.requests.size(), 1);
    QVERIFY(login.busy());
    const QUrl origin(QStringLiteral("https://id.zalo.me/"));
    QVERIFY(network.cookieJar()->setCookiesFromUrl({QNetworkCookie("fixture", "fake")}, origin));
    QVERIFY(!network.cookieJar()->cookiesForUrl(origin).isEmpty());
    login.cancel();
    QCOMPARE(login.state(), QStringLiteral("idle"));
    QVERIFY(network.cookieJar()->cookiesForUrl(origin).isEmpty());
    QTest::qWait(30);
    QCOMPARE(network.requests.size(), 1);
    QVERIFY(!login.busy());
}

void NativeQrTest::reentrantCancelStopsBeforeNextRequest_data()
{
    QTest::addColumn<QString>("trigger");
    QTest::addColumn<int>("expectedRequests");
    QTest::newRow("preparing") << QStringLiteral("preparing") << 0;
    QTest::newRow("waiting scan") << QStringLiteral("waiting_scan") << 4;
    QTest::newRow("waiting confirm") << QStringLiteral("waiting_confirm") << 5;
    QTest::newRow("checking session") << QStringLiteral("checking") << 6;
}

void NativeQrTest::reentrantCancelStopsBeforeNextRequest()
{
    QFETCH(QString, trigger);
    QFETCH(int, expectedRequests);
    FakeNetwork network;
    network.responses = throughConfirmation();
    NativeQrLogin login(&network, fastTiming());
    connect(&login, &NativeQrLogin::changed, &login, [&login, &trigger] {
        if (login.state() == trigger)
            login.cancel();
    });
    login.start();
    QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("idle"), 5000);
    QTest::qWait(30);
    QCOMPARE(network.requests.size(), expectedRequests);
    QVERIFY(!login.hasQr());
}

void NativeQrTest::expiryStopsPolling()
{
    FakeNetwork network;
    network.responses = throughConfirmation().mid(0, 4);
    network.responses.append({200, {}, {}, QNetworkReply::NoError, true});
    auto timing = fastTiming();
    timing.qrLifetimeMs = 25;
    NativeQrLogin login(&network, timing);
    login.start();
    QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("failed"), 5000);
    QVERIFY(!login.hasQr());
    QCOMPARE(network.requests.size(), 5);
    QTest::qWait(30);
    QCOMPARE(network.requests.size(), 5);
}

void NativeQrTest::requestDeadlineStopsHangingServer()
{
    FakeNetwork network;
    network.responses.append({200, {}, {}, QNetworkReply::NoError, true});
    auto timing = fastTiming();
    timing.requestTimeoutMs = 25;
    NativeQrLogin login(&network, timing);
    login.start();
    QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("failed"), 5000);
    QCOMPARE(network.requests.size(), 1);
    QVERIFY(!login.webSessionReady());
}

void NativeQrTest::rejectsUntrustedRedirects_data()
{
    QTest::addColumn<QUrl>("target");
    QTest::newRow("external host") << QUrl(QStringLiteral("https://evil.example/"));
    QTest::newRow("suffix trick") << QUrl(QStringLiteral("https://jr.zaloapp.com.evil.example/"));
    QTest::newRow("plaintext") << QUrl(QStringLiteral("http://jr.zaloapp.com/"));
    QTest::newRow("credentials") << QUrl(QStringLiteral("https://user@jr.zaloapp.com/"));
    QTest::newRow("port") << QUrl(QStringLiteral("https://jr.zaloapp.com:444/"));
    QTest::newRow("fragment") << QUrl(QStringLiteral("https://jr.zaloapp.com/#private"));
}

void NativeQrTest::rejectsUntrustedRedirects()
{
    QFETCH(QUrl, target);
    FakeNetwork network;
    network.responses = throughConfirmation();
    network.responses.append({302, {}, target});
    NativeQrLogin login(&network, fastTiming());
    login.start();
    QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("failed"), 5000);
    QCOMPARE(network.requests.size(), 7);
    QVERIFY(!login.status().contains(QStringLiteral("evil")));
    QVERIFY(!login.webSessionReady());
}

void NativeQrTest::rejectsPostRedirect()
{
    FakeNetwork network;
    network.responses = throughConfirmation().mid(0, 2);
    network.responses.append({302, {}, QUrl(QStringLiteral("https://id.zalo.me/other"))});
    NativeQrLogin login(&network, fastTiming());
    login.start();
    QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("failed"), 5000);
    QCOMPARE(network.requests.size(), 3);
}

void NativeQrTest::limitsRedirectHops()
{
    FakeNetwork network;
    network.responses = throughConfirmation();
    for (int i = 0; i < 6; ++i)
        network.responses.append({302, {}, QUrl(QStringLiteral("https://jr.zaloapp.com/again"))});
    NativeQrLogin login(&network, fastTiming());
    login.start();
    QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("failed"), 5000);
    QCOMPARE(network.requests.size(), 12); // six preparation steps + five hops + final redirect
    QVERIFY(!login.webSessionReady());
}

void NativeQrTest::rejectsMalformedAndOversizedResponses()
{
    {
        FakeNetwork network;
        network.responses = throughConfirmation().mid(0, 3);
        network.responses.append({200, "{\"error_code\":0,\"data\":{\"code\":\"x\",\"image\":\"%%%\"}}"});
        NativeQrLogin login(&network, fastTiming());
        login.start();
        QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("failed"), 5000);
        QVERIFY(!login.hasQr());
        QCOMPARE(network.requests.size(), 4);
    }
    {
        FakeNetwork network;
        network.responses.append({200, QByteArray(2 * 1024 * 1024 + 1, 'x')});
        NativeQrLogin login(&network, fastTiming());
        login.start();
        QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("failed"), 5000);
        QCOMPARE(network.requests.size(), 1);
        QVERIFY(!login.webSessionReady());
    }
}

void NativeQrTest::rejectsNetworkFailure()
{
    FakeNetwork network;
    network.responses.append({0, {}, {}, QNetworkReply::SslHandshakeFailedError});
    NativeQrLogin login(&network, fastTiming());
    login.start();
    QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("failed"), 5000);
    QCOMPARE(network.requests.size(), 1);
    QVERIFY(!login.webSessionReady());
}

QTEST_GUILESS_MAIN(NativeQrTest)
#include "test_native_qr.moc"
