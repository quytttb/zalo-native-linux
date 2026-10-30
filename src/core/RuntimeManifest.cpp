#include "RuntimeManifest.h"

#include "AsarArchive.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

namespace ZaloNative {

namespace {

const QStringList kEntryCandidates = {
    QStringLiteral("index.html"),
    QStringLiteral("login.html"),
    QStringLiteral("dist/index.html"),
    QStringLiteral("build/index.html"),
    QStringLiteral("app/index.html"),
};

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

} // namespace

QJsonObject RuntimeManifest::toJson() const
{
    QJsonObject object{
        {QStringLiteral("asarPath"), asarPath},
        {QStringLiteral("runtimeRoot"), runtimeRoot},
        {QStringLiteral("name"), name},
        {QStringLiteral("appVersion"), appVersion},
        {QStringLiteral("electronVersion"), electronVersion},
        {QStringLiteral("mainEntry"), mainEntry},
        {QStringLiteral("entryHtml"), entryHtml},
        {QStringLiteral("nativeModules"), QJsonArray::fromStringList(nativeModules)},
        {QStringLiteral("warnings"), QJsonArray::fromStringList(warnings)},
        {QStringLiteral("valid"), isValid()},
    };
    return object;
}

std::optional<RuntimeManifest> RuntimeManifest::fromArchive(const AsarArchive &archive, QString *error)
{
    if (!archive.isOpen()) {
        if (error)
            *error = QStringLiteral("archive is not open");
        return std::nullopt;
    }

    RuntimeManifest manifest;
    manifest.asarPath = archive.archivePath();
    manifest.runtimeRoot = QFileInfo(archive.archivePath()).absolutePath();

    QByteArray packageJson;
    QString readError;
    if (archive.readFile(QStringLiteral("package.json"), &packageJson, &readError, 4 * 1024 * 1024)) {
        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(packageJson, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject package = document.object();
            manifest.name = package.value(QStringLiteral("name")).toString();
            manifest.appVersion = package.value(QStringLiteral("version")).toString();
            manifest.mainEntry = package.value(QStringLiteral("main")).toString();

            const QJsonObject devDependencies = package.value(QStringLiteral("devDependencies")).toObject();
            manifest.electronVersion = devDependencies.value(QStringLiteral("electron")).toString();
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
    return manifest;
}

} // namespace ZaloNative
