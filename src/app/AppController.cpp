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
    return QStringLiteral("qrc:/qt/qml/ZaloNative/renderer/stub.html");
}

int AppController::conversationCount() const
{
    return m_conversations->rowCount();
}

int AppController::messageCount() const
{
    return m_messages->rowCount();
}

void AppController::detectRuntime()
{
    const QString root = m_options.runtimeRoot.isEmpty() ? RuntimeLocator::defaultRoot()
                                                         : m_options.runtimeRoot;
    QString error;
    const auto manifest = RuntimeLocator::locate(root, &error);

    if (manifest) {
        m_runtimeDetected = true;
        m_runtimeManifest = manifest->toJson();
        m_runtimeStatus = QStringLiteral("Đã thấy runtime %1 %2 — tích hợp renderer ở mốc 2")
                              .arg(manifest->name.isEmpty() ? QStringLiteral("Zalo") : manifest->name,
                                   manifest->appVersion);
        appendLog(QStringLiteral("runtime: found %1 (%2)").arg(manifest->asarPath, manifest->entryHtml));
        for (const QString &warning : manifest->warnings)
            appendLog(QStringLiteral("runtime: warning: %1").arg(warning));
    } else {
        m_runtimeDetected = false;
        m_runtimeStatus = QStringLiteral("Chưa có runtime Zalo — đang chạy dữ liệu demo");
        appendLog(QStringLiteral("runtime: not found under %1 (%2)").arg(root, error));
    }

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
    appendLog(QStringLiteral("webengine: renderer page loaded"));
}

void AppController::onRendererFailed(const QString &error)
{
    appendLog(QStringLiteral("✗ webengine: %1").arg(error.left(1000)));
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
