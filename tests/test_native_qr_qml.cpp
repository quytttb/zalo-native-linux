#include "FakeQrNetwork.h"
#include "NativeQrLogin.h"
#include "QrImageProvider.h"

#include <QBuffer>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTest>
#include <qqml.h>

class TestApp : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *qrLogin READ qrLogin CONSTANT)
public:
    explicit TestApp(ZaloNative::NativeQrLogin *login) : m_login(login) {}
    QObject *qrLogin() const { return m_login; }
private:
    ZaloNative::NativeQrLogin *m_login;
};

class NativeQrQmlTest : public QObject
{
    Q_OBJECT
private slots:
    void realShellDisplaysAndClearsSyntheticQr();
};

void NativeQrQmlTest::realShellDisplaysAndClearsSyntheticQr()
{
    QImage fixture(21, 21, QImage::Format_RGB32);
    fixture.fill(Qt::white);
    fixture.setPixelColor(0, 0, Qt::black);
    QByteArray png;
    QBuffer buffer(&png);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(fixture.save(&buffer, "PNG"));
    const QByteArray response = QJsonDocument(QJsonObject{
        {QStringLiteral("error_code"), 0},
        {QStringLiteral("data"), QJsonObject{
            {QStringLiteral("code"), QStringLiteral("fictional-ui-code")},
            {QStringLiteral("image"), QString::fromLatin1(png.toBase64())}}}
    }).toJson(QJsonDocument::Compact);

    FakeNetwork network;
    network.responses = {
        {200, "<script src=\"https://stc-zlogin.zdn.vn/main-1.2.3.js\"></script>"},
        {200, "{}"}, {200, "{}"}, {200, response},
        {200, {}, {}, QNetworkReply::NoError, true},
    };
    ZaloNative::NativeQrLogin::Timing timing;
    timing.requestTimeoutMs = 5000;
    timing.qrLifetimeMs = 5000;
    ZaloNative::NativeQrLogin login(&network, timing);
    TestApp app(&login);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    qmlRegisterSingletonInstance("ZaloNative", 1, 0, "App", &app);
    qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(ZALO_SOURCE_DIR) + QStringLiteral("/qml/Theme.qml")),
                             "ZaloNative", 1, 0, "Theme");
    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("nativeqr"), new ZaloNative::QrImageProvider(&login));
    QQmlComponent component(&engine,
                            QUrl::fromLocalFile(QStringLiteral(ZALO_SOURCE_DIR) + QStringLiteral("/qml/NativeShell.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> root(component.create());
    QVERIFY2(root, qPrintable(component.errorString()));
    QQuickWindow window;
    auto *item = qobject_cast<QQuickItem *>(root.data());
    QVERIFY(item);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(900, 700));
    window.resize(900, 700);
    window.show();

    QObject *start = root->findChild<QObject *>(QStringLiteral("nativeQrStart"));
    QObject *cancel = root->findChild<QObject *>(QStringLiteral("nativeQrCancel"));
    QObject *image = root->findChild<QObject *>(QStringLiteral("nativeQrImage"));
    QVERIFY(start);
    QVERIFY(cancel);
    QVERIFY(image);
    QVERIFY(start->property("enabled").toBool());
    QVERIFY(!cancel->property("enabled").toBool());
    QCOMPARE(network.requests.size(), 0);
    QVERIFY(QMetaObject::invokeMethod(start, "clicked", Qt::DirectConnection));
    QTRY_COMPARE_WITH_TIMEOUT(login.state(), QStringLiteral("waiting_scan"), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(image->property("status").toInt(), 1, 5000); // Image.Ready
    QVERIFY(login.hasQr());
    QVERIFY(cancel->property("enabled").toBool());
    QCOMPARE(network.requests.size(), 5);
    QVERIFY(image->property("source").toUrl().toString().startsWith(QStringLiteral("image://nativeqr/current?rev=")));
    QVERIFY(!image->property("cache").toBool());
    QVERIFY(QMetaObject::invokeMethod(cancel, "clicked", Qt::DirectConnection));
    QCOMPARE(login.state(), QStringLiteral("idle"));
    QVERIFY(!login.hasQr());
    QTRY_VERIFY_WITH_TIMEOUT(image->property("source").toUrl().isEmpty(), 5000);
    QCOMPARE(network.requests.size(), 5);
}

QTEST_MAIN(NativeQrQmlTest)
#include "test_native_qr_qml.moc"
