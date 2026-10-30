#include "HostApi.h"
#include "LocalScheme.h"
#include "RuntimeLocator.h"
#include "RuntimePagePolicy.h"

#include <QBuffer>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <cstdio>
#include <memory>

namespace {

constexpr int kDefaultWaitMs = 7000;

QString schemeModeFromCommandLine(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        const QByteArray arg(argv[i]);
        if (arg == "--scheme-mode" && i + 1 < argc)
            return QString::fromUtf8(argv[++i]);
        if (arg.startsWith("--scheme-mode="))
            return QString::fromUtf8(arg.mid(int(qstrlen("--scheme-mode="))));
    }
    return QStringLiteral("local");
}

QString safeSource(const QString &source)
{
    const QUrl url(source);
    if (!url.isValid())
        return QStringLiteral("(invalid source)");
    return url.fileName().isEmpty() ? url.host() : url.fileName();
}

class RuntimeSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT

public:
    explicit RuntimeSchemeHandler(QString root, bool rewriteCsp,
                                  ZaloNative::HostApi *hostApi = nullptr,
                                  QObject *parent = nullptr)
        : QWebEngineUrlSchemeHandler(parent)
        , m_root(std::move(root))
        , m_rewriteCsp(rewriteCsp)
        , m_hostApi(hostApi)
    {
    }

    void requestStarted(QWebEngineUrlRequestJob *job) override
    {
        if (job->requestUrl().path() == QLatin1String("/__zalonative_host")) {
            handleHostRequest(job);
            return;
        }

        QString error;
        const auto resolved = ZaloNative::LocalScheme::resolve(m_root, job->requestUrl(), &error);
        if (!resolved) {
            m_errors.append(error);
            job->fail(ZaloNative::LocalScheme::resolve(m_root, job->requestUrl(), nullptr)
                          ? QWebEngineUrlRequestJob::RequestFailed
                          : QWebEngineUrlRequestJob::UrlNotFound);
            return;
        }

        auto *buffer = new QBuffer(job);
        QFile file(resolved->absolutePath);
        if (!file.open(QIODevice::ReadOnly)) {
            m_errors.append(QStringLiteral("cannot read %1").arg(resolved->absolutePath));
            job->fail(QWebEngineUrlRequestJob::RequestFailed);
            return;
        }
        QByteArray data = file.read(64 * 1024 * 1024);
        if (m_rewriteCsp && resolved->mimeType.startsWith(QLatin1String("text/html"))) {
            const auto rewritten =
                ZaloNative::RuntimePagePolicy::allowSchemeInCsp(data, QByteArrayLiteral("zalo-app"));
            if (rewritten.changed) {
                data = rewritten.html;
                m_cspRewritten = true;
            }
        }
        buffer->setData(data);
        job->reply(resolved->mimeType.toUtf8(), buffer);
    }

    QStringList errors() const { return m_errors; }
    bool cspRewritten() const { return m_cspRewritten; }
    int hostRequestCount() const { return m_hostRequestCount; }

private:
    void handleHostRequest(QWebEngineUrlRequestJob *job)
    {
        if (!m_hostApi || job->requestMethod() != QByteArrayLiteral("GET")) {
            job->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
        }

        const QUrlQuery query(job->requestUrl());
        const QByteArray payload
            = query.queryItemValue(QStringLiteral("request"), QUrl::FullyDecoded).toUtf8();
        if (payload.isEmpty() || payload.size() > 512 * 1024) {
            m_errors.append(QStringLiteral("invalid host request payload"));
            job->fail(QWebEngineUrlRequestJob::RequestFailed);
            return;
        }

        m_hostRequestCount++;
        const QJsonObject response = m_hostApi->dispatchJson(QString::fromUtf8(payload));
        auto *buffer = new QBuffer(job);
        buffer->setData(QJsonDocument(response).toJson(QJsonDocument::Compact));
        job->reply(QByteArrayLiteral("application/json"), buffer);
    }

    QString m_root;
    bool m_rewriteCsp = false;
    bool m_cspRewritten = false;
    ZaloNative::HostApi *m_hostApi = nullptr;
    int m_hostRequestCount = 0;
    QStringList m_errors;
};

class RequestRecorder : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

