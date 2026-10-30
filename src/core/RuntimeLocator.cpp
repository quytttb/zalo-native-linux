#include "RuntimeLocator.h"

#include "AsarArchive.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace ZaloNative {

QString RuntimeLocator::defaultRoot()
{
    const QString override = QProcessEnvironment::systemEnvironment()
                                 .value(QStringLiteral("ZALO_RUNTIME_DIR"));
    if (!override.isEmpty())
        return override;
    return QDir::homePath() + QStringLiteral("/.local/share/zalo-native/runtime");
}

bool RuntimeLocator::isAppDirectory(const QString &path)
{
    return RuntimeManifest::isAppDirectory(path);
}

QStringList RuntimeLocator::candidateAppDirs(const QString &root)
{
    const QFileInfo info(root);
    if (!info.isDir())
        return {};

    const QString absolute = info.absoluteFilePath();
    QStringList candidates;
    if (isAppDirectory(absolute))
        candidates.append(absolute);
    candidates << absolute + QStringLiteral("/app")
               << absolute + QStringLiteral("/AppDir/app")
               << absolute + QStringLiteral("/runtime/app")
               << absolute + QStringLiteral("/usr/share/zalo/app");

    QStringList existing;
    for (const QString &candidate : candidates) {
        if (isAppDirectory(candidate))
            existing.append(candidate);
    }
    existing.removeDuplicates();
    return existing;
}

QStringList RuntimeLocator::candidateAsars(const QString &root)
{
    const QFileInfo info(root);
    QStringList candidates;

    if (info.isFile() && info.suffix().compare(QLatin1String("asar"), Qt::CaseInsensitive) == 0)
        candidates.append(info.absoluteFilePath());
    else if (info.isDir()) {
        candidates << info.absoluteFilePath() + QStringLiteral("/resources/app.asar")
                   << info.absoluteFilePath() + QStringLiteral("/AppDir/resources/app.asar")
                   << info.absoluteFilePath() + QStringLiteral("/app.asar")
                   << info.absoluteFilePath() + QStringLiteral("/runtime/resources/app.asar")
                   << info.absoluteFilePath() + QStringLiteral("/usr/share/zalo/resources/app.asar");
    }

    QStringList existing;
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            existing.append(candidate);
    }
    existing.removeDuplicates();
    return existing;
}

std::optional<RuntimeManifest> RuntimeLocator::locate(const QString &root, QString *error)
{
    const QStringList appDirs = candidateAppDirs(root);
    QStringList errors;

    for (const QString &appDir : appDirs) {
        QString manifestError;
        auto manifest = RuntimeManifest::fromAppDirectory(appDir, &manifestError);
        if (manifest)
            return manifest;
        errors.append(manifestError);
    }

    const QStringList candidates = candidateAsars(root);
    for (const QString &candidate : candidates) {
        QString archiveError;
        auto archive = AsarArchive::open(candidate, &archiveError);
        if (!archive) {
            errors.append(archiveError);
            continue;
        }

        QString manifestError;
        auto manifest = RuntimeManifest::fromArchive(*archive, &manifestError);
        if (!manifest) {
            errors.append(manifestError);
            continue;
        }

        const QFileInfo info(root);
        if (info.isDir())
            manifest->runtimeRoot = info.absoluteFilePath();
        else
            manifest->runtimeRoot = QFileInfo(candidate).absolutePath();
        return manifest;
    }

    if (error) {
        if (errors.isEmpty()) {
            *error = QStringLiteral("no runtime app directory or app.asar found under %1").arg(root);
        } else {
            *error = errors.join(QStringLiteral("; "));
        }
    }
    return std::nullopt;
}

} // namespace ZaloNative
