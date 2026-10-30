#pragma once

#include "RuntimeManifest.h"

#include <QJsonObject>
#include <QString>

#include <optional>

namespace ZaloNative {

struct RuntimeImportOptions
{
    QString expectedVersion;
    QString expectedTreeSha256;
    qint64 maxBytes = 4LL * 1024 * 1024 * 1024;
    int maxFiles = 250000;
    bool overwrite = false;
};

struct RuntimeImportResult
{
    QString destination;
    QString sourcePath;
    QString importedAt;
    QString treeSha256;
    QString manifestPath;
    RuntimeManifest manifest;
    int fileCount = 0;
    qint64 byteCount = 0;

    QJsonObject toJson() const;
};

// Copies an extracted application directory into an isolated runtime folder,
// preserving relative paths, refusing symlinks that escape the source and
// writing a `runtime.json` import manifest with a deterministic tree hash.
//
// A local hash proves integrity of the copy; it does not authenticate the
// vendor. `sourceVerified` stays false unless a trusted hash/signature is
// supplied externally.
class RuntimeImporter
{
public:
    static std::optional<RuntimeImportResult> importAppDirectory(
        const QString &source,
        const QString &destination,
        const RuntimeImportOptions &options,
        QString *error = nullptr);

    // Recomputes the tree hash and compares it with runtime.json. This detects
    // local modification/corruption, not vendor authenticity.
    static std::optional<QJsonObject> verify(const QString &runtimeDir, QString *error = nullptr);

    static QString treeDigest(const QString &dir,
                              int *fileCount = nullptr,
                              qint64 *byteCount = nullptr,
                              QString *error = nullptr);
};

} // namespace ZaloNative
