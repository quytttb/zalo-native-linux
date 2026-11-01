#include "QrImageProvider.h"

namespace ZaloNative {

QrImageProvider::QrImageProvider(NativeQrLogin *login)
    : QQuickImageProvider(QQuickImageProvider::Image), m_login(login)
{
}

QImage QrImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    if (!m_login || id.section(QLatin1Char('?'), 0, 0) != QLatin1String("current"))
        return {};
    QImage image = m_login->qrImage();
    if (size)
        *size = image.size();
    if (!image.isNull() && requestedSize.isValid())
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::FastTransformation);
    return image;
}

} // namespace ZaloNative
