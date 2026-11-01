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
class NativeQrLogin;

class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel *conversations READ conversations CONSTANT)
    Q_PROPERTY(QAbstractItemModel *messages READ messages CONSTANT)
    Q_PROPERTY(QObject *bridgeObject READ bridgeObject CONSTANT)
    Q_PROPERTY(QObject *qrLogin READ qrLogin CONSTANT)

    Q_PROPERTY(QString currentId READ currentId NOTIFY currentChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentChanged)
    Q_PROPERTY(QString currentSubtitle READ currentSubtitle NOTIFY currentChanged)
    Q_PROPERTY(QString currentAvatarText READ currentAvatarText NOTIFY currentChanged)
    Q_PROPERTY(QString currentAvatarColor READ currentAvatarColor NOTIFY currentChanged)
    Q_PROPERTY(bool currentOnline READ currentOnline NOTIFY currentChanged)

    Q_PROPERTY(QString runtimeStatus READ runtimeStatus NOTIFY runtimeStatusChanged)
    Q_PROPERTY(bool runtimeDetected READ runtimeDetected NOTIFY runtimeStatusChanged)
    Q_PROPERTY(bool runtimeHasCore READ runtimeHasCore NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString runtimeManifestJson READ runtimeManifestJson NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString runtimeSourceKind READ runtimeSourceKind NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString runtimeAppVersion READ runtimeAppVersion NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString runtimeAppDir READ runtimeAppDir NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString runtimeEntryHtml READ runtimeEntryHtml NOTIFY runtimeStatusChanged)
    Q_PROPERTY(int runtimeExposedApiCount READ runtimeExposedApiCount NOTIFY runtimeStatusChanged)
    Q_PROPERTY(int runtimeSyncCallSites READ runtimeSyncCallSites NOTIFY runtimeStatusChanged)
    Q_PROPERTY(int runtimeDynamicSyncCalls READ runtimeDynamicSyncCalls NOTIFY runtimeStatusChanged)
    Q_PROPERTY(int runtimeNativeLibraryCount READ runtimeNativeLibraryCount NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QStringList runtimeWarnings READ runtimeWarnings NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QStringList runtimeExposedApis READ runtimeExposedApis NOTIFY runtimeStatusChanged)

    Q_PROPERTY(int conversationCount READ conversationCount NOTIFY countsChanged)
    Q_PROPERTY(int messageCount READ messageCount NOTIFY countsChanged)
    Q_PROPERTY(bool demoMode READ demoMode CONSTANT)
    Q_PROPERTY(bool webMode READ webMode CONSTANT)
    Q_PROPERTY(bool nativeMode READ nativeMode CONSTANT)
    Q_PROPERTY(bool runtimeDiagnosticsMode READ runtimeDiagnosticsMode CONSTANT)
    Q_PROPERTY(QString appMode READ appMode CONSTANT)
    Q_PROPERTY(bool rendererHostEnabled READ rendererHostEnabled CONSTANT)
    Q_PROPERTY(QString rendererUrl READ rendererUrl CONSTANT)
    Q_PROPERTY(QString modeLabel READ modeLabel CONSTANT)
    Q_PROPERTY(QString currentPageUrl READ currentPageUrl NOTIFY rendererPageChanged)
    Q_PROPERTY(QString rendererPageTitle READ rendererPageTitle NOTIFY rendererPageChanged)
    Q_PROPERTY(QString rendererState READ rendererState NOTIFY rendererPageChanged)
    Q_PROPERTY(QString diagnosticsText READ diagnosticsText NOTIFY diagnosticsChanged)

