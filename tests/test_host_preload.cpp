#include "HostApi.h"

#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QtWebEngineQuick/QtWebEngineQuick>

using namespace ZaloNative;

namespace {

class HostPreloadSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT

public:
    HostPreloadSchemeHandler(HostApi *api, QObject *parent = nullptr)
        : QWebEngineUrlSchemeHandler(parent)
        , m_api(api)
    {
    }

    void requestStarted(QWebEngineUrlRequestJob *job) override
    {
        const QUrl url = job->requestUrl();
        if (url.path() == QLatin1String("/__zalonative_host")) {
            if (job->requestMethod() != QByteArrayLiteral("GET")) {
                job->fail(QWebEngineUrlRequestJob::UrlNotFound);
                return;
            }
            const QUrlQuery query(url);
            const QByteArray payload
                = query.queryItemValue(QStringLiteral("request"), QUrl::FullyDecoded).toUtf8();
            const QJsonObject response = m_api->dispatchJson(QString::fromUtf8(payload));
            auto *buffer = new QBuffer(job);
            buffer->setData(QJsonDocument(response).toJson(QJsonDocument::Compact));
            job->reply(QByteArrayLiteral("application/json"), buffer);
            return;
        }

        if (url.path() == QLatin1String("/index.html")) {
            auto *buffer = new QBuffer(job);
            buffer->setData(QByteArrayLiteral(
                "<!doctype html><html><head><meta charset=\"utf-8\">"
                "<title>host</title></head><body><p>host</p></body></html>"));
            job->reply(QByteArrayLiteral("text/html"), buffer);
            return;
        }

        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
    }

private:
    HostApi *m_api = nullptr;
};

} // namespace

class HostPreloadTest : public QObject
{
    Q_OBJECT

private slots:
    void preloadUsesBoundedHostApi();
};

