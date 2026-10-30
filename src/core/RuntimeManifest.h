#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace ZaloNative {

class AsarArchive;

// Describes an extracted Zalo desktop runtime (Electron app.asar).
struct RuntimeManifest
{
    QString asarPath;
    QString runtimeRoot;
    QString name;
    QString appVersion;
    QString electronVersion;
    QString mainEntry;
    QString entryHtml;
    QStringList nativeModules;
    QStringList warnings;

    bool isValid() const { return !asarPath.isEmpty(); }
    QJsonObject toJson() const;

    static std::optional<RuntimeManifest> fromArchive(const AsarArchive &archive, QString *error = nullptr);
};

} // namespace ZaloNative
