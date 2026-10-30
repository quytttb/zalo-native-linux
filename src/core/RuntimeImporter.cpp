#include "RuntimeImporter.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>

namespace ZaloNative {

namespace {

struct FileRecord
{
    qint64 size = 0;
    QByteArray sha256;
};

bool isInside(const QString &canonicalChild, const QString &canonicalParent)
{
    const QString prefix =
        canonicalParent.endsWith(QLatin1Char('/')) ? canonicalParent : canonicalParent + QLatin1Char('/');
    return canonicalChild == canonicalParent || canonicalChild.startsWith(prefix);
}

QByteArray digestTree(const QMap<QString, FileRecord> &files)
{
    QCryptographicHash tree(QCryptographicHash::Sha256);
    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        tree.addData(it.key().toUtf8());
        tree.addData(QByteArrayLiteral("\0"));
        tree.addData(QByteArray::number(it.value().size));
        tree.addData(QByteArrayLiteral("\0"));
        tree.addData(it.value().sha256);
        tree.addData(QByteArrayLiteral("\n"));
    }
    return tree.result();
}

QString copyOneFile(const QString &sourcePath,
                    const QString &destinationPath,
                    qint64 maxBytes,
                    qint64 *copiedBytes,
                    QByteArray *sha256,
                    QString *error)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("cannot read %1: %2").arg(sourcePath, source.errorString());
        return QString();
    }

    QFile destination(destinationPath);
    if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("cannot write %1: %2")
                         .arg(destinationPath, destination.errorString());
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(1024 * 1024, Qt::Uninitialized);
    qint64 written = 0;
    while (!source.atEnd()) {
        const qint64 read = source.read(buffer.data(), buffer.size());
        if (read < 0) {
            if (error)
                *error = QStringLiteral("cannot read %1: %2").arg(sourcePath, source.errorString());
            return QString();
        }
        if (read == 0)
            break;
        written += read;
        if (written > maxBytes) {
            if (error)
                *error = QStringLiteral("import exceeds the %1 byte limit").arg(maxBytes);
            return QString();
        }
        hash.addData(QByteArrayView(buffer.constData(), read));
        if (destination.write(buffer.constData(), read) != read) {
            if (error)
                *error = QStringLiteral("short write to %1: %2")
                             .arg(destinationPath, destination.errorString());
            return QString();
        }
    }
    if (!destination.flush() || destination.error() != QFileDevice::NoError) {
        if (error)
            *error = QStringLiteral("cannot flush %1: %2")
                         .arg(destinationPath, destination.errorString());
        return QString();
    }

    destination.setPermissions(source.permissions() | QFileDevice::ReadOwner
                               | QFileDevice::WriteOwner | QFileDevice::ReadGroup
                               | QFileDevice::ReadOther);
    *copiedBytes += written;
    *sha256 = hash.result();
    return QStringLiteral("ok");
}

QJsonObject readImportManifest(const QString &runtimeDir, QString *error)
{
    const QString path = QDir(runtimeDir).absoluteFilePath(QStringLiteral("runtime.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
        return {};
    }
    if (file.size() > 4 * 1024 * 1024) {
        if (error)
            *error = QStringLiteral("%1 is larger than 4 MiB").arg(path);
        return {};
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("%1 is not valid JSON: %2").arg(path, parseError.errorString());
        return {};
    }
    return document.object();
}

} // namespace

QJsonObject RuntimeImportResult::toJson() const
{
    return QJsonObject{
        {QStringLiteral("schema"), QStringLiteral("zalonative.runtime/1")},
        {QStringLiteral("destination"), destination},
        {QStringLiteral("source"), sourcePath},
        {QStringLiteral("importedAt"), importedAt},
        {QStringLiteral("treeSha256"), treeSha256},
        {QStringLiteral("manifestPath"), manifestPath},
        {QStringLiteral("fileCount"), fileCount},
        {QStringLiteral("byteCount"), byteCount},
        {QStringLiteral("manifest"), manifest.toJson()},
        {QStringLiteral("verification"),
         QJsonObject{
             {QStringLiteral("status"), QStringLiteral("unverified")},
             {QStringLiteral("sourceVerified"), false},
             {QStringLiteral("note"),
              QStringLiteral("Imported from a user-supplied directory. The local tree hash detects "
                             "later modification but is not a vendor signature; pass "
                             "--expected-tree-sha256 to enforce a hash from a trusted source.")},
         }},
    };
}

