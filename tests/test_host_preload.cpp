#include "HostApi.h"

#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
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
    void watchedStoreObservesExternalChanges();
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
        result.osRelease = $znode.os.release();
        $znode.fs.writeFileSync('hello.txt', 'xin chào');
        result.read = $znode.fs.readFileSync('hello.txt', 'utf8');
        var bytes = $znode.fs.readFileSync('hello.txt');
        result.bytesUtf8 = bytes.toString();
        result.bytesHex = bytes.toString('hex');
        result.bytesBase64 = bytes.toString('base64');
        result.bytesArray = bytes instanceof Uint8Array;
        try { bytes.toString('ascii'); } catch (e) { result.bytesUnsupported = e.code; }
        try { bytes.toString('utf8', 0, 1); } catch (e) { result.bytesSlice = e.code; }
        $znode.fs.writeFileSync('bytes.bin', new Uint8Array([0xef, 0xbb, 0xbf, 0xff]));
        result.bytesBom = $znode.fs.readFileSync('bytes.bin').toString();
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
        var prototypeNotification = false;
        var offSpecial = store.onDidAnyChange(function (next) {
            prototypeNotification = Object.prototype.hasOwnProperty.call(next, '__proto__')
                && next['__proto__'].marker === 7;
        });
        store.set('__proto__', {marker: 7});
        result.prototypeKey = store.get('__proto__').marker;
        result.prototypeNotification = prototypeNotification;
        result.snapshotPrototypeKey = anyChanges.length > 0 && store.snapshot()['__proto__'].marker;
        offSpecial();
        store.delete('__proto__');
        var specialStore = $zelectron.getElectronStore('__proto__');
        specialStore.set('value', 9);
        result.specialStore = specialStore.get('value');
        var namedStore = $zelectron.getElectronStore({name: 'named', watch: false});
        namedStore.set('value', 11);
        result.namedStore = $zelectron.getElectronStore('named').get('value');
        result.separateStore = $zelectron.getElectronStore().has('value');
        result.watchSameStore = $zelectron.getElectronStore({name: 'named', watch: true}) === namedStore;
        var upgrade = $zelectron.getElectronStore('watch-upgrade');
        var releaseUpgrade = upgrade.onDidChange('key', function () {});
        result.watchInitiallyOff = upgrade.watchTimer === null;
        $zelectron.getElectronStore({name: 'watch-upgrade', watch: true});
        result.watchUpgrade = upgrade.watchTimer !== null;
        releaseUpgrade();
        result.watchUpgradeStopped = upgrade.watchTimer === null;
        var releases = [];
        for (var wi = 0; wi < 16; ++wi)
            releases.push($zelectron.getElectronStore({name: 'watch-limit-' + wi, watch: true})
                .onDidChange('key', function () {}));
        try {
            $zelectron.getElectronStore({name: 'watch-limit-extra', watch: true})
                .onDidChange('key', function () {});
        } catch (e) { result.watchLimit = e.code; }
        releases.forEach(function (release) { release(); });
        var afterLimit = $zelectron.getElectronStore({name: 'watch-limit-extra', watch: true});
        var releaseAfterLimit = afterLimit.onDidChange('key', function () {});
        result.watchSlotReused = afterLimit.watchTimer !== null;
        releaseAfterLimit();
        try { $zelectron.getElectronStore({name: 'named', defaults: {value: 1}}); }
        catch (e) { result.defaultsUnsupported = e.code; }
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
    QVERIFY(!result.value(QStringLiteral("osRelease")).toString().isEmpty());
    QCOMPARE(result.value(QStringLiteral("read")).toString(), QStringLiteral("xin chào"));
    QCOMPARE(result.value(QStringLiteral("bytesUtf8")).toString(), QStringLiteral("xin chào"));
    QCOMPARE(result.value(QStringLiteral("bytesHex")).toString(),
             QString::fromLatin1(QStringLiteral("xin chào").toUtf8().toHex()));
    QCOMPARE(result.value(QStringLiteral("bytesBase64")).toString(),
             QString::fromLatin1(QStringLiteral("xin chào").toUtf8().toBase64()));
    QVERIFY(result.value(QStringLiteral("bytesArray")).toBool());
    QCOMPARE(result.value(QStringLiteral("bytesUnsupported")).toString(), QStringLiteral("ERR_HOST_UNSUPPORTED"));
    QCOMPARE(result.value(QStringLiteral("bytesSlice")).toString(), QStringLiteral("ERR_HOST_UNSUPPORTED"));
    QCOMPARE(result.value(QStringLiteral("bytesBom")).toString(), QString::fromUtf8("\xef\xbb\xbf\xef\xbf\xbd"));
    QVERIFY(result.value(QStringLiteral("fsPromise")).toBool());
    QVERIFY(result.value(QStringLiteral("exists")).toBool());
    QCOMPARE(result.value(QStringLiteral("store")).toInt(), 2);
    QCOMPARE(result.value(QStringLiteral("storeSize")).toInt(), 1);
    QCOMPARE(result.value(QStringLiteral("storeChanged")).toString(), QStringLiteral("1:2:1"));
    QCOMPARE(result.value(QStringLiteral("storeAny")).toString(), QStringLiteral("1:2:1"));
    QCOMPARE(result.value(QStringLiteral("prototypeKey")).toInt(), 7);
    QVERIFY(result.value(QStringLiteral("prototypeNotification")).toBool());
    QCOMPARE(result.value(QStringLiteral("snapshotPrototypeKey")).toInt(), 7);
    QCOMPARE(result.value(QStringLiteral("specialStore")).toInt(), 9);
    QCOMPARE(result.value(QStringLiteral("namedStore")).toInt(), 11);
    QVERIFY(!result.value(QStringLiteral("separateStore")).toBool());
    QVERIFY(result.value(QStringLiteral("watchSameStore")).toBool());
    QVERIFY(result.value(QStringLiteral("watchInitiallyOff")).toBool());
    QVERIFY(result.value(QStringLiteral("watchUpgrade")).toBool());
    QVERIFY(result.value(QStringLiteral("watchUpgradeStopped")).toBool());
    QCOMPARE(result.value(QStringLiteral("watchLimit")).toString(), QStringLiteral("ERR_HOST_WATCH_LIMIT"));
    QVERIFY(result.value(QStringLiteral("watchSlotReused")).toBool());
    QCOMPARE(result.value(QStringLiteral("defaultsUnsupported")).toString(), QStringLiteral("ERR_HOST_UNSUPPORTED"));
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

