#include "RuntimeManifest.h"

#include "AsarArchive.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

namespace ZaloNative {

namespace {

const QStringList kEntryCandidates = {
    QStringLiteral("pc-dist/index.html"),
    QStringLiteral("pc-dist/login.html"),
    QStringLiteral("index.html"),
    QStringLiteral("login.html"),
    QStringLiteral("dist/index.html"),
    QStringLiteral("build/index.html"),
    QStringLiteral("app/index.html"),
};

const QStringList kPackageCandidates = {
    QStringLiteral("package.json.bak"),
    QStringLiteral("package.json"),
};

bool readJsonObject(const QString &path, QJsonObject *object, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
        return false;
    }
    if (file.size() > 4 * 1024 * 1024) {
        if (error)
            *error = QStringLiteral("%1 is larger than 4 MiB").arg(path);
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("%1 is not valid JSON: %2").arg(path, parseError.errorString());
        return false;
    }

    *object = document.object();
    return true;
}

QString firstExisting(const QString &root, const QStringList &candidates, QString *relative = nullptr)
{
    const QDir dir(root);
    for (const QString &candidate : candidates) {
        if (dir.exists(candidate)) {
            if (relative)
                *relative = candidate;
            return dir.absoluteFilePath(candidate);
        }
    }
    if (relative)
        relative->clear();
    return QString();
}

QStringList preloadFiles(const QString &appDir)
{
    QDir mainDist(QDir(appDir).absoluteFilePath(QStringLiteral("main-dist")));
    if (!mainDist.exists())
        return {};

    QStringList names = mainDist.entryList(
        {QStringLiteral("preload-*.js"), QStringLiteral("compact-app-preload.js")},
        QDir::Files,
        QDir::Name);
    names.removeDuplicates();

    QStringList files;
    for (const QString &name : names)
        files.append(mainDist.absoluteFilePath(name));
    return files;
}

QStringList nativeModulesFromEntries(const QStringList &entries)
{
    QSet<QString> modules;
    for (const QString &entry : entries) {
        if (!entry.endsWith(QLatin1String(".node")))
            continue;
        const qsizetype marker = entry.indexOf(QLatin1String("node_modules/"));
        if (marker < 0)
            continue;
        const QString rest = entry.mid(marker + int(qstrlen("node_modules/")));
        const QString module = rest.section(QLatin1Char('/'), 0, 0);
        if (!module.isEmpty() && module != QLatin1String("@"))
            modules.insert(module);
        else if (module == QLatin1String("@"))
            modules.insert(rest.section(QLatin1Char('/'), 0, 1));
        if (modules.size() >= 64)
            break;
    }
    QStringList out(modules.begin(), modules.end());
    out.sort();
    return out;
}

QStringList collectNativeLibraries(const QString &appDir)
{
    QStringList libraries;
    QDirIterator iterator(appDir,
                          {QStringLiteral("*.node"), QStringLiteral("*.so"), QStringLiteral("*.so.*")},
                          QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext() && libraries.size() < 200) {
        const QFileInfo info(iterator.next());
        QString relative = QDir(appDir).relativeFilePath(info.absoluteFilePath());
        if (relative.startsWith(QLatin1String("../")))
            continue;
        libraries.append(relative);
    }

    // Helper binaries such as zocr-host have no shared-library suffix. Inspect
    // only the dedicated native/ directory so huge JS trees are not walked.
    QDir nativeDir(QDir(appDir).absoluteFilePath(QStringLiteral("native")));
    if (nativeDir.exists()) {
        QDirIterator executableIterator(nativeDir.absolutePath(),
                                        QDir::Files | QDir::Executable,
                                        QDirIterator::Subdirectories);
        while (executableIterator.hasNext() && libraries.size() < 200) {
            const QFileInfo info(executableIterator.next());
            const QString suffix = info.suffix().toLower();
            if (suffix == QLatin1String("js") || suffix == QLatin1String("json")
                || suffix == QLatin1String("ts") || suffix == QLatin1String("md")
                || suffix == QLatin1String("txt") || suffix == QLatin1String("toml")
                || suffix == QLatin1String("lock")) {
                continue;
            }
            QString relative = QDir(appDir).relativeFilePath(info.absoluteFilePath());
            if (!libraries.contains(relative))
                libraries.append(relative);
        }
    }

    libraries.sort();
    return libraries;
}

void readBuildInfo(RuntimeManifest &manifest, const QString &appDir)
{
    const QString path = QDir(appDir).absoluteFilePath(QStringLiteral("pc-dist/build-info.json"));
    QJsonObject info;
    QString error;
    if (!QFileInfo::exists(path))
        return;
    if (!readJsonObject(path, &info, &error)) {
        manifest.warnings.append(error);
        return;
    }
    manifest.buildVersion = info.value(QStringLiteral("version")).toString();
    manifest.buildCommit = info.value(QStringLiteral("commit")).toString();
    manifest.buildDate = info.value(QStringLiteral("buildDate")).toString();
}

QString relativeLauncherAsar(const QString &appDir)
{
    const QDir dir(appDir);
    const QString parent = QFileInfo(appDir).absolutePath();
    const QStringList candidates{
        QDir(parent).absoluteFilePath(QStringLiteral("resources/app.asar")),
        dir.absoluteFilePath(QStringLiteral("resources/app.asar")),
        QDir(parent).absoluteFilePath(QStringLiteral("app.asar")),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

void scanArchivePreloads(RuntimeManifest &manifest, const AsarArchive &archive)
{
    QStringList entries;
    for (const QString &entry : archive.entries()) {
        const QString name = entry.section(QLatin1Char('/'), -1);
        if (name.startsWith(QLatin1String("preload-")) && name.endsWith(QLatin1String(".js"))) {
            entries.append(entry);
            if (entries.size() >= 8)
                break;
        }
    }
    if (entries.isEmpty())
        return;

    QStringList readable;
    for (const QString &entry : entries) {
        QByteArray data;
        QString error;
        if (archive.readFile(entry, &data, &error, 8 * 1024 * 1024))
            readable.append(QString::fromUtf8(data));
    }

    PreloadInventory inventory;
    inventory.scripts = entries;
    for (const QString &text : readable) {
        const PreloadInventory one = PreloadInventory::scanText(text);
        inventory.exposedApis.append(one.exposedApis);
        inventory.ipcChannels.append(one.ipcChannels);
        inventory.syncChannels.append(one.syncChannels);
        inventory.linuxChannels.append(one.linuxChannels);
        inventory.nativeRequires.append(one.nativeRequires);
        inventory.syncCallSites += one.syncCallSites;
        inventory.warnings.append(one.warnings);
    }
    inventory.exposedApis.removeDuplicates();
    inventory.ipcChannels.removeDuplicates();
    inventory.syncChannels.removeDuplicates();
    inventory.linuxChannels.removeDuplicates();
    inventory.nativeRequires.removeDuplicates();
    inventory.dynamicSyncCalls =
        qMax(0, inventory.syncCallSites - inventory.syncChannels.size());
    inventory.warnings.removeDuplicates();
    manifest.preloads = inventory;
}

} // namespace

bool RuntimeManifest::isAppDirectory(const QString &dir)
{
    const QDir root(dir);
    if (!root.exists())
        return false;
    if (root.exists(QStringLiteral("pc-dist/index.html")))
        return true;
    if (root.exists(QStringLiteral("package.json.bak")))
        return true;
    return root.exists(QStringLiteral("package.json")) && root.exists(QStringLiteral("main-dist"));
}

QJsonObject RuntimeManifest::toJson() const
{
    return QJsonObject{
        {QStringLiteral("asarPath"), asarPath},
        {QStringLiteral("runtimeRoot"), runtimeRoot},
        {QStringLiteral("appDir"), appDir},
        {QStringLiteral("sourceKind"), sourceKind},
        {QStringLiteral("launcherAsar"), launcherAsar},
        {QStringLiteral("name"), name},
        {QStringLiteral("appVersion"), appVersion},
        {QStringLiteral("electronVersion"), electronVersion},
        {QStringLiteral("buildVersion"), buildVersion},
        {QStringLiteral("buildCommit"), buildCommit},
        {QStringLiteral("buildDate"), buildDate},
        {QStringLiteral("mainEntry"), mainEntry},
        {QStringLiteral("entryHtml"), entryHtml},
        {QStringLiteral("nativeModules"), QJsonArray::fromStringList(nativeModules)},
        {QStringLiteral("nativeLibraries"), QJsonArray::fromStringList(nativeLibraries)},
        {QStringLiteral("preloads"), preloads.toJson()},
        {QStringLiteral("warnings"), QJsonArray::fromStringList(warnings)},
        {QStringLiteral("valid"), isValid()},
        {QStringLiteral("runtimeCore"), isRuntimeCore()},
        {QStringLiteral("launcherOnly"), isLauncherOnly()},
        {QStringLiteral("sourceVerified"), false},
    };
}

std::optional<RuntimeManifest> RuntimeManifest::fromAppDirectory(const QString &appDir,
                                                                 QString *error)
{
    const QFileInfo info(appDir);
    if (!info.isDir()) {
        if (error)
            *error = QStringLiteral("not a directory: %1").arg(appDir);
        return std::nullopt;
    }
    if (!isAppDirectory(appDir)) {
        if (error)
            *error = QStringLiteral("no pc-dist/main-dist layout under %1").arg(appDir);
        return std::nullopt;
    }

    RuntimeManifest manifest;
    manifest.runtimeRoot = info.absoluteFilePath();
    manifest.appDir = info.absoluteFilePath();
    manifest.sourceKind = QStringLiteral("app-dir");

    const QString packagePath = firstExisting(appDir, kPackageCandidates);
    if (!packagePath.isEmpty()) {
        QJsonObject package;
        QString packageError;
        if (readJsonObject(packagePath, &package, &packageError)) {
            manifest.name = package.value(QStringLiteral("name")).toString();
            manifest.appVersion = package.value(QStringLiteral("version")).toString();
            manifest.mainEntry = package.value(QStringLiteral("main")).toString();

            const QJsonObject devDependencies =
                package.value(QStringLiteral("devDependencies")).toObject();
            const QJsonObject dependencies =
                package.value(QStringLiteral("dependencies")).toObject();
            manifest.electronVersion =
                devDependencies.value(QStringLiteral("electron")).toString();
            if (manifest.electronVersion.isEmpty())
                manifest.electronVersion = dependencies.value(QStringLiteral("electron")).toString();
            if (manifest.electronVersion.isEmpty())
                manifest.electronVersion = package.value(QStringLiteral("electronVersion")).toString();
        } else {
            manifest.warnings.append(packageError);
        }
        if (QFileInfo(packagePath).fileName() == QLatin1String("package.json.bak")) {
            manifest.warnings.append(
                QStringLiteral("Using package.json.bak; a community port may have rewritten "
                               "the active package.json."));
        }
    } else {
        manifest.warnings.append(QStringLiteral("package.json/package.json.bak is missing."));
    }

    readBuildInfo(manifest, appDir);

    QString entryRelative;
    firstExisting(appDir, kEntryCandidates, &entryRelative);
    manifest.entryHtml = entryRelative;
    if (manifest.entryHtml.isEmpty())
        manifest.warnings.append(QStringLiteral("no known HTML entry point found"));
    if (manifest.appVersion.isEmpty())
        manifest.warnings.append(QStringLiteral("application version is unknown"));
    if (manifest.electronVersion.isEmpty())
        manifest.warnings.append(QStringLiteral("Electron version is unknown"));

    const QStringList preloads = preloadFiles(appDir);
    if (preloads.isEmpty()) {
        manifest.warnings.append(
            QStringLiteral("No preload scripts found under main-dist; the host API surface "
                           "cannot be inventoried."));
    } else {
        manifest.preloads = PreloadInventory::scanFiles(preloads);
        manifest.warnings.append(manifest.preloads.warnings);
    }

    manifest.nativeLibraries = collectNativeLibraries(appDir);
    manifest.launcherAsar = relativeLauncherAsar(appDir);
    if (!manifest.launcherAsar.isEmpty()) {
        manifest.warnings.append(
            QStringLiteral("A launcher ASAR is present at %1. It is not the application core "
                           "and is not used for hosting.").arg(manifest.launcherAsar));
    }

    return manifest;
}

std::optional<RuntimeManifest> RuntimeManifest::fromArchive(const AsarArchive &archive,
                                                            QString *error)
{
    if (!archive.isOpen()) {
        if (error)
            *error = QStringLiteral("archive is not open");
        return std::nullopt;
    }

    RuntimeManifest manifest;
    manifest.asarPath = archive.archivePath();
    manifest.runtimeRoot = QFileInfo(archive.archivePath()).absolutePath();
    manifest.sourceKind = QStringLiteral("asar");

    QByteArray packageJson;
    QString readError;
    if (archive.readFile(QStringLiteral("package.json"), &packageJson, &readError,
                         4 * 1024 * 1024)) {
        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(packageJson, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject package = document.object();
            manifest.name = package.value(QStringLiteral("name")).toString();
            manifest.appVersion = package.value(QStringLiteral("version")).toString();
            manifest.mainEntry = package.value(QStringLiteral("main")).toString();

            const QJsonObject devDependencies =
                package.value(QStringLiteral("devDependencies")).toObject();
            manifest.electronVersion =
                devDependencies.value(QStringLiteral("electron")).toString();
            if (manifest.electronVersion.isEmpty())
                manifest.electronVersion = package.value(QStringLiteral("electronVersion")).toString();
        } else {
            manifest.warnings.append(QStringLiteral("package.json is not valid JSON"));
        }
    } else {
        manifest.warnings.append(QStringLiteral("package.json missing: %1").arg(readError));
    }

    for (const QString &candidate : kEntryCandidates) {
        if (archive.isFile(candidate)) {
            manifest.entryHtml = candidate;
            break;
        }
    }
    if (manifest.entryHtml.isEmpty())
        manifest.warnings.append(QStringLiteral("no known HTML entry point found"));
    if (manifest.appVersion.isEmpty())
        manifest.warnings.append(QStringLiteral("application version is unknown"));
    if (manifest.electronVersion.isEmpty())
        manifest.warnings.append(QStringLiteral("Electron version is unknown"));

    manifest.nativeModules = nativeModulesFromEntries(archive.entries());
    scanArchivePreloads(manifest, archive);

    if (manifest.entryHtml.isEmpty()) {
        manifest.warnings.append(
            QStringLiteral("This looks like a launcher ASAR. The extracted application core "
                           "usually lives next to it (for example AppDir/app/pc-dist)."));
    }

    return manifest;
}

} // namespace ZaloNative
