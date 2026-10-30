#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace ZaloNative {

// Static, non-executing inventory of a runtime's Electron preload scripts.
//
// This is deliberately conservative: it only reports literal API/channel names
// that can be recovered from the text. Minified code can hide channel names in
// variables, so a missing entry is not proof that the API is unused, and an
// entry here is not proof that a host can implement it.
struct PreloadInventory
{
    QStringList scripts;
    QStringList exposedApis;
    QStringList ipcChannels;
    QStringList syncChannels;
    QStringList linuxChannels;
    QStringList nativeRequires;
    int syncCallSites = 0;
    int dynamicSyncCalls = 0;
    int scannedBytes = 0;
    bool truncated = false;
    QStringList warnings;

    bool isEmpty() const;
    QJsonObject toJson() const;

    // Scans the given files. Files larger than maxBytesPerFile are read only
    // up to that limit and flagged as truncated.
    static PreloadInventory scanFiles(const QStringList &paths,
                                      qint64 maxBytesPerFile = 8 * 1024 * 1024);
    static PreloadInventory scanText(const QString &text, const QString &label = QString());
};

} // namespace ZaloNative