std::optional<RuntimeImportResult> RuntimeImporter::importAppDirectory(
    const QString &source,
    const QString &destination,
    const RuntimeImportOptions &options,
    QString *error)
{
    auto fail = [error](const QString &message) -> std::optional<RuntimeImportResult> {
        if (error)
            *error = message;
        return std::nullopt;
    };

    const QFileInfo sourceInfo(source);
    if (!sourceInfo.isDir())
        return fail(QStringLiteral("source is not a directory: %1").arg(source));

    QString manifestError;
    auto manifest = RuntimeManifest::fromAppDirectory(sourceInfo.absoluteFilePath(), &manifestError);
    if (!manifest)
        return fail(manifestError);
    if (!manifest->isRuntimeCore())
        return fail(QStringLiteral("source has no HTML entry point; refusing to import a "
                                   "launcher-only runtime"));

    if (!options.expectedVersion.isEmpty()
        && manifest->appVersion != options.expectedVersion) {
        return fail(QStringLiteral("runtime version %1 does not match the pinned version %2")
                        .arg(manifest->appVersion, options.expectedVersion));
    }

    const QString sourceCanonical = sourceInfo.canonicalFilePath();
    if (sourceCanonical.isEmpty())
        return fail(QStringLiteral("cannot resolve source directory"));

    const QFileInfo destinationInfo(destination);
    const QString destinationAbsolute = destinationInfo.absoluteFilePath();
    QDir destinationParent(destinationInfo.absolutePath());
    if (!destinationParent.exists() && !QDir().mkpath(destinationParent.absolutePath()))
        return fail(QStringLiteral("cannot create %1").arg(destinationParent.absolutePath()));

    const QString parentCanonical = QFileInfo(destinationParent.absolutePath()).canonicalFilePath();
    if (parentCanonical.isEmpty())
        return fail(QStringLiteral("cannot resolve destination parent"));

    const QString destinationCanonicalCandidate =
        parentCanonical + QLatin1Char('/') + destinationInfo.fileName();
    if (isInside(destinationCanonicalCandidate, sourceCanonical)
        || isInside(sourceCanonical, destinationCanonicalCandidate)) {
        return fail(QStringLiteral("source and destination must not contain each other"));
    }

    if (destinationInfo.exists()) {
        if (!destinationInfo.isDir())
            return fail(QStringLiteral("destination exists and is not a directory"));
        QDir existing(destinationAbsolute);
        const QStringList entries = existing.entryList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        if (!entries.isEmpty()) {
            if (!options.overwrite) {
                return fail(QStringLiteral("destination is not empty: %1 (pass --overwrite to "
                                           "replace an imported runtime)")
                                .arg(destinationAbsolute));
            }
            if (!QFileInfo::exists(existing.absoluteFilePath(QStringLiteral("runtime.json")))) {
                return fail(QStringLiteral("refusing to delete %1: it has no runtime.json import "
                                           "marker")
                                .arg(destinationAbsolute));
            }
            if (!existing.removeRecursively())
                return fail(QStringLiteral("cannot remove the previous runtime at %1")
                                .arg(destinationAbsolute));
        } else if (!existing.removeRecursively()) {
            return fail(QStringLiteral("cannot remove the empty destination %1")
                            .arg(destinationAbsolute));
        }
    }

    QTemporaryDir temporary(destinationAbsolute + QStringLiteral(".import-XXXXXX"));
    if (!temporary.isValid())
        return fail(QStringLiteral("cannot create a temporary import directory: %1")
                        .arg(temporary.errorString()));

    const QString tempPath = temporary.path();
    QDir tempDir(tempPath);

    QMap<QString, FileRecord> records;
    qint64 totalBytes = 0;
    int fileCount = 0;

    QDirIterator iterator(sourceCanonical,
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString sourcePath = iterator.next();
        const QFileInfo info(sourcePath);
        const QString relative = QDir(sourceCanonical).relativeFilePath(sourcePath);
        if (relative.isEmpty() || relative.startsWith(QLatin1String("../")))
            return fail(QStringLiteral("unsafe source path: %1").arg(sourcePath));
        if (relative == QLatin1String("runtime.json"))
            continue;

        const QString targetPath = tempDir.absoluteFilePath(relative);
        if (!isInside(QFileInfo(targetPath).absolutePath(), tempPath))
            return fail(QStringLiteral("import path escapes the destination: %1").arg(relative));

        QString copySource = sourcePath;
        QFileInfo copyInfo = info;
        if (info.isSymLink()) {
            const QString canonicalTarget = info.canonicalFilePath();
            if (canonicalTarget.isEmpty())
                return fail(QStringLiteral("broken symlink in runtime: %1").arg(relative));
            if (!isInside(canonicalTarget, sourceCanonical))
                return fail(QStringLiteral("symlink escapes the source: %1").arg(relative));
            copyInfo = QFileInfo(canonicalTarget);
            if (copyInfo.isDir()) {
                return fail(QStringLiteral("symlinked directories are not supported: %1")
                                .arg(relative));
            }
            copySource = canonicalTarget;
        }

        if (copyInfo.isDir()) {
            if (!QDir().mkpath(targetPath))
                return fail(QStringLiteral("cannot create %1").arg(targetPath));
            continue;
        }
        if (!copyInfo.isFile())
            continue;

        if (++fileCount > options.maxFiles)
            return fail(QStringLiteral("import exceeds the %1 file limit").arg(options.maxFiles));

        if (!QFileInfo(targetPath).absoluteDir().exists()
            && !QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
            return fail(QStringLiteral("cannot create %1").arg(QFileInfo(targetPath).absolutePath()));
        }

        QByteArray sha256;
        QString copyError;
        if (copyOneFile(copySource, targetPath, options.maxBytes - totalBytes, &totalBytes,
                        &sha256, &copyError)
            != QLatin1String("ok")) {
            return fail(copyError);
        }
        records.insert(relative, FileRecord{copyInfo.size(), sha256});
    }

    if (records.isEmpty())
        return fail(QStringLiteral("source contains no copyable files"));

    const QByteArray treeSha256 = digestTree(records);

    if (!options.expectedTreeSha256.isEmpty()
        && QString::fromLatin1(treeSha256.toHex()) != options.expectedTreeSha256.toLower()) {
        return fail(QStringLiteral("tree hash %1 does not match the expected hash %2")
                        .arg(QString::fromLatin1(treeSha256.toHex()),
                             options.expectedTreeSha256));
    }

    RuntimeImportResult result;
    result.destination = destinationAbsolute;
    result.sourcePath = sourceCanonical;
    result.importedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    result.treeSha256 = QString::fromLatin1(treeSha256.toHex());
    result.manifestPath = tempDir.absoluteFilePath(QStringLiteral("runtime.json"));
    result.manifest = *manifest;
    result.fileCount = fileCount;
    result.byteCount = totalBytes;

    // Include the files a host needs first. The tree hash covers every entry.
    QJsonArray keyFiles;
    QSet<QString> keyNames{
        QStringLiteral("package.json"),
        QStringLiteral("package.json.bak"),
        QStringLiteral("bootstrap.js"),
    };
    if (!manifest->entryHtml.isEmpty())
        keyNames.insert(manifest->entryHtml);
    if (!manifest->mainEntry.isEmpty())
        keyNames.insert(manifest->mainEntry);
    for (const QString &library : manifest->nativeLibraries)
        keyNames.insert(library);
    for (const QString &script : manifest->preloads.scripts) {
        keyNames.insert(QStringLiteral("main-dist/") + script);
    }
    for (auto it = records.constBegin(); it != records.constEnd(); ++it) {
        if (!keyNames.contains(it.key()))
            continue;
        keyFiles.append(QJsonObject{
            {QStringLiteral("path"), it.key()},
            {QStringLiteral("size"), it.value().size},
            {QStringLiteral("sha256"), QString::fromLatin1(it.value().sha256.toHex())},
        });
    }

    QJsonObject document = result.toJson();
    document.insert(QStringLiteral("keyFiles"), keyFiles);

    QSaveFile manifestFile(result.manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly)) {
        return fail(QStringLiteral("cannot write %1: %2")
                        .arg(result.manifestPath, manifestFile.errorString()));
    }
    manifestFile.write(QJsonDocument(document).toJson(QJsonDocument::Indented));
    if (!manifestFile.commit())
        return fail(QStringLiteral("cannot commit %1: %2")
                        .arg(result.manifestPath, manifestFile.errorString()));

    temporary.setAutoRemove(false);
    if (!QDir().rename(tempPath, destinationAbsolute)) {
        QDir(tempPath).removeRecursively();
        return fail(QStringLiteral("cannot move the imported runtime to %1").arg(destinationAbsolute));
    }

    return result;
}

