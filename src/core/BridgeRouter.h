#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace ZaloNative {

// Transport-independent implementation of the renderer <-> native bridge.
//
// The same router is used by the Qt WebEngine host, by the demo producer and by
// unit tests. Messages are JSON objects:
//
//   request : {"id": "...", "kind": "call", "path": "state.publish", "args": [...]}
//   success : {"id": "...", "ok": true, "value": ...}
//   failure : {"id": "...", "ok": false, "error": {"code": "...", "message": "..."}}
//
// Only methods in the allowlist can be called. Payloads are capped at 1 MiB and
// requests never carry file paths: adapters publish structured state, native
// code decides what to do with it.
class BridgeRouter : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxPayloadBytes = 1024 * 1024;

    explicit BridgeRouter(QObject *parent = nullptr);

    QJsonObject dispatch(const QJsonObject &request);
    QJsonObject dispatchJson(const QString &json);

    QJsonObject spec() const;
    QJsonObject runtimeInfo() const { return m_runtimeInfo; }
    void setRuntimeInfo(const QJsonObject &info);
    QJsonObject lastState() const { return m_lastState; }

signals:
    void statePublished(const QJsonObject &state);
    void logLine(const QString &line);

private:
    QJsonObject handle(const QString &path, const QJsonArray &args, QString *errorCode, QString *errorMessage);
    void logRequest(const QString &path);

    QJsonObject m_runtimeInfo;
    QJsonObject m_lastState;
};

// Adapter exposed to JavaScript through QWebChannel as the "bridge" object.
class BridgeObject : public QObject
{
    Q_OBJECT

public:
    explicit BridgeObject(BridgeRouter *router, QObject *parent = nullptr);

    Q_INVOKABLE QString requestJson(const QString &json);

signals:
    void event(const QString &json);

private:
    BridgeRouter *m_router = nullptr;
};

} // namespace ZaloNative
