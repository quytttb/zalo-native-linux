#include "AppController.h"

#include "BridgeRouter.h"
#include "ConversationFilterModel.h"
#include "ConversationModel.h"
#include "MessageModel.h"
#include "RuntimeLocator.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QUuid>

namespace ZaloNative {

namespace {

constexpr int kMaxLogLines = 500;

QString shortPreview(const QString &text)
{
    return text.simplified().left(90);
}

} // namespace

AppController::AppController(const Options &options, QObject *parent)
    : QObject(parent)
    , m_options(options)
    , m_router(new BridgeRouter(this))
    , m_bridgeObject(new BridgeObject(m_router, this))
    , m_conversations(new ConversationModel(this))
    , m_filter(new ConversationFilterModel(this))
    , m_messages(new MessageModel(this))
{
    m_filter->setSourceModel(m_conversations);

    connect(m_router, &BridgeRouter::statePublished, this, &AppController::applyState);
    connect(m_router, &BridgeRouter::logLine, this, &AppController::appendLog);

    detectRuntime();

    if (m_options.demo) {
        QTimer::singleShot(0, this, &AppController::reloadDemo);
    }
}

QAbstractItemModel *AppController::conversations()
{
    return m_filter;
}

QAbstractItemModel *AppController::messages()
{
    return m_messages;
}

QObject *AppController::bridgeObject()
{
    return m_bridgeObject;
}

QString AppController::runtimeManifestJson() const
{
    return QString::fromUtf8(QJsonDocument(m_runtimeManifest).toJson(QJsonDocument::Indented));
}

bool AppController::rendererHostEnabled() const
{
#ifdef ZALO_HAVE_WEBENGINE
    return true;
#else
    return false;
#endif
}

QString AppController::rendererUrl() const
{
    if (!m_options.rendererUrl.isEmpty())
        return m_options.rendererUrl;
    return QStringLiteral("https://chat.zalo.me/");
}

QString AppController::appMode() const
{
    if (m_options.demo)
        return QStringLiteral("demo");
    if (m_options.webMode)
        return QStringLiteral("web");
    return QStringLiteral("native");
}

QString AppController::modeLabel() const
{
    if (m_options.demo)
        return QStringLiteral("Demo — dữ liệu giả lập");
    if (m_options.webMode)
        return QStringLiteral("Zalo Web thử nghiệm — không phải UI đích");
    if (!rendererHostEnabled())
        return QStringLiteral("Runtime native — bản build không có Qt WebEngine");
    return QStringLiteral("Runtime native — Qt WebEngine host");
}

QString AppController::sanitizedUrl(const QString &url) const
{
    // Never put query strings or fragments into the log: Zalo uses them for
    // session hand-off and they can contain credentials.
    QUrl parsed(url);
    if (!parsed.isValid())
        return url.left(200);
    parsed.setQuery(QString());
    parsed.setFragment(QString());
    return parsed.toString(QUrl::RemoveUserInfo);
}

void AppController::updateRendererState()
{
    const QUrl url(m_currentPageUrl);
    const QString host = url.host().toLower();

    if (m_rendererState == QStringLiteral("error"))
        return;
    if (m_rendererLoading)
        m_rendererState = QStringLiteral("loading");
    else if (host == QStringLiteral("id.zalo.me"))
        m_rendererState = QStringLiteral("login");
    else if (host == QStringLiteral("chat.zalo.me"))
        m_rendererState = QStringLiteral("ready");
    else if (host.isEmpty())
        m_rendererState = QStringLiteral("idle");
    else
        m_rendererState = QStringLiteral("browsing");

    emit rendererPageChanged();
}

void AppController::onRendererPageChanged(const QString &url, const QString &title)
{
    const QString safeUrl = sanitizedUrl(url);
    const bool changed = (safeUrl != m_currentPageUrl) || (title != m_rendererPageTitle);
    m_currentPageUrl = safeUrl;
    m_rendererPageTitle = title.left(200);
    if (!changed)
        return;

    if (!m_rendererLoading && m_rendererState != QStringLiteral("error")) {
        m_rendererState = QStringLiteral("idle");
        updateRendererState();
    } else {
        emit rendererPageChanged();
    }
    appendLog(QStringLiteral("web: %1").arg(safeUrl));
}

void AppController::onRendererLoadingChanged(bool loading)
{
    if (m_rendererLoading == loading)
        return;
    m_rendererLoading = loading;
    if (loading)
        m_rendererState = QStringLiteral("loading");
    else
        m_rendererState = QStringLiteral("idle");
    updateRendererState();
}

int AppController::conversationCount() const
{
    return m_conversations->rowCount();
}

int AppController::messageCount() const
{
    return m_messages->rowCount();
}

void AppController::clearRuntime()
{
    m_runtimeDetected = false;
    m_runtimeHasCore = false;
    m_runtimeSourceKind.clear();
    m_runtimeAppVersion.clear();
    m_runtimeAppDir.clear();
    m_runtimeEntryHtml.clear();
    m_runtimeSyncCallSites = 0;
    m_runtimeDynamicSyncCalls = 0;
    m_runtimeNativeLibraryCount = 0;
    m_runtimeWarnings.clear();
    m_runtimeExposedApis.clear();
    m_runtimeManifest = QJsonObject();
}

void AppController::detectRuntime()
{
    clearRuntime();

    const QString root = m_options.runtimeRoot.isEmpty() ? RuntimeLocator::defaultRoot()
                                                         : m_options.runtimeRoot;
    QString error;
    const auto manifest = RuntimeLocator::locate(root, &error);

    if (!manifest) {
        m_runtimeStatus = QStringLiteral(
            "Chưa có lõi Zalo PC trong %1.\n\n"
            "Đây là chế độ runtime native: ứng dụng không mở Zalo Web và không dùng dữ liệu demo. "
            "Hãy import một thư mục ứng dụng Zalo đã giải nén bằng:\n"
            "  zalort import <app-dir> ~/.local/share/zalo-native/runtime\n"
            "hoặc truyền --runtime <path>. Lõi Zalo vẫn chưa được tự động chạy.")
                              .arg(root);
        appendLog(QStringLiteral("runtime: not present under %1 (%2)").arg(root, error));
        m_router->setRuntimeInfo(m_runtimeManifest);
        emit runtimeStatusChanged();
        return;
    }

    m_runtimeDetected = true;
    m_runtimeHasCore = manifest->isRuntimeCore();
    m_runtimeManifest = manifest->toJson();
    m_runtimeSourceKind = manifest->sourceKind;
    m_runtimeAppVersion = manifest->appVersion;
    m_runtimeAppDir = manifest->appDir.isEmpty() ? manifest->runtimeRoot : manifest->appDir;
    m_runtimeEntryHtml = manifest->entryHtml;
    m_runtimeSyncCallSites = manifest->preloads.syncCallSites;
    m_runtimeDynamicSyncCalls = manifest->preloads.dynamicSyncCalls;
    m_runtimeNativeLibraryCount = manifest->nativeLibraries.size();
    m_runtimeWarnings = manifest->warnings;
    m_runtimeExposedApis = manifest->preloads.exposedApis;

    if (!m_runtimeHasCore) {
        m_runtimeStatus = QStringLiteral(
            "Đã đọc được ASAR launcher Zalo %1 nhưng không thấy entry HTML. "
            "Đây không phải lõi ứng dụng và không thể host. Hãy trỏ --runtime vào thư mục "
            "ứng dụng đã giải nén (ví dụ AppDir/app hoặc thư mục chứa pc-dist).")
                              .arg(manifest->appVersion.isEmpty()
                                       ? QStringLiteral("(version không rõ)")
                                       : manifest->appVersion);
    } else {
        m_runtimeStatus = QStringLiteral(
            "Đã nhận diện lõi Zalo PC %1 tại %2 (nguồn chưa xác minh, không phải bản Zalo Linux "
            "chính thức). Entry: %3. Preload: %4 API, %5 lời gọi sync (%6 kênh động). "
            "Qt host bootstrap chưa chạy; đây là màn hình chẩn đoán runtime, không phải chat.")
                              .arg(manifest->appVersion.isEmpty()
                                       ? QStringLiteral("(version không rõ)")
                                       : manifest->appVersion,
                                   m_runtimeAppDir,
                                   manifest->entryHtml)
                              .arg(manifest->preloads.exposedApis.size())
                              .arg(manifest->preloads.syncCallSites)
                              .arg(manifest->preloads.dynamicSyncCalls);
    }

    appendLog(QStringLiteral("runtime: %1 %2 (%3)")
                  .arg(manifest->sourceKind,
                       manifest->appVersion.isEmpty() ? QStringLiteral("?") : manifest->appVersion,
                       m_runtimeAppDir));
    if (!manifest->entryHtml.isEmpty())
        appendLog(QStringLiteral("runtime: entry %1").arg(manifest->entryHtml));
    appendLog(QStringLiteral("runtime: %1 preload API(s), %2 sync call site(s), %3 native lib(s)")
                  .arg(manifest->preloads.exposedApis.size())
                  .arg(manifest->preloads.syncCallSites)
                  .arg(manifest->nativeLibraries.size()));
    for (const QString &warning : manifest->warnings)
        appendLog(QStringLiteral("runtime: warning: %1").arg(warning));

    m_router->setRuntimeInfo(m_runtimeManifest);
    emit runtimeStatusChanged();
}

void AppController::reloadDemo()
{
    QFile file(QStringLiteral(":/qt/qml/ZaloNative/renderer/demo-state.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        appendLog(QStringLiteral("✗ demo: cannot open the bundled conversation fixture"));
        return;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        appendLog(QStringLiteral("✗ demo: fixture is invalid: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject request{
        {QStringLiteral("id"), QStringLiteral("demo")},
        {QStringLiteral("kind"), QStringLiteral("call")},
        {QStringLiteral("path"), QStringLiteral("state.publish")},
        {QStringLiteral("args"), QJsonArray{document.object()}},
    };
    const QJsonObject response = m_router->dispatch(request);
    if (!response.value(QStringLiteral("ok")).toBool())
        appendLog(QStringLiteral("✗ demo: state.publish was rejected"));
    else
        appendLog(QStringLiteral("demo: published the bundled fixture"));
}

void AppController::applyState(const QJsonObject &state)
{
    const QJsonArray conversations = state.value(QStringLiteral("conversations")).toArray();
    m_conversations->setConversations(conversations);

    m_messagesById.clear();
    const QJsonObject messages = state.value(QStringLiteral("messages")).toObject();
    for (auto it = messages.constBegin(); it != messages.constEnd(); ++it) {
        if (it.value().isArray())
            m_messagesById.insert(it.key(), it.value().toArray());
    }

    const bool currentStillExists = m_conversations->rowForId(m_currentId) >= 0;
    if (currentStillExists) {
        m_messages->setMessages(m_messagesById.value(m_currentId));
    } else if (m_conversations->rowCount() > 0) {
        openConversation(m_conversations->idAt(0));
    } else {
        m_currentId.clear();
        m_messages->clear();
        emit currentChanged();
    }

    appendLog(QStringLiteral("state: %1 conversations, %2 with messages")
                  .arg(conversations.size())
                  .arg(m_messagesById.size()));
    emit countsChanged();
}

void AppController::openConversation(const QString &id)
{
    const int row = m_conversations->rowForId(id);
    if (row < 0)
        return;

    const QModelIndex index = m_conversations->index(row, 0);
    m_currentId = id;
    m_currentTitle = index.data(ConversationModel::TitleRole).toString();
    m_currentAvatarText = index.data(ConversationModel::AvatarTextRole).toString();
    m_currentAvatarColor = index.data(ConversationModel::AvatarColorRole).toString();
    m_currentOnline = index.data(ConversationModel::OnlineRole).toBool();
    m_currentSubtitle = m_currentOnline ? QStringLiteral("Đang hoạt động")
                                        : QStringLiteral("Hoạt động gần đây");

    m_messages->setMessages(m_messagesById.value(id));
    m_conversations->markRead(id);
    emit currentChanged();
    emit countsChanged();
    appendLog(QStringLiteral("ui: opened conversation %1").arg(id));
}

bool AppController::sendMessage(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || m_currentId.isEmpty())
        return false;

    const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm"));
    const qint64 timestamp = QDateTime::currentSecsSinceEpoch();
    const QString id = QStringLiteral("local-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);

    const QJsonObject message{
        {QStringLiteral("id"), id},
        {QStringLiteral("sender"), QStringLiteral("Bạn")},
        {QStringLiteral("text"), trimmed},
        {QStringLiteral("time"), time},
        {QStringLiteral("outgoing"), true},
        {QStringLiteral("status"), QStringLiteral("sent")},
        {QStringLiteral("kind"), QStringLiteral("text")},
    };

    m_messagesById[m_currentId].append(message);
    m_messages->appendMessage(message);
    m_conversations->touch(m_currentId, QStringLiteral("Bạn: ") + shortPreview(trimmed), time, timestamp);

    emit countsChanged();
    appendLog(QStringLiteral("demo: queued a local message (%1 chars)").arg(trimmed.size()));
    return true;
}

void AppController::setSearchText(const QString &text)
{
    m_filter->setFilterText(text);
}

void AppController::refreshRuntime()
{
    detectRuntime();
}

void AppController::clearDiagnostics()
{
    m_log.clear();
    emit diagnosticsChanged();
}

void AppController::logRenderer(const QString &message)
{
    appendLog(QStringLiteral("renderer: %1").arg(message.left(1000)));
}

void AppController::onRendererLoaded()
{
    m_rendererLoading = false;
    if (m_rendererState == QStringLiteral("error"))
        m_rendererState = QStringLiteral("idle");
    updateRendererState();
    appendLog(QStringLiteral("webengine: page loaded"));
}

void AppController::onRendererFailed(const QString &error)
{
    m_rendererLoading = false;
    m_rendererState = QStringLiteral("error");
    appendLog(QStringLiteral("✗ webengine: %1").arg(error.left(1000)));
    emit rendererPageChanged();
}

void AppController::appendLog(const QString &line)
{
    const QString stamped = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))
        + QStringLiteral("  ") + line;
    m_log.append(stamped);
    while (m_log.size() > kMaxLogLines)
        m_log.removeFirst();
    emit diagnosticsChanged();
}

} // namespace ZaloNative