void HostPreloadTest::watchedStoreObservesExternalChanges()
{
    QTemporaryDir data;
    QVERIFY(data.isValid());
    HostApi api(data.path(), data.path(), QStringLiteral("test-version"));
    HostApi external(data.path(), data.path(), QStringLiteral("test-version"));
    QWebEngineProfile profile;
    profile.installUrlSchemeHandler(QByteArrayLiteral("zalohost"),
                                    new HostPreloadSchemeHandler(&api, &profile));
    QWebEnginePage page(&profile);
    QFile preload(QStringLiteral(ZALO_SOURCE_DIR) + QStringLiteral("/renderer/host/preload.js"));
    QVERIFY(preload.open(QIODevice::ReadOnly));
    QWebEngineScript script;
    script.setName(QStringLiteral("zalonative-host-watch-test"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(QString::fromUtf8(preload.readAll()));
    page.scripts().insert(script);

    QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
    page.setUrl(QUrl(QStringLiteral("zalohost://app/index.html")));
    QVERIFY(loaded.wait(20000));
    QVERIFY(!loaded.isEmpty() && loaded.first().value(0).toBool());
    page.runJavaScript(QStringLiteral(R"(
        (function () {
            var fn = function (n, cb) { cb(null, this.base + n, 'ignored'); };
            var fail = function (cb) { cb(new Error('expected')); };
            var custom = function () {};
            var special = function () { return 'custom'; };
            custom[$znode.util.promisify.custom] = special;
            var valid = $znode.util.promisify(custom) === special;
            try { $znode.util.promisify(null); valid = false; }
            catch (error) { valid = valid && error instanceof TypeError; }
            Promise.all([
                $znode.util.promisify(fn).call({base: 4}, 3),
                $znode.util.promisify(fail)().then(function () { return 'wrong'; },
                                                   function (error) { return error.message; })
            ]).then(function (values) {
                document.title = valid && values[0] === 7 && values[1] === 'expected'
                    ? 'promisify:ok' : 'promisify:failed';
            }).catch(function () { document.title = 'promisify:failed'; });
        })();
    )"));
    QTRY_COMPARE_WITH_TIMEOUT(page.title(), QStringLiteral("promisify:ok"), 10000);
    bool done = false;
    page.runJavaScript(QStringLiteral(R"(
        window.testStore = $zelectron.getElectronStore({name:'watch-test', watch:true});
        window.testStore.set('item', 'first');
        window.watchEvents = [];
        window.offKey = window.testStore.onDidChange('item', function (value, old) {
            window.watchEvents.push('key:' + old + ':' + value);
        });
        window.offAny = window.testStore.onDidAnyChange(function (value, old) {
            window.watchEvents.push('any:' + old.item + ':' + value.item);
            document.title = 'watch:' + value.item;
        });
    )"), [&done](const QVariant &) { done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);

    const auto writeExternal = [&external](const QString &value) {
        return external.dispatch(QJsonObject{
            {QStringLiteral("op"), QStringLiteral("store.set")},
            {QStringLiteral("args"), QJsonObject{{QStringLiteral("name"), QStringLiteral("watch-test")},
                                                  {QStringLiteral("key"), QStringLiteral("item")},
                                                  {QStringLiteral("value"), value}}}});
    };
    QVERIFY(writeExternal(QStringLiteral("second")).value(QStringLiteral("ok")).toBool());
    QTRY_COMPARE_WITH_TIMEOUT(page.title(), QStringLiteral("watch:second"), 10000);

    QVariant events;
    done = false;
    page.runJavaScript(QStringLiteral("JSON.stringify(window.watchEvents)"),
                       [&events, &done](const QVariant &value) { events = value; done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
    const QJsonArray observed = QJsonDocument::fromJson(events.toString().toUtf8()).array();
    QCOMPARE(observed.size(), 2);
    QCOMPARE(observed.at(0).toString(), QStringLiteral("key:first:second"));
    QCOMPARE(observed.at(1).toString(), QStringLiteral("any:first:second"));

    done = false;
    page.runJavaScript(QStringLiteral("testStore.set('item', 'local')"),
                       [&done](const QVariant &) { done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
    QTRY_COMPARE_WITH_TIMEOUT(page.title(), QStringLiteral("watch:local"), 5000);
    QTest::qWait(2200);
    done = false;
    page.runJavaScript(QStringLiteral("JSON.stringify(window.watchEvents)"),
                       [&events, &done](const QVariant &value) { events = value; done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
    const QJsonArray afterLocal = QJsonDocument::fromJson(events.toString().toUtf8()).array();
    QCOMPARE(afterLocal.size(), 4);
    QCOMPARE(afterLocal.at(2).toString(), QStringLiteral("key:second:local"));
    QCOMPARE(afterLocal.at(3).toString(), QStringLiteral("any:second:local"));
    done = false;
    page.runJavaScript(QStringLiteral("testStore.set('item', 'local'); testStore.delete('missing');"),
                       [&done](const QVariant &) { done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
    done = false;
    page.runJavaScript(QStringLiteral("window.watchEvents.length"),
                       [&events, &done](const QVariant &value) { events = value; done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
    QCOMPARE(events.toInt(), 4);

    QVariant stopped;
    done = false;
    page.runJavaScript(QStringLiteral("offKey(); offAny(); testStore.watchTimer === null"),
                       [&stopped, &done](const QVariant &value) { stopped = value; done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
    QVERIFY(stopped.toBool());
    QVERIFY(writeExternal(QStringLiteral("third")).value(QStringLiteral("ok")).toBool());
    QTest::qWait(2200);
    QCOMPARE(page.title(), QStringLiteral("watch:local"));
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
