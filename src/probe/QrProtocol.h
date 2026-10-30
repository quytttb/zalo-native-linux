#pragma once

#include <QByteArray>
#include <QString>

namespace ZaloNative::QrProtocol {
QString loginVersion(const QByteArray &html);
bool qrResponse(const QByteArray &json, QString *code, QByteArray *png);
}