public:
    struct Options
    {
        QString runtimeRoot;
        QString rendererUrl;
        bool demo = false;
        bool runtimeDiagnostics = false;
        // Explicit opt-in for the historical visible Web experiment.
        bool webMode = false;
    };

    explicit AppController(const Options &options, QObject *parent = nullptr);

    QAbstractItemModel *conversations();
    QAbstractItemModel *messages();
    QObject *bridgeObject();
    QObject *qrLogin();
    NativeQrLogin *nativeQrLogin() const { return m_qrLogin; }

    QString currentId() const { return m_currentId; }
    QString currentTitle() const { return m_currentTitle; }
    QString currentSubtitle() const { return m_currentSubtitle; }
    QString currentAvatarText() const { return m_currentAvatarText; }
    QString currentAvatarColor() const { return m_currentAvatarColor; }
    bool currentOnline() const { return m_currentOnline; }

    QString runtimeStatus() const { return m_runtimeStatus; }
    bool runtimeDetected() const { return m_runtimeDetected; }
    bool runtimeHasCore() const { return m_runtimeHasCore; }
    QString runtimeManifestJson() const;
    QString runtimeSourceKind() const { return m_runtimeSourceKind; }
    QString runtimeAppVersion() const { return m_runtimeAppVersion; }
    QString runtimeAppDir() const { return m_runtimeAppDir; }
    QString runtimeEntryHtml() const { return m_runtimeEntryHtml; }
    int runtimeExposedApiCount() const { return m_runtimeExposedApis.size(); }
    int runtimeSyncCallSites() const { return m_runtimeSyncCallSites; }
    int runtimeDynamicSyncCalls() const { return m_runtimeDynamicSyncCalls; }
    int runtimeNativeLibraryCount() const { return m_runtimeNativeLibraryCount; }
    QStringList runtimeWarnings() const { return m_runtimeWarnings; }
    QStringList runtimeExposedApis() const { return m_runtimeExposedApis; }

    bool demoMode() const { return m_options.demo; }
    bool webMode() const { return m_options.webMode; }
    bool nativeMode() const { return !m_options.demo && !m_options.webMode && !m_options.runtimeDiagnostics; }
    bool runtimeDiagnosticsMode() const { return !m_options.demo && !m_options.webMode && m_options.runtimeDiagnostics; }
    QString appMode() const;
    bool rendererHostEnabled() const;
    QString rendererUrl() const;
    QString modeLabel() const;
    QString currentPageUrl() const { return m_currentPageUrl; }
    QString rendererPageTitle() const { return m_rendererPageTitle; }
    QString rendererState() const { return m_rendererState; }
    QString diagnosticsText() const { return m_log.join(QLatin1Char('\n')); }

    Q_INVOKABLE void openConversation(const QString &id);
    Q_INVOKABLE bool sendMessage(const QString &text);
    Q_INVOKABLE void setSearchText(const QString &text);
    Q_INVOKABLE void reloadDemo();
    Q_INVOKABLE void refreshRuntime();
    Q_INVOKABLE void clearDiagnostics();
    Q_INVOKABLE void logRenderer(const QString &message);
    Q_INVOKABLE void onRendererLoaded();
    Q_INVOKABLE void onRendererFailed(const QString &error);
    Q_INVOKABLE void onRendererPageChanged(const QString &url, const QString &title);
    Q_INVOKABLE void onRendererLoadingChanged(bool loading);

    int conversationCount() const;
    int messageCount() const;

signals:
    void currentChanged();
    void countsChanged();
    void runtimeStatusChanged();
    void diagnosticsChanged();
    void rendererPageChanged();

private:
    void detectRuntime();
    void clearRuntime();
    void applyState(const QJsonObject &state);
    void appendLog(const QString &line);
    void updateRendererState();
    QString sanitizedUrl(const QString &url) const;

    Options m_options;
    BridgeRouter *m_router = nullptr;
    BridgeObject *m_bridgeObject = nullptr;
    ConversationModel *m_conversations = nullptr;
    ConversationFilterModel *m_filter = nullptr;
    MessageModel *m_messages = nullptr;
    NativeQrLogin *m_qrLogin = nullptr;

    QHash<QString, QJsonArray> m_messagesById;
    QString m_currentId;
    QString m_currentTitle;
    QString m_currentSubtitle;
    QString m_currentAvatarText;
    QString m_currentAvatarColor;
    bool m_currentOnline = false;

    bool m_runtimeDetected = false;
    bool m_runtimeHasCore = false;
    QString m_runtimeStatus;
    QString m_runtimeSourceKind;
    QString m_runtimeAppVersion;
    QString m_runtimeAppDir;
    QString m_runtimeEntryHtml;
    int m_runtimeSyncCallSites = 0;
    int m_runtimeDynamicSyncCalls = 0;
    int m_runtimeNativeLibraryCount = 0;
    QStringList m_runtimeWarnings;
    QStringList m_runtimeExposedApis;
    QJsonObject m_runtimeManifest;

    QString m_currentPageUrl;
    QString m_rendererPageTitle;
    QString m_rendererState = QStringLiteral("idle");
    bool m_rendererLoading = false;

    QStringList m_log;
};

} // namespace ZaloNative
