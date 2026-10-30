#include "LocalScheme.h"

#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStringList>

namespace ZaloNative::LocalScheme {

namespace {

std::optional<ResolvedFile> reject(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return std::nullopt;
}

} // namespace

QString mimeTypeForPath(const QString &path)
{
    const QMimeType type = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension);
    return type.isValid() ? type.name() : QStringLiteral("application/octet-stream");
}

std::optional<ResolvedFile> resolve(const QString &rootDir, const QUrl &url, QString *error)
{
    const QFileInfo rootInfo(rootDir);
    if (!rootInfo.isDir())
        return reject(error, QStringLiteral("runtime root is not a directory: %1").arg(rootDir));

    QString path = url.path(QUrl::FullyDecoded);
    if (path.isEmpty() || path == QLatin1String("/"))
        path = QStringLiteral("index.html");

    if (path.contains(QChar::Null))
        return reject(error, QStringLiteral("NUL byte in request path"));
    if (path.contains(QLatin1Char('\\')))
        return reject(error, QStringLiteral("backslash in request path"));
    if (path.startsWith(QLatin1Char('/')))
        path.remove(0, 1);

    QStringList parts;
    for (const QString &part : path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (part == QLatin1String("."))
            continue;
        if (part == QLatin1String(".."))
            return reject(error, QStringLiteral("path traversal is not allowed"));
        parts.append(part);
    }
    if (parts.isEmpty())
        return reject(error, QStringLiteral("empty request path"));

    const QString relative = parts.join(QLatin1Char('/'));
    const QString candidate = rootInfo.absoluteFilePath() + QLatin1Char('/') + relative;

    const QString rootCanonical = rootInfo.canonicalFilePath();
    if (rootCanonical.isEmpty())
        return reject(error, QStringLiteral("cannot resolve runtime root"));

    const QFileInfo candidateInfo(candidate);
    if (!candidateInfo.exists())
        return reject(error, QStringLiteral("not found: %1").arg(relative));
    if (!candidateInfo.isFile())
        return reject(error, QStringLiteral("not a regular file: %1").arg(relative));

    const QString canonical = candidateInfo.canonicalFilePath();
    const QString prefix = rootCanonical.endsWith(QLatin1Char('/')) ? rootCanonical : rootCanonical + QLatin1Char('/');
    if (!canonical.startsWith(prefix))
        return reject(error, QStringLiteral("path escapes the runtime root: %1").arg(relative));

    return ResolvedFile{canonical, mimeTypeForPath(canonical)};
}

} // namespace ZaloNative::LocalScheme
