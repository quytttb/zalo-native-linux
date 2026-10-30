#include "QrProtocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace ZaloNative::QrProtocol {

QString loginVersion(const QByteArray &html)
{
    if (html.size() > 2 * 1024 * 1024)
        return {};
    static const QRegularExpression pattern(
        QStringLiteral(R"(https://stc-zlogin\.zdn\.vn/main-([0-9.]+)\.js)"));
    return pattern.match(QString::fromUtf8(html)).captured(1);
}

bool qrResponse(const QByteArray &json, QString *code, QByteArray *png)
{
    if (!code || !png || json.size() > 2 * 1024 * 1024)
        return false;
    const auto root = QJsonDocument::fromJson(json).object();
    if (!root.value(QStringLiteral("error_code")).isDouble()
        || root.value(QStringLiteral("error_code")).toInt(-1) != 0)
        return false;
    const auto data = root.value(QStringLiteral("data")).toObject();
    const QString qrCode = data.value(QStringLiteral("code")).toString();
    QString image = data.value(QStringLiteral("image")).toString();
    const QString prefix = QStringLiteral("data:image/png;base64,");
    if (image.startsWith(prefix))
        image.remove(0, prefix.size());
    const auto decoded = QByteArray::fromBase64Encoding(image.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (qrCode.isEmpty() || qrCode.size() > 4096 || !decoded
        || !decoded.decoded.startsWith(QByteArray::fromHex("89504e470d0a1a0a")))
        return false;
    *code = qrCode;
    *png = decoded.decoded;
    return true;
}
}