public:
    RequestRecorder(bool blockRemote, QObject *parent = nullptr)
        : QWebEngineUrlRequestInterceptor(parent)
        , m_blockRemote(blockRemote)
    {
    }

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        const QUrl url = info.requestUrl();
        const QString scheme = url.scheme().toLower();
        if (scheme == QLatin1String("data") || scheme == QLatin1String("blob")
            || scheme == QLatin1String("qrc")) {
            return;
        }
        QString key = scheme + QStringLiteral("://") + url.host();
        if (scheme == QLatin1String("zalo-app")) {
            key = QStringLiteral("zalo-app (local runtime)");
        } else if (m_blockRemote) {
            info.block(true);
            m_blockedCount++;
        }
        m_hosts.insert(key);
    }

    QStringList hosts() const
    {
        QStringList out(m_hosts.begin(), m_hosts.end());
        out.sort();
        return out;
    }

    int blockedCount() const { return m_blockedCount; }

private:
    QSet<QString> m_hosts;
    bool m_blockRemote = false;
    int m_blockedCount = 0;
};

class ProbePage : public QWebEnginePage
{
    Q_OBJECT

public:
    using QWebEnginePage::QWebEnginePage;

    QStringList consoleLines;

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override
    {
        Q_UNUSED(message);
        const QString prefix = level == ErrorMessageLevel ? QStringLiteral("error")
            : (level == WarningMessageLevel ? QStringLiteral("warn") : QStringLiteral("info"));
        consoleLines.append(QStringLiteral("%1 %2:%3 [message omitted]")
                                .arg(prefix, safeSource(sourceID))
                                .arg(lineNumber));
        while (consoleLines.size() > 200)
            consoleLines.removeFirst();
    }
};

