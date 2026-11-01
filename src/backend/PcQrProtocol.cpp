// PC QR request/response contract referenced from ctdozlite (MIT OR Apache-2.0),
// commit 1345f5d0cfd24fdb7d54e5caf3a88e3e3b2acd4a, src/qr.rs and crypto.rs.
// MIT attribution and license: third_party/ctdozlite/LICENSE-MIT. No Rust source,
// real QR tickets, account data, or captured responses are copied here.
#include "PcQrProtocol.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QUrlQuery>

namespace ZaloNative::PcQrProtocol {

namespace {

constexpr qsizetype kMaxResponseBytes = 2 * 1024 * 1024;
constexpr qsizetype kMaxUrlChars = 8192;

bool fieldOk(const QString &value, qsizetype maxLength)
{
    return !value.isEmpty() && value.size() <= maxLength && !value.contains(QChar::Null);
}

bool pollUrlOk(const QUrl &url, const QString &expectedPath)
{
    return url.isValid() && url.toString().size() <= kMaxUrlChars
        && url.scheme() == QLatin1String("https")
        && url.host().toLower() == QLatin1String("qr-id.zaloapp.com")
        && (url.port() == -1 || url.port() == 443) && url.userInfo().isEmpty()
        && !url.hasFragment() && url.path() == expectedPath
        && !QUrlQuery(url).queryItemValue(QStringLiteral("code")).isEmpty();
}

bool payloadOk(const QString &payload)
{
    if (!fieldOk(payload, 4096))
        return false;
    // This URL is only encoded in a QR; never fetch it as an HTTP request.
    const QUrl url(payload, QUrl::StrictMode);
    const bool http = url.scheme() == QLatin1String("http");
    const bool https = url.scheme() == QLatin1String("https");
    return url.isValid() && (http || https)
        && url.host().toLower() == QLatin1String("zaloapp.com")
        && (url.port() == -1 || (http && url.port() == 80) || (https && url.port() == 443))
        && url.userInfo().isEmpty() && !url.hasFragment()
        && url.path() == QLatin1String("/qr/l")
        && !QUrlQuery(url).queryItemValue(QStringLiteral("tk")).isEmpty();
}

bool boundedInteger(const QJsonObject &obj, const QString &key, int defaultValue,
                    int minimum, int maximum, int *result)
{
    const QJsonValue value = obj.value(key);
    if (value.isUndefined()) {
        *result = defaultValue;
        return true;
    }
    if (!value.isDouble())
        return false;
    const qint64 number = value.toInteger(-1);
    if (number < minimum || number > maximum)
        return false;
    *result = int(number);
    return true;
}

} // namespace

std::optional<QUrl> buildRequest(const RequestFields &fields)
{
    if (!fieldOk(fields.deviceId, 256) || !fieldOk(fields.computerName, 256)
        || !fieldOk(fields.language, 16) || !fieldOk(fields.clientType, 16)
        || !fieldOk(fields.clientVersion, 32) || fields.clientTimeSeconds <= 0)
        return std::nullopt;

    // The earlier independent C++ probe used the same sorted-value digest for
    // another protocol operation (src/probe/ProtocolCrypto.cpp). This MD5 is
    // wire compatibility only, not an authentication or security guarantee.
    QMap<QString, QString> params{
        {QStringLiteral("client_time"), QString::number(fields.clientTimeSeconds)},
        {QStringLiteral("client_version"), fields.clientVersion},
        {QStringLiteral("computer_name"), fields.computerName},
        {QStringLiteral("imei"), fields.deviceId},
        {QStringLiteral("language"), fields.language},
        {QStringLiteral("logged_uids"), QStringLiteral("[]")},
        {QStringLiteral("qr_form"), QStringLiteral("0")},
        {QStringLiteral("self_g"), QStringLiteral("1")},
        {QStringLiteral("type"), fields.clientType},
    };
    QByteArray input = QByteArrayLiteral("zsecurereqqr");
    for (auto it = params.cbegin(); it != params.cend(); ++it)
        input += it.value().toUtf8();
    params.insert(QStringLiteral("signkey"), QString::fromLatin1(
        QCryptographicHash::hash(input, QCryptographicHash::Md5).toHex()));

    QByteArray query;
    for (auto it = params.cbegin(); it != params.cend(); ++it) {
        if (!query.isEmpty())
            query += '&';
        query += QUrl::toPercentEncoding(it.key()) + '=' + QUrl::toPercentEncoding(it.value());
    }
    const QUrl url = QUrl::fromEncoded(QByteArrayLiteral("https://wpa.zaloapp.com/api/login/reqqr?") + query,
                                       QUrl::StrictMode);
    if (!url.isValid())
        return std::nullopt;
    return url;
}

std::optional<Ticket> parseTicket(const QByteArray &response)
{
    if (response.size() > kMaxResponseBytes)
        return std::nullopt;
    const QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject())
        return std::nullopt;
    const QJsonObject root = doc.object();
    const QJsonValue error = root.value(QStringLiteral("error_code"));
    if (!error.isDouble() || error.toInteger(-1) != 0
        || !root.value(QStringLiteral("data")).isObject())
        return std::nullopt;
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    Ticket ticket;
    ticket.qrPayload = data.value(QStringLiteral("qr_cnt")).toString();
    ticket.scanUrl = QUrl(data.value(QStringLiteral("chk_wait_scan")).toString(), QUrl::StrictMode);
    ticket.confirmUrl = QUrl(data.value(QStringLiteral("chk_wait_cfirm")).toString(), QUrl::StrictMode);
    if (!payloadOk(ticket.qrPayload)
        || !pollUrlOk(ticket.scanUrl, QStringLiteral("/polling/qr/waiting"))
        || !pollUrlOk(ticket.confirmUrl, QStringLiteral("/polling/qr/pc-status"))
        || !boundedInteger(data, QStringLiteral("qr_timeout"), 180, 1, 300, &ticket.timeoutSeconds)
        || !boundedInteger(data, QStringLiteral("qr_retry"), 3, 0, 10, &ticket.retryCount))
        return std::nullopt;
    return ticket;
}

} // namespace ZaloNative::PcQrProtocol
