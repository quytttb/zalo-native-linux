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

    // Existing ASAR candidates below a runtime root (or the root itself when it
    // already points at an .asar file).
    static QStringList candidateAsars(const QString &root);

    static std::optional<RuntimeManifest> locate(const QString &root, QString *error = nullptr);
};

} // namespace ZaloNative
