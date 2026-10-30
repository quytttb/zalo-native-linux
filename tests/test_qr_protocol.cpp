#include "QrProtocol.h"
#include "ProtocolCrypto.h"

#include <QCryptographicHash>
#include <QtTest>

class QrProtocolTest : public QObject
{
    Q_OBJECT
private slots:
    void findsOnlyExpectedVersion()
    {
        using ZaloNative::QrProtocol::loginVersion;
        QCOMPARE(loginVersion("<script src=\"https://stc-zlogin.zdn.vn/main-1.2.3.js\"></script>"), "1.2.3");
        QVERIFY(loginVersion("https://evil.example/main-1.2.3.js").isEmpty());
        QVERIFY(loginVersion(QByteArray(2 * 1024 * 1024 + 1, 'x')).isEmpty());
    }
    void validatesQr()
    {
        QString code;
        QByteArray png;
        QVERIFY(ZaloNative::QrProtocol::qrResponse(
            R"({"error_code":0,"data":{"code":"fixture","image":"data:image/png;base64,iVBORw0KGgo="}})", &code, &png));
        QCOMPARE(code, "fixture");
        QVERIFY(!ZaloNative::QrProtocol::qrResponse(R"({"error_code":1,"data":{}})", &code, &png));
        QVERIFY(!ZaloNative::QrProtocol::qrResponse(R"({"error_code":0,"data":{"code":"fixture","image":"%%%"}})", &code, &png));
        QVERIFY(!ZaloNative::QrProtocol::qrResponse("{}", nullptr, &png));
    }
    void cryptoFixtures()
    {
        using namespace ZaloNative::ProtocolCrypto;
        const QByteArray key("0123456789abcdef0123456789abcdef");
        const auto ciphertext = encrypt(key, "fixture");
        QVERIFY(ciphertext.has_value());
        QCOMPARE(ciphertext->toBase64(), QByteArray("7rkGyfACz1Y33VAJYE88cg=="));
        const auto plain = decrypt(key, *ciphertext);
        QVERIFY(plain.has_value());
        QCOMPARE(*plain, QByteArray("fixture"));
        QVERIFY(!encrypt("short", "x"));
        QVERIFY(!decrypt(key, QByteArray("broken")));
        const QMap<QByteArray, QByteArray> values{{"b", "two"}, {"a", "one"}};
        QCOMPARE(sign("test", values), QCryptographicHash::hash("zsecuretestonetwo", QCryptographicHash::Md5).toHex());
    }
};
QTEST_GUILESS_MAIN(QrProtocolTest)
#include "test_qr_protocol.moc"