void HostPreloadTest::preloadUsesBoundedHostApi()
{
    QTemporaryDir data;
    QVERIFY(data.isValid());
    HostApi api(data.path(), data.path(), QStringLiteral("test-version"));

    QWebEngineProfile profile;
    auto *handler = new HostPreloadSchemeHandler(&api, &profile);
    profile.installUrlSchemeHandler(QByteArrayLiteral("zalohost"), handler);

    QWebEnginePage page(&profile);

    QFile preload(QStringLiteral(ZALO_SOURCE_DIR) + QStringLiteral("/renderer/host/preload.js"));
    QVERIFY2(preload.open(QIODevice::ReadOnly), "cannot open renderer/host/preload.js");
    const QByteArray config = QJsonDocument(QJsonObject{
        {QStringLiteral("cwd"), api.appRoot()},
        {QStringLiteral("platform"), QStringLiteral("linux")},
        {QStringLiteral("arch"), QStringLiteral("x64")}})
                                 .toJson(QJsonDocument::Compact);

    QWebEngineScript script;
    script.setName(QStringLiteral("zalonative-host-preload-test"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(QStringLiteral("window.__zalonativeHostConfig = ")
                         + QString::fromUtf8(config) + QStringLiteral(";\n")
                         + QString::fromUtf8(preload.readAll()));
    page.scripts().insert(script);

    QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
    page.setUrl(QUrl(QStringLiteral("zalohost://app/index.html")));
    QVERIFY2(loaded.wait(20000), "host test page did not finish loading");
    QVERIFY2(!loaded.isEmpty() && loaded.first().value(0).toBool(), "host test page load failed");

    const QString js = QStringLiteral(R"(
(function () {
    var result = {};
    try {
        result.pathJoin = $znode.path.join('/a', 'b', '..', 'c');
        result.pathNormalize = $znode.path.normalize('a//b/../c/');
        result.pathResolve = $znode.path.resolve('nested', '..', 'file.txt');
        result.pathDirname = $znode.path.dirname('/a/b/c.txt');
        result.pathExtname = $znode.path.extname('/a/b/c.txt');
        result.userData = $zelectron.getPath('userData');
        result.appVersion = $zelectron.getAppVersion();
        $znode.fs.writeFileSync('hello.txt', 'xin chào');
        result.read = $znode.fs.readFileSync('hello.txt', 'utf8');
        result.fsPromise = typeof $znode.fsPromise.readFile === 'function';
        $znode.fsExtra.ensureDirSync('nested/dir');
        result.exists = $znode.fs.existsSync('nested/dir');
        var store = $zelectron.getElectronStore('test');
        store.set('k', {a: 1});
        var changes = [];
        var anyChanges = [];
        var off = store.onDidChange('k', function (next, previous) {
            changes.push([next && next.a, previous && previous.a]);
        });
        store.onDidAnyChange(function (next, previous) {
            anyChanges.push([next.k && next.k.a, previous.k && previous.k.a]);
        });
        store.set('k', {a: 2});
        result.store = store.get('k').a;
        result.storeSize = store.size;
        result.storeChanged = changes.length + ':' + changes[0][0] + ':' + changes[0][1];
        result.storeAny = anyChanges.length + ':' + anyChanges[0][0] + ':' + anyChanges[0][1];
        off();
        store.set('k', {a: 3});
        result.storeAfterUnsub = changes.length;
        try { store.reset('k'); } catch (e) { result.storeReset = e.code; }
        result.storeHasAfterReset = store.has('k');
        try { $zelectronNative.getResourceUsage(); } catch (e) { result.resourceUsage = e.code; }
        try { $znode.os.cpus(); } catch (e) { result.cpus = e.code; }
        result.nativelibs = typeof $znode.nativelibs.fileUtilities().getDirectorySizeAsync;
        try {
            $znode.fs.writeFileSync('../outside.txt', 'x');
            result.outside = 'no-error';
        } catch (e) {
            result.outside = e.code;
        }
        try {
            $zelectron.getPath('documents');
            result.documents = 'no-error';
        } catch (e) {
            result.documents = e.code;
        }
        try {
            $znode.fs.readFileSync('missing.txt', 'utf8');
            result.missing = 'no-error';
        } catch (e) {
            result.missing = e.code;
        }
    } catch (e) {
        result.error = String(e) + ' code=' + e.code + ' stack=' + (e.stack || '');
    }
    return JSON.stringify(result);
})()
)");

    QVariant value;
    bool done = false;
    page.runJavaScript(js, [&value, &done](const QVariant &result) {
        value = result;
        done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);

    const QJsonObject result = QJsonDocument::fromJson(value.toString().toUtf8()).object();
    QVERIFY2(!result.contains(QStringLiteral("error")),
             qPrintable(result.value(QStringLiteral("error")).toString()));
    QCOMPARE(result.value(QStringLiteral("pathJoin")).toString(), QStringLiteral("/a/c"));
    QCOMPARE(result.value(QStringLiteral("pathNormalize")).toString(), QStringLiteral("a/c/"));
    QCOMPARE(result.value(QStringLiteral("pathDirname")).toString(), QStringLiteral("/a/b"));
    QCOMPARE(result.value(QStringLiteral("pathExtname")).toString(), QStringLiteral(".txt"));
    QVERIFY(result.value(QStringLiteral("pathResolve"))
                .toString()
                .endsWith(QStringLiteral("/file.txt")));
    QVERIFY(result.value(QStringLiteral("userData"))
                .toString()
                .startsWith(api.dataRoot()));
    QCOMPARE(result.value(QStringLiteral("appVersion")).toString(),
             QStringLiteral("test-version"));
    QCOMPARE(result.value(QStringLiteral("read")).toString(), QStringLiteral("xin chào"));
    QVERIFY(result.value(QStringLiteral("fsPromise")).toBool());
    QVERIFY(result.value(QStringLiteral("exists")).toBool());
    QCOMPARE(result.value(QStringLiteral("store")).toInt(), 2);
    QCOMPARE(result.value(QStringLiteral("storeSize")).toInt(), 1);
    QCOMPARE(result.value(QStringLiteral("storeChanged")).toString(), QStringLiteral("1:2:1"));
    QCOMPARE(result.value(QStringLiteral("storeAny")).toString(), QStringLiteral("1:2:1"));
    QCOMPARE(result.value(QStringLiteral("storeAfterUnsub")).toInt(), 1);
    QCOMPARE(result.value(QStringLiteral("storeReset")).toString(), QStringLiteral("ERR_HOST_UNSUPPORTED"));
    QVERIFY(result.value(QStringLiteral("storeHasAfterReset")).toBool());
    QCOMPARE(result.value(QStringLiteral("nativelibs")).toString(), QStringLiteral("function"));
    QCOMPARE(result.value(QStringLiteral("resourceUsage")).toString(), QStringLiteral("ERR_HOST_UNSUPPORTED"));
    QCOMPARE(result.value(QStringLiteral("cpus")).toString(), QStringLiteral("ERR_HOST_UNSUPPORTED"));
    QCOMPARE(result.value(QStringLiteral("outside")).toString(),
             QStringLiteral("ERR_HOST_OUTSIDE_ROOT"));
    QCOMPARE(result.value(QStringLiteral("documents")).toString(),
             QStringLiteral("ERR_HOST_PATH_UNSUPPORTED"));
    QCOMPARE(result.value(QStringLiteral("missing")).toString(), QStringLiteral("ENOENT"));
}

int main(int argc, char *argv[])
{
    QWebEngineUrlScheme scheme(QByteArrayLiteral("zalohost"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(443);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme
                    | QWebEngineUrlScheme::LocalScheme
                    | QWebEngineUrlScheme::LocalAccessAllowed
                    | QWebEngineUrlScheme::CorsEnabled);
    QWebEngineUrlScheme::registerScheme(scheme);

    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    HostPreloadTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_host_preload.moc"
