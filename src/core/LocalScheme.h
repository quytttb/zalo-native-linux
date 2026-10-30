#pragma once

#include <QString>
#include <QUrl>

#include <optional>

namespace ZaloNative::LocalScheme {

struct ResolvedFile
{
    QString absolutePath;
    QString mimeType;
};

// Maps a request path from the private "zalo-app://" scheme onto a file below
// rootDir. Rejects absolute paths, "..", NUL bytes, backslashes and symlinks
// that resolve outside the root. An empty path maps to index.html.
std::optional<ResolvedFile> resolve(const QString &rootDir, const QUrl &url, QString *error = nullptr);

QString mimeTypeForPath(const QString &path);

} // namespace ZaloNative::LocalScheme