QString RuntimeImporter::treeDigest(const QString &dir,
                                    int *fileCount,
                                    qint64 *byteCount,
                                    QString *error)
{
    const QFileInfo rootInfo(dir);
    if (!rootInfo.isDir()) {
        if (error)
            *error = QStringLiteral("not a directory: %1").arg(dir);
        return QString();
    }
    const QString root = rootInfo.canonicalFilePath();
    if (root.isEmpty()) {
        if (error)
            *error = QStringLiteral("cannot resolve %1").arg(dir);
        return QString();
    }

    QMap<QString, FileRecord> records;
    int count = 0;
    qint64 bytes = 0;
    QDirIterator iterator(root,
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        const QString relative = QDir(root).relativeFilePath(path);
        if (relative == QLatin1String("runtime.json"))
            continue;
        if (info.isSymLink()) {
            const QString target = info.canonicalFilePath();
            if (target.isEmpty() || !isInside(target, root)) {
                if (error)
                    *error = QStringLiteral("symlink escapes the runtime: %1").arg(relative);
                return QString();
            }
            if (QFileInfo(target).isDir())
                continue;
        } else if (info.isDir()) {
            continue;
        }
        if (!info.isFile())
            continue;

        QFile file(info.isSymLink() ? info.canonicalFilePath() : path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = QStringLiteral("cannot read %1: %2").arg(relative, file.errorString());
            return QString();
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file)) {
            if (error)
                *error = QStringLiteral("cannot hash %1").arg(relative);
            return QString();
        }
        ++count;
        bytes += file.size();
        records.insert(relative, FileRecord{file.size(), hash.result()});
    }

    if (fileCount)
        *fileCount = count;
    if (byteCount)
        *byteCount = bytes;
    return QString::fromLatin1(digestTree(records).toHex());
}

