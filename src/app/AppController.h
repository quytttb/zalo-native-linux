#pragma once

#include <QAbstractItemModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

namespace ZaloNative {

class BridgeObject;
class BridgeRouter;
class ConversationFilterModel;
class ConversationModel;
class MessageModel;

class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel *conversations READ conversations CONSTANT)
    Q_PROPERTY(QAbstractItemModel *messages READ messages CONSTANT)
    Q_PROPERTY(QObject *bridgeObject READ bridgeObject CONSTANT)

    Q_PROPERTY(QString currentId READ currentId NOTIFY currentChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentChanged)
    Q_PROPERTY(QString currentSubtitle READ currentSubtitle NOTIFY currentChanged)
    Q_PROPERTY(QString currentAvatarText READ currentAvatarText NOTIFY currentChanged)
    Q_PROPERTY(QString currentAvatarColor READ currentAvatarColor NOTIFY currentChanged)
    Q_PROPERTY(bool currentOnline READ currentOnline NOTIFY currentChanged)

    Q_PROPERTY(QString runtimeStatus READ runtimeStatus NOTIFY runtimeStatusChanged)
    Q_PROPERTY(bool runtimeDetected READ runtimeDetected NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString runtimeManifestJson READ runtimeManifestJson NOTIFY runtimeStatusChanged)
    Q_PROPERTY(int conversationCount READ conversationCount NOTIFY countsChanged)
    Q_PROPERTY(int messageCount READ messageCount NOTIFY countsChanged)
    Q_PROPERTY(bool demoMode READ demoMode CONSTANT)
    Q_PROPERTY(bool rendererHostEnabled READ rendererHostEnabled CONSTANT)
    Q_PROPERTY(QString rendererUrl READ rendererUrl CONSTANT)
    Q_PROPERTY(QString diagnosticsText READ diagnosticsText NOTIFY diagnosticsChanged)

public:
    struct Options
    {
        QString runtimeRoot;
        bool demo = true;
    };

    explicit AppController(const Options &options, QObject *parent = nullptr);

    QAbstractItemModel *conversations();
    QAbstractItemModel *messages();
    QObject *bridgeObject();

    QString currentId() const { return m_currentId; }
    QString currentTitle() const { return m_currentTitle; }
    QString currentSubtitle() const { return m_currentSubtitle; }
    QString currentAvatarText() const { return m_currentAvatarText; }
    QString currentAvatarColor() const { return m_currentAvatarColor; }
    bool currentOnline() const { return m_currentOnline; }

    QString runtimeStatus() const { return m_runtimeStatus; }
    bool runtimeDetected() const { return m_runtimeDetected; }
    QString runtimeManifestJson() const;
    bool demoMode() const { return m_options.demo; }
    bool rendererHostEnabled() const;
    QString rendererUrl() const;
    QString diagnosticsText() const { return m_log.join(QLatin1Char('\n')); }

    Q_INVOKABLE void openConversation(const QString &id);
    Q_INVOKABLE bool sendMessage(const QString &text);
    Q_INVOKABLE void setSearchText(const QString &text);
    Q_INVOKABLE void reloadDemo();
    Q_INVOKABLE void clearDiagnostics();
    Q_INVOKABLE void logRenderer(const QString &message);
    Q_INVOKABLE void onRendererLoaded();
    Q_INVOKABLE void onRendererFailed(const QString &error);

    int conversationCount() const;
    int messageCount() const;

signals:
    void currentChanged();
    void countsChanged();
    void runtimeStatusChanged();
    void diagnosticsChanged();

private:
    void detectRuntime();
    void applyState(const QJsonObject &state);
    void appendLog(const QString &line);

    Options m_options;
    BridgeRouter *m_router = nullptr;
    BridgeObject *m_bridgeObject = nullptr;
    ConversationModel *m_conversations = nullptr;
    ConversationFilterModel *m_filter = nullptr;
    MessageModel *m_messages = nullptr;

    QHash<QString, QJsonArray> m_messagesById;
    QString m_currentId;
    QString m_currentTitle;
    QString m_currentSubtitle;
    QString m_currentAvatarText;
    QString m_currentAvatarColor;
    bool m_currentOnline = false;

    bool m_runtimeDetected = false;
    QString m_runtimeStatus;
    QJsonObject m_runtimeManifest;

    QStringList m_log;
};

} // namespace ZaloNative
