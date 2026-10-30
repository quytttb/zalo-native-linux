#pragma once

#include <QByteArray>
#include <QMap>
#include <optional>

namespace ZaloNative::ProtocolCrypto {
// Experimental compatibility primitives for Zalo's web protocol. They are
// not general-purpose cryptography: the wire protocol specifies the zero IV.
QByteArray sign(const QByteArray &operation, const QMap<QByteArray, QByteArray> &values);
std::optional<QByteArray> encrypt(const QByteArray &key, const QByteArray &plain);
std::optional<QByteArray> decrypt(const QByteArray &key, const QByteArray &cipher);
}
