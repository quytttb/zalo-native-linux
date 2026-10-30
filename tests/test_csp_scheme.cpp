#include "RuntimePagePolicy.h"

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

#include <atomic>

// M1 CSP integration: proves that the scoped in-memory rewrite is what lets a
// same-origin script run under the runtime's CSP, without disabling web security
// or ignoring CSP globally. This test does not load a Zalo runtime or log in.
class CspSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT

public:
    explicit CspSchemeHandler(QObject *parent = nullptr)
        : QWebEngineUrlSchemeHandler(parent)
    {
    }

    bool lastResponseRewritten() const { return m_rewritten.load(); }

    void requestStarted(QWebEngineUrlRequestJob *job) override
    {
        auto *buffer = new QBuffer(job);
        const QString path = job->requestUrl().path();
        if (path == QLatin1String("/app.js")) {
            buffer->setData("document.title = 'script-ran';");
            job->reply("text/javascript", buffer);
            return;
        }
        if (path != QLatin1String("/baseline.html")
            && path != QLatin1String("/rewritten.html")) {
            job->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
        }

        const QByteArray html = R"(<!doctype html>
<html><head>
<meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'self'; style-src 'self'">
<title>pending</title>
</head><body>
<script src="app.js"></script>
</body></html>)";

        m_rewritten = false;
        QByteArray served = html;
        if (path == QLatin1String("/rewritten.html")) {
            const auto result = ZaloNative::RuntimePagePolicy::allowSchemeInCsp(
                html, QByteArrayLiteral("zalocsp"));
            if (result.changed) {
                served = result.html;
                m_rewritten = true;
            }
        }
        buffer->setData(served);
        job->reply("text/html", buffer);
    }

private:
    std::atomic_bool m_rewritten{false};
};

class CspSchemeTest : public QObject
{
    Q_OBJECT

private slots:
    void scopedRewriteEnablesSameOriginScript();
};

void CspSchemeTest::scopedRewriteEnablesSameOriginScript()
{
    QWebEngineProfile profile;
    auto *handler = new CspSchemeHandler(&profile);
    profile.installUrlSchemeHandler(QByteArrayLiteral("zalocsp"), handler);

    QWebEnginePage page(&profile);

    QSignalSpy baselineLoaded(&page, &QWebEnginePage::loadFinished);
    page.setUrl(QUrl(QStringLiteral("zalocsp://runtime/baseline.html")));
    QVERIFY2(baselineLoaded.wait(20000), "baseline page did not finish loading");
    QVERIFY2(!baselineLoaded.isEmpty() && baselineLoaded.first().value(0).toBool(),
             "baseline page load failed");
    QTest::qWait(700);
    QVERIFY2(!handler->lastResponseRewritten(), "baseline must not be rewritten");
    QCOMPARE(page.title(), QStringLiteral("pending"));

    QSignalSpy rewrittenLoaded(&page, &QWebEnginePage::loadFinished);
    page.setUrl(QUrl(QStringLiteral("zalocsp://runtime/rewritten.html")));
    QVERIFY2(rewrittenLoaded.wait(20000), "rewritten page did not finish loading");
    QVERIFY2(!rewrittenLoaded.isEmpty() && rewrittenLoaded.first().value(0).toBool(),
             "rewritten page load failed");
    QVERIFY2(handler->lastResponseRewritten(), "rewritten page was not rewritten");
    QTRY_COMPARE_WITH_TIMEOUT(page.title(), QStringLiteral("script-ran"), 5000);
}

int main(int argc, char *argv[])
{
    QWebEngineUrlScheme scheme(QByteArrayLiteral("zalocsp"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(443);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme
                    | QWebEngineUrlScheme::LocalScheme
                    | QWebEngineUrlScheme::LocalAccessAllowed
                    | QWebEngineUrlScheme::CorsEnabled);
    QWebEngineUrlScheme::registerScheme(scheme);

    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    CspSchemeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_csp_scheme.moc"
