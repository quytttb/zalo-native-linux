#pragma once

#include "NativeQrLogin.h"

#include <QPointer>
#include <QQuickImageProvider>

namespace ZaloNative {

// QML's image cache is disabled by NativeShell; this exposes only the image,
// never the QR payload, through an in-memory provider.
class QrImageProvider : public QQuickImageProvider
{
public:
    explicit QrImageProvider(NativeQrLogin *login);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QPointer<NativeQrLogin> m_login;
};

} // namespace ZaloNative