QWebEngineScript collectorScript()
{
    QWebEngineScript script;
    script.setName(QStringLiteral("zalonative-probe-collector"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(QStringLiteral(R"(
(function () {
    window.__zaloProbe = { errors: [], rejections: [], idb: { state: 'pending' } };
    function classify(value) {
        var text = String(value || '');
        if (text.indexOf('ERR_HOST_UNSUPPORTED') >= 0 || text.indexOf('is not implemented') >= 0)
            return 'host operation unsupported';
        if (text.indexOf('Cannot inject the dependency') >= 0)
            return 'dependency injection failure';
        if (text.indexOf('Network Error') >= 0)
            return 'network error';
        if (text.indexOf('loadShellQos') >= 0)
            return 'missing loadShellQos';
        if (text.indexOf('$zwindow') >= 0)
            return 'missing $zwindow';
        return 'other error (details omitted)';
    }
    try {
        var open = indexedDB.open('__zalonative_probe', 1);
        open.onsuccess = function () {
            try {
                window.__zaloProbe.idb = { state: 'ok', version: open.result.version };
                open.result.close();
                indexedDB.deleteDatabase('__zalonative_probe');
            } catch (e) {
                window.__zaloProbe.idb = { state: 'ok-cleanup-failed' };
            }
        };
        open.onerror = function () {
            window.__zaloProbe.idb = { state: 'error' };
        };
        open.onblocked = function () {
            window.__zaloProbe.idb = { state: 'blocked' };
        };
    } catch (e) {
        window.__zaloProbe.idb = { state: 'exception' };
    }
    window.addEventListener('error', function (event) {
        try {
            window.__zaloProbe.errors.push(classify(event.message));
        } catch (e) {}
    });
    window.addEventListener('unhandledrejection', function (event) {
        try {
            window.__zaloProbe.rejections.push(classify(event.reason));
        } catch (e) {}
    });
})();
)"));
    return script;
}

QString captureScript(const QStringList &apiNames)
{
    QJsonArray names;
    for (const QString &name : apiNames)
        names.append(name);
    const QString namesJson = QString::fromUtf8(QJsonDocument(names).toJson(QJsonDocument::Compact));

    return QStringLiteral(R"(
(function () {
    var names = %1;
    var missing = {};
    names.forEach(function (name) {
        try { missing[name] = typeof window[name]; }
        catch (e) { missing[name] = 'error:' + e.name; }
    });
    var storage = {};
    try { storage.localStorage = (function () {
        localStorage.setItem('__zalo_probe', '1');
        localStorage.removeItem('__zalo_probe');
        return 'ok';
    })(); } catch (e) { storage.localStorage = 'blocked:' + e.name; }
    storage.indexedDB = typeof indexedDB;
    storage.caches = typeof caches;
    storage.serviceWorker = typeof navigator.serviceWorker;
    storage.crossOriginIsolated = window.crossOriginIsolated === true;
    var probe = window.__zaloProbe || { errors: [], rejections: [] };
    var stubs = window.__zaloStubPreload
        ? { enabled: true, calls: window.__zaloStubPreload.calls,
            unknown: window.__zaloStubPreload.unknown }
        : { enabled: false, calls: [], unknown: [] };
    return JSON.stringify({
        url: location.href,
        title: document.title,
        readyState: document.readyState,
        errors: probe.errors,
        rejections: probe.rejections,
        missing: missing,
        storage: storage,
        idb: probe.idb,
        stubs: stubs,
        bodyLength: document.body ? document.body.innerHTML.length : 0
    });
})();
)").arg(namesJson);
}

QWebEngineScript stubPreloadScript(const QStringList &autoNames)
{
    // Diagnostic only. These are deliberately non-functional stubs: they record
    // which members the runtime touches and return neutral values so bootstrap
    // can proceed far enough to reveal the next contract requirement. They do
    // not implement IPC, storage, login or any privileged behavior.
    QJsonArray names;
    for (const QString &name : autoNames) {
        if (name == QLatin1String("$zlogger") || name == QLatin1String("$zwindow"))
            continue;
        names.append(name);
    }
    const QString namesJson = QString::fromUtf8(QJsonDocument(names).toJson(QJsonDocument::Compact));

    QWebEngineScript script;
    script.setName(QStringLiteral("zalonative-probe-stub-preload"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(QStringLiteral(R"(
(function () {
    if (window.__zaloStubPreload)
        return;
    var state = window.__zaloStubPreload = {
        note: 'diagnostic stubs only; no IPC, storage, login or privileged behavior',
        calls: [],
        unknown: []
    };
    function describe(value) {
        try {
            if (value === undefined) return 'undefined';
            if (value === null) return 'null';
            var type = typeof value;
            if (type === 'string') {
                // Never copy arbitrary values into the probe log: they can be
                // cookies, tokens, paths or message content. Keep a small set of
                // non-sensitive Electron path names for contract discovery.
                if (/^(userData|appData|documents|downloads|home|temp|exe|desktop|logs|crashDumps)$/.test(value))
                    return JSON.stringify(value);
                return 'string(' + value.length + ')';
            }
            if (type === 'number' || type === 'boolean') return String(value);
            if (type === 'function') return 'function';
            if (type === 'object') {
                if (Array.isArray(value)) return 'array(' + value.length + ')';
                return 'object(' + Object.keys(value).length + ' keys)';
            }
            return type;
        } catch (e) {
            return 'unreadable';
        }
    }
    function record(owner, member, kind, args) {
        if (state.calls.length >= 300)
            return;
        var rendered = [];
        for (var i = 0; i < Math.min(args ? args.length : 0, 4); ++i)
            rendered.push(describe(args[i]));
        state.calls.push(owner + '.' + member + ' ' + kind + '(' + rendered.join(', ') + ')');
    }
    function makeProxy(owner, api) {
        return new Proxy(api, {
            get: function (target, prop, receiver) {
                if (typeof prop !== 'string')
                    return Reflect.get(target, prop, receiver);
                if (prop in target)
                    return Reflect.get(target, prop, receiver);
                if (prop === 'then' || prop === 'toJSON')
                    return undefined;
                if (state.unknown.length < 120 && state.unknown.indexOf(owner + '.' + prop) < 0)
                    state.unknown.push(owner + '.' + prop);
                return function () {
                    record(owner, prop, 'unknown', arguments);
                    return Promise.resolve(undefined);
                };
            },
            set: function (target, prop, value) {
                if (typeof prop === 'string')
                    record(owner, prop, 'set', [value]);
                return Reflect.set(target, prop, value);
            }
        });
    }
    function makeAnyProxy(owner) {
        var target = function () {
            record(owner, '(self)', 'call', arguments);
            return Promise.resolve(undefined);
        };
        return makeProxy(owner, target);
    }
    var logger = {
        sendLog: function () { record('$zlogger', 'sendLog', 'call', arguments); },
        sendLogGa: function () { record('$zlogger', 'sendLogGa', 'call', arguments); },
        sendViewerkey: function () { record('$zlogger', 'sendViewerkey', 'call', arguments); },
        onReceiveViewerkey: function () { record('$zlogger', 'onReceiveViewerkey', 'call', arguments); },
        removeListenViewerkey: function () { record('$zlogger', 'removeListenViewerkey', 'call', arguments); },
        loadShellQos: function () {
            record('$zlogger', 'loadShellQos', 'call', arguments);
            return Promise.resolve({});
        },
        checkRecentCrash: function () {
            record('$zlogger', 'checkRecentCrash', 'call', arguments);
            return Promise.resolve({ crashed: false });
        }
    };
    var zwindow = {
        setLoginSize: function () {
            record('$zwindow', 'setLoginSize', 'call', arguments);
            return Promise.resolve(true);
        },
        focus: function () {
            record('$zwindow', 'focus', 'call', arguments);
            return Promise.resolve(true);
        },
        onVisibilityChange: function () { record('$zwindow', 'onVisibilityChange', 'call', arguments); },
        removeListenVisibilityChange: function () { record('$zwindow', 'removeListenVisibilityChange', 'call', arguments); },
        showSaveDialog: function () {
            record('$zwindow', 'showSaveDialog', 'call', arguments);
            return Promise.resolve({ canceled: true });
        },
        showOpenDialog: function () {
            record('$zwindow', 'showOpenDialog', 'call', arguments);
            return Promise.resolve({ canceled: true, filePaths: [] });
        }
    };
    var electronNative = {
        getResourceUsage: function () {
            record('$zelectronNative', 'getResourceUsage', 'call', arguments);
            return { images: { size: 0 } };
        },
        clearCache: function () { record('$zelectronNative', 'clearCache', 'call', arguments); },
        getAvailableHeapSize: function () {
            record('$zelectronNative', 'getAvailableHeapSize', 'call', arguments);
            return 0;
        },
        setZoomFactor: function () { record('$zelectronNative', 'setZoomFactor', 'call', arguments); },
        lockWebframeZoom: function () {
            record('$zelectronNative', 'lockWebframeZoom', 'call', arguments);
        },
        getClipboardText: function () { record('$zelectronNative', 'getClipboardText', 'call', arguments); return ''; },
        writeTextToClipboard: function () {
            record('$zelectronNative', 'writeTextToClipboard', 'call', arguments);
        },
        getClipboardImage: function () {
            record('$zelectronNative', 'getClipboardImage', 'call', arguments);
            return null;
        },
        writeImageToClipboard: function () {
            record('$zelectronNative', 'writeImageToClipboard', 'call', arguments);
            return true;
        }
    };
    // Minimal POSIX-style path helper for diagnostics. It never touches disk.
    function normalizePath(value) {
        var input = String(value).replace(/\\/g, '/');
        var absolute = input.charAt(0) === '/';
        var parts = input.split('/');
        var out = [];
        parts.forEach(function (part) {
            if (!part || part === '.')
                return;
            if (part === '..') {
                if (out.length && out[out.length - 1] !== '..')
                    out.pop();
                else if (!absolute)
                    out.push('..');
                return;
            }
            out.push(part);
        });
        var joined = out.join('/');
        if (absolute)
            return '/' + joined;
        return joined || '.';
    }
    var pathModule = {
        sep: '/',
        delimiter: ':',
        normalize: normalizePath,
        join: function () {
            var args = Array.prototype.slice.call(arguments).filter(function (v) { return typeof v === 'string'; });
            return normalizePath(args.join('/'));
        },
        resolve: function () {
            var resolved = '';
            for (var i = arguments.length - 1; i >= -1; --i) {
                var part = i >= 0 ? arguments[i] : '/';
                if (typeof part !== 'string')
                    continue;
                resolved = part + '/' + resolved;
                if (part.charAt(0) === '/')
                    break;
            }
            return normalizePath('/' + resolved);
        },
        isAbsolute: function (value) { return String(value).charAt(0) === '/'; },
        dirname: function (value) {
            var normalized = normalizePath(value);
            var index = normalized.lastIndexOf('/');
            if (index < 0) return '.';
            if (index === 0) return '/';
            return normalized.slice(0, index);
        },
        basename: function (value, ext) {
            var normalized = normalizePath(value);
            var name = normalized.slice(normalized.lastIndexOf('/') + 1);
            if (ext && name.slice(-ext.length) === ext)
                name = name.slice(0, -ext.length);
            return name;
        },
        extname: function (value) {
            var name = pathModule.basename(value);
            var index = name.lastIndexOf('.');
            return index > 0 ? name.slice(index) : '';
        }
    };
    var stubBase = '/tmp/opencode/zalo-probe-home';
    function stubPath(name) {
        return stubBase + '/' + String(name || 'userData');
    }
    var storeData = {};
    var electron = {
        getPath: function (name) {
            record('$zelectron', 'getPath', 'call', arguments);
            return Promise.resolve(stubPath(name));
        },
        getPathSync: function (name) {
            record('$zelectron', 'getPathSync', 'call', arguments);
            return stubPath(name);
        },
        getAppPath: function () {
            record('$zelectron', 'getAppPath', 'call', arguments);
            return Promise.resolve(stubBase + '/app');
        },
        getAppPathSync: function () {
            record('$zelectron', 'getAppPathSync', 'call', arguments);
            return stubBase + '/app';
        },
        getAppVersion: function () {
            record('$zelectron', 'getAppVersion', 'call', arguments);
            return 'probe-stub';
        },
        getElectronStore: function (name) {
            record('$zelectron', 'getElectronStore', 'call', arguments);
            var key = String(name || 'default');
            storeData[key] = storeData[key] || {};
            return {
                get: function (k, fallback) {
                    return Object.prototype.hasOwnProperty.call(storeData[key], k)
                        ? storeData[key][k] : fallback;
                },
                set: function (k, v) { storeData[key][k] = v; },
                has: function (k) { return Object.prototype.hasOwnProperty.call(storeData[key], k); },
                delete: function (k) { delete storeData[key][k]; }
            };
        },
        setCookie: function () { record('$zelectron', 'setCookie', 'call', arguments); }
    };
    var node = {
        path: pathModule,
        fs: makeAnyProxy('$znode.fs'),
        fsExtra: makeAnyProxy('$znode.fsExtra'),
        os: {
            homedir: function () { return stubBase; },
            tmpdir: function () { return '/tmp/opencode'; },
            platform: function () { return 'linux'; },
            type: function () { return 'Linux'; },
            arch: function () { return 'x64'; },
            release: function () { return ''; },
            hostname: function () { return ''; },
            networkInterfaces: function () { return {}; },
            cpus: function () {
                var count = (navigator && navigator.hardwareConcurrency) || 1;
                var list = [];
                for (var i = 0; i < count; ++i)
                    list.push({ model: 'unknown', speed: 0,
                                times: { user: 0, nice: 0, sys: 0, idle: 0, irq: 0 } });
                return list;
            }
        },
        process: {
            platform: 'linux',
            cwd: function () { return stubBase; },
            env: {},
            versions: {}
        }
    };
    function makeNamespace(owner, members) {
        var namespace = {};
        members.forEach(function (member) {
            namespace[member] = makeAnyProxy(owner + '.' + member);
        });
        return makeProxy(owner, namespace);
    }
    var autoNames = %1;
    autoNames.forEach(function (name) {
        if (name === '$zlogger' || name === '$zwindow' || name === '$znode'
            || name === '$zelectronNative'
            || name === '$zelectron' || name === '$zFeatures' || name === '$zTFModels')
            return;
        try {
            if (name in window && window[name])
                return;
            window[name] = makeAnyProxy(name);
        } catch (e) {}
    });
    window.$zlogger = makeProxy('$zlogger', logger);
    window.$zwindow = makeProxy('$zwindow', zwindow);
    if (!window.$zelectronNative)
        window.$zelectronNative = makeProxy('$zelectronNative', electronNative);
    if (!(window.$znode && window.$znode.__zalonativeHostApi === 1))
        window.$znode = makeProxy('$znode', node);
    if (!(window.$znode && window.$znode.__zalonativeHostApi === 1))
        window.$zelectron = makeProxy('$zelectron', electron);
    window.$zFeatures = makeNamespace('$zFeatures', [
        'activeDeactive', 'mainless', 'syncDownload', 'zwalker', 'fileWalkerStream',
        'kikiAssistant', 'migrateDB'
    ]);
    window.$zTFModels = makeNamespace('$zTFModels', ['ss']);
})();
)").arg(namesJson));
    return script;
}

bool hostPreloadScript(const QString &appRoot, QWebEngineScript *script, QString *error)
{
    QFile file(QStringLiteral(ZALO_SOURCE_DIR) + QStringLiteral("/renderer/host/preload.js"));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("cannot read renderer/host/preload.js");
        return false;
    }
    const QByteArray source = file.readAll();
    if (source.isEmpty()) {
        if (error)
            *error = QStringLiteral("renderer/host/preload.js is empty");
        return false;
    }

    const QJsonObject config{
        {QStringLiteral("cwd"), appRoot},
        {QStringLiteral("platform"), QStringLiteral("linux")},
        {QStringLiteral("arch"), QStringLiteral("x64")},
    };
    const QByteArray configJson = QJsonDocument(config).toJson(QJsonDocument::Compact);

    script->setName(QStringLiteral("zalonative-probe-host-preload"));
    script->setInjectionPoint(QWebEngineScript::DocumentCreation);
    script->setWorldId(QWebEngineScript::MainWorld);
    script->setRunsOnSubFrames(false);
    script->setSourceCode(QStringLiteral("window.__zalonativeHostConfig = ")
                          + QString::fromUtf8(configJson) + QStringLiteral(";\n")
                          + QString::fromUtf8(source));
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    const QString schemeMode = schemeModeFromCommandLine(argc, argv);
    QWebEngineUrlScheme::Flags schemeFlags = QWebEngineUrlScheme::SecureScheme
        | QWebEngineUrlScheme::LocalScheme
        | QWebEngineUrlScheme::LocalAccessAllowed
        | QWebEngineUrlScheme::CorsEnabled;
    if (schemeMode == QLatin1String("standard")) {
        // Diagnostic alternative: keep a host-bearing, secure origin without
        // marking the scheme as "local" (which can make its origin opaque).
        schemeFlags = QWebEngineUrlScheme::SecureScheme
            | QWebEngineUrlScheme::LocalAccessAllowed
            | QWebEngineUrlScheme::CorsEnabled;
    } else if (schemeMode == QLatin1String("csp-ignored")) {
        // Diagnostic only: proves whether CSP is the blocker. Never a default.
        schemeFlags |= QWebEngineUrlScheme::ContentSecurityPolicyIgnored;
    } else if (schemeMode != QLatin1String("local")) {
        std::fprintf(stderr,
                     "zalo-bootstrap-probe: unknown --scheme-mode '%s' "
                     "(expected local, standard or csp-ignored)\n",
                     qUtf8Printable(schemeMode));
        return 2;
    }

    QWebEngineUrlScheme scheme(QByteArrayLiteral("zalo-app"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(443);
    scheme.setFlags(schemeFlags);
    QWebEngineUrlScheme::registerScheme(scheme);

    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("zalo-bootstrap-probe"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Load a user-supplied Zalo runtime entry in Qt WebEngine without host "
                       "APIs, record QML/JS/network/storage blockers, and exit."));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("runtime"), QStringLiteral("Extracted runtime or app-dir."),
                      QStringLiteral("path")});
    parser.addOption({QStringLiteral("wait-ms"), QStringLiteral("Time to observe after load."),
                      QStringLiteral("ms"), QString::number(kDefaultWaitMs)});
    parser.addOption({QStringLiteral("scheme-mode"),
                      QStringLiteral("Diagnostic scheme flags: local (default), standard or "
                                     "csp-ignored."),
                      QStringLiteral("mode"), QStringLiteral("local")});
    parser.addOption({QStringLiteral("block-remote"),
                      QStringLiteral("Block every non-local network request during the probe.")});
    parser.addOption({QStringLiteral("rewrite-csp"),
                      QStringLiteral("In memory only: add the local runtime scheme to the page "
                                     "CSP before serving it.")});
    parser.addOption({QStringLiteral("stub-preload"),
                      QStringLiteral("Diagnostic only: inject recording, non-functional preload "
                                     "stubs (logger/window/path/electron and remaining names); "
                                     "no IPC, storage, login or privileged behavior.")});
    parser.addOption({QStringLiteral("host-api"),
                      QStringLiteral("Diagnostic only: inject the bounded renderer/host/preload.js "
                                     "and serve app-data-scoped host calls from an isolated "
                                     "temporary directory. Requires --block-remote and "
                                     "--rewrite-csp; never logs in.")});
    parser.process(app);

    const bool stubPreload = parser.isSet(QStringLiteral("stub-preload"));
    if (stubPreload && !parser.isSet(QStringLiteral("block-remote"))) {
        std::fprintf(stderr,
                     "zalo-bootstrap-probe: --stub-preload requires --block-remote; "
                     "diagnostic stubs must not trigger network or login attempts\n");
        return 2;
    }

    const bool hostApiEnabled = parser.isSet(QStringLiteral("host-api"));
    if (hostApiEnabled
        && (!parser.isSet(QStringLiteral("block-remote"))
            || !parser.isSet(QStringLiteral("rewrite-csp")))) {
        std::fprintf(stderr,
                     "zalo-bootstrap-probe: --host-api requires --block-remote and "
                     "--rewrite-csp; the host layer is diagnostic and must not use the "
                     "network\n");
        return 2;
    }

    const QString runtimePath = parser.value(QStringLiteral("runtime"));
    if (runtimePath.isEmpty()) {
        std::fprintf(stderr, "zalo-bootstrap-probe: --runtime <app-dir> is required\n");
        return 2;
    }

    QString error;
    const auto manifest = ZaloNative::RuntimeLocator::locate(runtimePath, &error);
    if (!manifest) {
        std::fprintf(stderr, "zalo-bootstrap-probe: %s\n", qUtf8Printable(error));
        return 2;
    }
    if (!manifest->isRuntimeCore()) {
        std::fprintf(stderr, "zalo-bootstrap-probe: the selected runtime has no entry HTML\n");
        return 2;
    }

    const QString root = manifest->appDir.isEmpty() ? manifest->runtimeRoot : manifest->appDir;
    const QUrl entry(QStringLiteral("zalo-app://runtime/") + manifest->entryHtml);
    const QStringList apiNames = manifest->preloads.exposedApis;

    std::unique_ptr<QTemporaryDir> hostDataDir;
    std::unique_ptr<ZaloNative::HostApi> hostApi;
    QWebEngineScript realPreload;
    if (hostApiEnabled) {
        hostDataDir = std::make_unique<QTemporaryDir>();
        if (!hostDataDir->isValid()) {
            std::fprintf(stderr, "zalo-bootstrap-probe: cannot create an isolated host data root\n");
            return 2;
        }
        hostApi = std::make_unique<ZaloNative::HostApi>(
            hostDataDir->path() + QStringLiteral("/data"), root, manifest->appVersion);

        QString preloadError;
        if (!hostPreloadScript(root, &realPreload, &preloadError)) {
            std::fprintf(stderr, "zalo-bootstrap-probe: %s\n", qUtf8Printable(preloadError));
            return 2;
        }
    }

    QWebEngineProfile profile;
    auto *handler = new RuntimeSchemeHandler(root, parser.isSet(QStringLiteral("rewrite-csp")),
                                             hostApi.get(), &profile);
    profile.installUrlSchemeHandler(QByteArrayLiteral("zalo-app"), handler);
    auto *recorder = new RequestRecorder(parser.isSet(QStringLiteral("block-remote")), &profile);
    profile.setUrlRequestInterceptor(recorder);

    ProbePage page(&profile);
    page.scripts().insert(collectorScript());
    if (hostApiEnabled)
        page.scripts().insert(realPreload);
    if (stubPreload)
        page.scripts().insert(stubPreloadScript(apiNames));
    page.settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    page.settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    page.settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    page.settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);

    QJsonObject summary{
        {QStringLiteral("probe"), QStringLiteral("zalonative.bootstrap/1")},
        {QStringLiteral("runtimeRoot"), root},
        {QStringLiteral("appVersion"), manifest->appVersion},
        {QStringLiteral("entryHtml"), manifest->entryHtml},
        {QStringLiteral("entryUrl"), entry.toString()},
        {QStringLiteral("sourceVerified"), false},
        {QStringLiteral("schemeMode"), schemeMode},
        {QStringLiteral("hostApisInjected"), hostApiEnabled},
        {QStringLiteral("hostApiMode"),
         hostApiEnabled ? QStringLiteral("bounded-diagnostic") : QStringLiteral("none")},
        {QStringLiteral("diagnosticStubsInjected"), stubPreload},
        {QStringLiteral("remoteRequestsBlocked"), parser.isSet(QStringLiteral("block-remote"))},
        {QStringLiteral("cspRewriteRequested"), parser.isSet(QStringLiteral("rewrite-csp"))},
        {QStringLiteral("note"),
         hostApiEnabled
             ? QStringLiteral("The real renderer/host/preload.js is injected against an isolated "
                              "temporary data root. Only bounded app-data filesystem/store calls are "
                              "served; unsupported APIs fail explicitly. Remote requests are blocked "
                              "and this probe does not authenticate or log in.")
             : QStringLiteral("No preload or host APIs are injected. Diagnostic stubs, when "
                              "enabled, are no-op/canceled and record calls only. Failures below are "
                              "expected and identify blockers; this probe does not authenticate or "
                              "log in.")},
    };

    bool captured = false;
    auto capture = [&](bool loaded, const QString &loadError) {
        if (captured)
            return;
        captured = true;
        page.runJavaScript(captureScript(apiNames), [&, loaded, loadError](const QVariant &value) {
            QJsonObject result{
                {QStringLiteral("loaded"), loaded},
                {QStringLiteral("loadError"), loadError},
                {QStringLiteral("finalUrl"), page.url().toString()},
                {QStringLiteral("title"), page.title()},
                {QStringLiteral("console"), QJsonArray::fromStringList(page.consoleLines)},
                {QStringLiteral("requestHosts"), QJsonArray::fromStringList(recorder->hosts())},
                {QStringLiteral("blockedRemoteCount"), recorder->blockedCount()},
                {QStringLiteral("cspRewritten"), handler->cspRewritten()},
                {QStringLiteral("hostRequestCount"), handler->hostRequestCount()},
                {QStringLiteral("profileOffTheRecord"), profile.isOffTheRecord()},
                {QStringLiteral("hostOperations"),
                 QJsonArray::fromStringList(hostApi ? hostApi->operations() : QStringList())},
                {QStringLiteral("schemeErrors"), QJsonArray::fromStringList(handler->errors())},
            };
            if (value.isValid()) {
                const QJsonDocument observed = QJsonDocument::fromJson(value.toString().toUtf8());
                if (observed.isObject())
                    result.insert(QStringLiteral("observed"), observed.object());
            }
            summary.insert(QStringLiteral("result"), result);
            QTextStream out(stdout);
            out << QString::fromUtf8(QJsonDocument(summary).toJson(QJsonDocument::Indented));
            QCoreApplication::quit();
        });
    };

    QObject::connect(&page, &QWebEnginePage::loadFinished, &app,
                     [&](bool ok) {
                         QTimer::singleShot(parser.value(QStringLiteral("wait-ms")).toInt(),
                                            &app, [&, ok]() { capture(ok, ok ? QString() : QStringLiteral("loadFinished(false)")); });
                     });
    QObject::connect(&page, &QWebEnginePage::renderProcessTerminated, &app,
                     [&](QWebEnginePage::RenderProcessTerminationStatus status, int exitCode) {
                         capture(false, QStringLiteral("render process terminated: %1, exit %2")
                                            .arg(int(status))
                                            .arg(exitCode));
                     });

    // Hard stop even if the page never emits loadFinished.
    QTimer::singleShot(30000, &app, [&]() {
        capture(false, QStringLiteral("probe timeout"));
    });

    page.setUrl(entry);
    return app.exec();
}

#include "main.moc"
