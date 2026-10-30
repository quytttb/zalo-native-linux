#include "BridgeRouter.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <algorithm>

namespace ZaloNative {

namespace {

QJsonObject makeError(const QString &code, const QString &message, const QString &id = {})
{
    QJsonObject error{
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message},
    };
    QJsonObject response{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), error},
    };
    if (!id.isEmpty())
        response.insert(QStringLiteral("id"), id);
    return response;
}

QJsonObject makeValue(const QJsonValue &value, const QString &id = {})
{
    QJsonObject response{
        {QStringLiteral("ok"), true},
        {QStringLiteral("value"), value},
    };
    if (!id.isEmpty())
        response.insert(QStringLiteral("id"), id);
    return response;
}

const QSet<QString> &allowedMethods()
{
    static const QSet<QString> methods = {
        QStringLiteral("app.ping"),
        QStringLiteral("app.info"),
        QStringLiteral("runtime.info"),
        QStringLiteral("state.publish"),
        QStringLiteral("state.snapshot"),
        QStringLiteral("log.write"),
    };
    return methods;
}

} // namespace

BridgeRouter::BridgeRouter(QObject *parent)
    : QObject(parent)
{
}

void BridgeRouter::setRuntimeInfo(const QJsonObject &info)
{
    m_runtimeInfo = info;
}

QJsonObject BridgeRouter::spec() const
{
    QJsonObject methods;
    methods.insert(QStringLiteral("app.ping"), QStringLiteral("call"));
    methods.insert(QStringLiteral("app.info"), QStringLiteral("call"));
    methods.insert(QStringLiteral("runtime.info"), QStringLiteral("call"));
    methods.insert(QStringLiteral("state.publish"), QStringLiteral("call"));
    methods.insert(QStringLiteral("state.snapshot"), QStringLiteral("call"));
    methods.insert(QStringLiteral("log.write"), QStringLiteral("call"));

    return QJsonObject{
        {QStringLiteral("protocol"), QStringLiteral("zalonative.bridge/1")},
        {QStringLiteral("maxPayloadBytes"), kMaxPayloadBytes},
        {QStringLiteral("methods"), methods},
    };
}

QJsonObject BridgeRouter::dispatchJson(const QString &json)
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const QString message = parseError.error != QJsonParseError::NoError
            ? parseError.errorString()
            : QStringLiteral("request must be a JSON object");
        emit logLine(QStringLiteral("✗ invalid_json: %1").arg(message));
        return makeError(QStringLiteral("invalid_json"), message);
    }
    return dispatch(document.object());
}

QJsonObject BridgeRouter::dispatch(const QJsonObject &request)
{
    const QString id = request.value(QStringLiteral("id")).toString();
    if (request.isEmpty())
        return makeError(QStringLiteral("invalid_request"), QStringLiteral("empty request"), id);

    const QByteArray encoded = QJsonDocument(request).toJson(QJsonDocument::Compact);
    if (encoded.size() > kMaxPayloadBytes) {
        emit logLine(QStringLiteral("✗ payload_too_large (%1 bytes)").arg(encoded.size()));
        return makeError(QStringLiteral("payload_too_large"),
                         QStringLiteral("request exceeds %1 bytes").arg(kMaxPayloadBytes), id);
    }

    const QString kind = request.value(QStringLiteral("kind")).toString(QStringLiteral("call"));
    if (kind != QLatin1String("call") && kind != QLatin1String("invoke")) {
        emit logLine(QStringLiteral("✗ unsupported_kind: %1").arg(kind));
        return makeError(QStringLiteral("unsupported_kind"),
                         QStringLiteral("only synchronous 'call' requests are supported in this milestone"), id);
    }

    const QString path = request.value(QStringLiteral("path")).toString();
    if (path.isEmpty())
        return makeError(QStringLiteral("invalid_request"), QStringLiteral("missing path"), id);
    if (!allowedMethods().contains(path)) {
        emit logLine(QStringLiteral("✗ unknown_method: %1").arg(path));
        return makeError(QStringLiteral("unknown_method"),
                         QStringLiteral("method is not allowlisted: %1").arg(path), id);
    }

    const QJsonValue argsValue = request.value(QStringLiteral("args"));
    if (!argsValue.isUndefined() && !argsValue.isArray())
        return makeError(QStringLiteral("invalid_args"), QStringLiteral("args must be an array"), id);
    const QJsonArray args = argsValue.toArray();

    logRequest(path);

    QString errorCode;
    QString errorMessage;
    const QJsonObject value = handle(path, args, &errorCode, &errorMessage);
    if (!errorCode.isEmpty()) {
        emit logLine(QStringLiteral("✗ %1: %2").arg(errorCode, errorMessage));
        return makeError(errorCode, errorMessage, id);
    }
    return makeValue(value, id);
}

