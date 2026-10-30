#pragma once

#include "PreloadInventory.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace ZaloNative {

class AsarArchive;

// Describes a Zalo desktop runtime in either supported shape:
//
//  * an extracted application directory (pc-dist/main-dist/native), for example
//    `AppDir/app` inside a community AppImage; or
//  * an Electron ASAR, for example an official `resources/app.asar`.
//
// A launcher ASAR can be inspected, but it is not the application core. The
// manifest records that distinction instead of calling any ASAR "valid runtime".
struct RuntimeManifest
{
    QString asarPath;
    QString runtimeRoot;
    QString appDir;
    QString sourceKind;
    QString launcherAsar;

    QString name;
    QString appVersion;
    QString electronVersion;
    QString buildVersion;
    QString buildCommit;
    QString buildDate;

    QString mainEntry;
    QString entryHtml;

    QStringList nativeModules;
    QStringList nativeLibraries;
    PreloadInventory preloads;

    QStringList warnings;

    bool isValid() const { return !asarPath.isEmpty() || !appDir.isEmpty(); }
    // True only when an entry document was found, not merely a parseable ASAR.
    bool isRuntimeCore() const { return isValid() && !entryHtml.isEmpty(); }
    bool isLauncherOnly() const { return isValid() && entryHtml.isEmpty(); }

    QJsonObject toJson() const;

    static std::optional<RuntimeManifest> fromArchive(const AsarArchive &archive,
                                                      QString *error = nullptr);
    static std::optional<RuntimeManifest> fromAppDirectory(const QString &appDir,
                                                           QString *error = nullptr);

    // True when `dir` looks like an extracted application directory. This does
    // not execute anything in it.
    static bool isAppDirectory(const QString &dir);
};

} // namespace ZaloNative
