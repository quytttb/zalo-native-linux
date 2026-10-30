#include <QBuffer>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QtWebEngineQuick/QtWebEngineQuick>

// M1 sync spike: can renderer-side JavaScript make a *synchronous* request to a
// Qt custom scheme on the same origin? If yes, a future preload shim can serve
// sendSync-style calls without QWebChannel's asynchronous round-trip. This test
// proves only the transport; it does not implement any Zalo API.
class SyncSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT

public:
    explicit SyncSchemeHandler(QObject *parent = nullptr)
        : QWebEngineUrlSchemeHandler(parent)
    {
    }

    void requestStarted(QWebEngineUrlRequestJob *job) override
    {
        auto *buffer = new QBuffer(job);
        const QString path = job->requestUrl().path();
        if (path == QLatin1String("/ping")) {
            buffer->setData("pong");
            job->reply("text/plain", buffer);
            return;
        }
        if (path == QLatin1String("/probe.html")) {
            buffer->setData(R"(<!doctype html>
<html><head><meta charset="utf-8"><title>pending</title></head>
<body>
<script>
try {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/ping', false);
  xhr.send();
  document.title = 'sync:' + xhr.status + ':' + xhr.responseText;
} catch (e) {
  document.title = 'error:' + e.name + ':' + e.message;
}
</script>
</body></html>)");
            job->reply("text/html", buffer);
            return;
        }
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
    }
};

class SyncSchemeTest : public QObject
{
    Q_OBJECT

private slots:
    void synchronousXhrToCustomScheme();
};

void SyncSchemeTest::synchronousXhrToCustomScheme()
{
    QWebEngineProfile profile;
    auto *handler = new SyncSchemeHandler(&profile);
    profile.installUrlSchemeHandler(QByteArrayLiteral("zalosync"), handler);

    QWebEnginePage page(&profile);
    QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
    page.setUrl(QUrl(QStringLiteral("zalosync://runtime/probe.html")));

    QVERIFY2(loaded.wait(20000), "custom-scheme page did not finish loading");
    QVERIFY2(!loaded.isEmpty() && loaded.first().value(0).toBool(), "page load failed");

    QTRY_VERIFY_WITH_TIMEOUT(page.title() != QLatin1String("pending"), 5000);
    qInfo().noquote() << "sync-scheme probe result:" << page.title();
    QCOMPARE(page.title(), QStringLiteral("sync:200:pong"));
}

int main(int argc, char *argv[])
{
    QWebEngineUrlScheme scheme(QByteArrayLiteral("zalosync"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(443);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme
                    | QWebEngineUrlScheme::LocalScheme
                    | QWebEngineUrlScheme::LocalAccessAllowed
                    | QWebEngineUrlScheme::CorsEnabled);
    QWebEngineUrlScheme::registerScheme(scheme);

    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    SyncSchemeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_sync_scheme.moc"
