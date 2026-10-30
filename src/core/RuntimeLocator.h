#pragma once

#include "RuntimeManifest.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace ZaloNative {

class RuntimeLocator
{
public:
    // $ZALO_RUNTIME_DIR, or ~/.local/share/zalo-native/runtime.
    static QString defaultRoot();

    // Extracted application directories below a runtime root, most specific
    // first. This is the preferred source because the launcher ASAR of a
    // packaged desktop build does not contain the renderer.
    static QStringList candidateAppDirs(const QString &root);

    // Existing ASAR candidates below a runtime root (or the root itself when it
    // already points at an .asar file).
    static QStringList candidateAsars(const QString &root);

    // Prefers an extracted application directory, then an ASAR. A launcher-only
    // ASAR is returned with warnings so callers can explain why it cannot host
    // the client; it is never silently upgraded to "runtime core".
    static std::optional<RuntimeManifest> locate(const QString &root, QString *error = nullptr);

    // True when the argument itself is an extracted application directory.
    static bool isAppDirectory(const QString &path);
};

} // namespace ZaloNative