QJsonObject BridgeRouter::handle(const QString &path, const QJsonArray &args, QString *errorCode,
                                 QString *errorMessage)
{
    if (path == QLatin1String("app.ping")) {
        return QJsonObject{
            {QStringLiteral("pong"), true},
            {QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        };
    }

    if (path == QLatin1String("app.info")) {
        return QJsonObject{
            {QStringLiteral("protocol"), QStringLiteral("zalonative.bridge/1")},
            {QStringLiteral("role"), QStringLiteral("native-host")},
        };
    }

    if (path == QLatin1String("runtime.info"))
        return m_runtimeInfo;

    if (path == QLatin1String("state.snapshot"))
        return m_lastState;

    if (path == QLatin1String("state.publish")) {
        if (args.isEmpty() || !args.at(0).isObject()) {
            *errorCode = QStringLiteral("invalid_args");
            *errorMessage = QStringLiteral("state.publish expects one object argument");
            return {};
        }
        const QJsonObject state = args.at(0).toObject();
        const QJsonValue conversations = state.value(QStringLiteral("conversations"));
        if (!conversations.isUndefined() && !conversations.isArray()) {
            *errorCode = QStringLiteral("invalid_args");
            *errorMessage = QStringLiteral("conversations must be an array");
            return {};
        }
        m_lastState = state;
        emit statePublished(state);
        return QJsonObject{
            {QStringLiteral("accepted"), true},
            {QStringLiteral("conversations"), conversations.toArray().size()},
        };
    }

    if (path == QLatin1String("log.write")) {
        QString message = args.isEmpty() ? QString() : args.at(0).toString();
        message = message.left(2000);
        if (message.isEmpty()) {
            *errorCode = QStringLiteral("invalid_args");
            *errorMessage = QStringLiteral("log.write expects a non-empty string");
            return {};
        }
        emit logLine(QStringLiteral("renderer: %1").arg(message));
        return QJsonObject{{QStringLiteral("written"), true}};
    }

    *errorCode = QStringLiteral("unknown_method");
    *errorMessage = QStringLiteral("unhandled method: %1").arg(path);
    return {};
}

void BridgeRouter::logRequest(const QString &path)
{
    emit logLine(QStringLiteral("← %1").arg(path));
}

BridgeObject::BridgeObject(BridgeRouter *router, QObject *parent)
    : QObject(parent)
    , m_router(router)
{
    if (!m_router)
        return;

    connect(m_router, &BridgeRouter::statePublished, this, [this](const QJsonObject &state) {
        const QJsonObject payload{
            {QStringLiteral("kind"), QStringLiteral("event")},
            {QStringLiteral("path"), QStringLiteral("state.changed")},
            {QStringLiteral("value"), state},
        };
        emit event(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    });
}

QString BridgeObject::requestJson(const QString &json)
{
    if (!m_router)
        return QStringLiteral("{\"ok\":false,\"error\":{\"code\":\"no_router\",\"message\":\"bridge is unavailable\"}}");
    const QJsonObject response = m_router->dispatchJson(json);
    return QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact));
}

} // namespace ZaloNative