std::optional<QJsonObject> RuntimeImporter::verify(const QString &runtimeDir, QString *error)
{
    auto fail = [error](const QString &message) -> std::optional<QJsonObject> {
        if (error)
            *error = message;
        return std::nullopt;
    };

    const QJsonObject import = readImportManifest(runtimeDir, error);
    if (import.isEmpty())
        return std::nullopt;
    if (import.value(QStringLiteral("schema")).toString()
        != QLatin1String("zalonative.runtime/1")) {
        return fail(QStringLiteral("runtime.json has an unsupported schema"));
    }

    const QString expected = import.value(QStringLiteral("treeSha256")).toString();
    if (expected.isEmpty())
        return fail(QStringLiteral("runtime.json has no treeSha256"));

    int fileCount = 0;
    qint64 byteCount = 0;
    QString digestError;
    const QString actual = treeDigest(runtimeDir, &fileCount, &byteCount, &digestError);
    if (actual.isEmpty())
        return fail(digestError);

    QString manifestError;
    auto manifest = RuntimeManifest::fromAppDirectory(runtimeDir, &manifestError);

    QJsonObject result{
        {QStringLiteral("ok"), actual.compare(expected, Qt::CaseInsensitive) == 0},
        {QStringLiteral("runtimeDir"), QFileInfo(runtimeDir).absoluteFilePath()},
        {QStringLiteral("expectedTreeSha256"), expected},
        {QStringLiteral("actualTreeSha256"), actual},
        {QStringLiteral("fileCount"), fileCount},
        {QStringLiteral("byteCount"), byteCount},
        {QStringLiteral("sourceVerified"), false},
        {QStringLiteral("note"),
         QStringLiteral("This verifies local integrity against runtime.json only. It does not "
                        "authenticate the vendor or prove host compatibility.")},
    };
    if (manifest) {
        result.insert(QStringLiteral("appVersion"), manifest->appVersion);
        result.insert(QStringLiteral("entryHtml"), manifest->entryHtml);
        result.insert(QStringLiteral("preloads"), manifest->preloads.toJson());
        result.insert(QStringLiteral("warnings"), QJsonArray::fromStringList(manifest->warnings));
    } else {
        result.insert(QStringLiteral("manifestError"), manifestError);
    }
    return result;
}

} // namespace ZaloNative
