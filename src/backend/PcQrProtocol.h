#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <optional>

namespace ZaloNative::PcQrProtocol {

// Pure serialization/parsing only. These types may contain sensitive values;
// callers must not print or persist them. No requests are sent by this module.
struct RequestFields
{
    QString deviceId;
    QString computerName;
    qint64 clientTimeSeconds = 0;
    QString language = QStringLiteral("vi");
    QString clientType = QStringLiteral("30");
    QString clientVersion = QStringLiteral("694");
};

struct Ticket
{
    QString qrPayload;
    QUrl scanUrl;
    QUrl confirmUrl;
    int timeoutSeconds = 180;
    int retryCount = 3;
};

std::optional<QUrl> buildRequest(const RequestFields &fields);
std::optional<Ticket> parseTicket(const QByteArray &response);

} // namespace ZaloNative::PcQrProtocol
