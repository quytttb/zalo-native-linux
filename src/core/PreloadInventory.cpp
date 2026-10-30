#include "PreloadInventory.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

namespace ZaloNative {

namespace {

constexpr int kMaxExposedApis = 100;
constexpr int kMaxChannels = 200;
constexpr int kMaxScripts = 64;

void appendUnique(QStringList &target, const QString &value, int limit)
{
    if (value.isEmpty() || target.size() >= limit)
        return;
    if (!target.contains(value))
        target.append(value);
}

QStringList matches(const QRegularExpression &expression, const QString &text, int limit)
{
    QStringList values;
    auto iterator = expression.globalMatch(text);
    while (iterator.hasNext() && values.size() < limit) {
        const QRegularExpressionMatch match = iterator.next();
        appendUnique(values, match.captured(1), limit);
    }
    return values;
}

void appendSummaryWarnings(PreloadInventory &inventory)
{
    if (inventory.syncCallSites > 0) {
        inventory.warnings.append(
            QStringLiteral("%1 synchronous sendSync call site(s); %2 use a literal channel, "
                           "%3 pass a computed channel. QWebChannel alone cannot serve these.")
                .arg(inventory.syncCallSites)
                .arg(inventory.syncChannels.size())
                .arg(inventory.dynamicSyncCalls));
    }
    if (!inventory.nativeRequires.isEmpty()) {
        inventory.warnings.append(
            QStringLiteral("Preload loads native .node modules; they must be rebuilt or replaced "
                           "by host services on Linux."));
    }
    if (!inventory.linuxChannels.isEmpty()) {
        inventory.warnings.append(
            QStringLiteral("%1 Linux-specific IPC channel(s) found; the runtime core already "
                           "expects a Linux host for these members.")
                .arg(inventory.linuxChannels.size()));
    }
}

} // namespace

bool PreloadInventory::isEmpty() const
{
    return scripts.isEmpty();
}

QJsonObject PreloadInventory::toJson() const
{
    return QJsonObject{
        {QStringLiteral("scripts"), QJsonArray::fromStringList(scripts)},
        {QStringLiteral("exposedApis"), QJsonArray::fromStringList(exposedApis)},
        {QStringLiteral("ipcChannels"), QJsonArray::fromStringList(ipcChannels)},
        {QStringLiteral("syncChannels"), QJsonArray::fromStringList(syncChannels)},
        {QStringLiteral("linuxChannels"), QJsonArray::fromStringList(linuxChannels)},
        {QStringLiteral("nativeRequires"), QJsonArray::fromStringList(nativeRequires)},
        {QStringLiteral("syncCallSites"), syncCallSites},
        {QStringLiteral("dynamicSyncCalls"), dynamicSyncCalls},
        {QStringLiteral("scannedBytes"), scannedBytes},
        {QStringLiteral("truncated"), truncated},
        {QStringLiteral("warnings"), QJsonArray::fromStringList(warnings)},
    };
}

PreloadInventory PreloadInventory::scanText(const QString &text, const QString &label)
{
    PreloadInventory inventory;
    if (!label.isEmpty())
        inventory.scripts.append(label);
    inventory.scannedBytes = int(text.size());

    const QRegularExpression expose(
        QStringLiteral(R"(exposeInMainWorld\s*\(\s*['"]([^'"]+)['"])"));
    const QRegularExpression ipcLiteral(
        QStringLiteral(R"(ipcRenderer\s*\.\s*(?:invoke|send|sendSync|on|once|postMessage)\s*\(\s*['"]([^'"]+)['"])"));
    const QRegularExpression syncLiteral(
        QStringLiteral(R"(sendSync\s*\(\s*['"]([^'"]+)['"])"));
    const QRegularExpression linuxLiteral(
        QStringLiteral(R"(['"]((?:zalo-)?(?:linux|wayland)-[A-Za-z0-9:._-]+)['"])"));
    const QRegularExpression nodeRequire(
        QStringLiteral(R"(require\s*\(\s*['"]([^'"]+\.node)['"]\s*\))"));
    const QRegularExpression syncCall(QStringLiteral(R"(sendSync\s*\()"));

    inventory.exposedApis = matches(expose, text, kMaxExposedApis);
    inventory.ipcChannels = matches(ipcLiteral, text, kMaxChannels);
    inventory.syncChannels = matches(syncLiteral, text, kMaxChannels);
    inventory.linuxChannels = matches(linuxLiteral, text, kMaxChannels);
    inventory.nativeRequires = matches(nodeRequire, text, kMaxChannels);

    auto syncIterator = syncCall.globalMatch(text);
    while (syncIterator.hasNext()) {
        syncIterator.next();
        ++inventory.syncCallSites;
    }
    inventory.dynamicSyncCalls = qMax(0, inventory.syncCallSites - inventory.syncChannels.size());

    appendSummaryWarnings(inventory);
    return inventory;
}

PreloadInventory PreloadInventory::scanFiles(const QStringList &paths, qint64 maxBytesPerFile)
{
    PreloadInventory inventory;
    QStringList sorted = paths;
    sorted.sort();

    for (const QString &path : sorted) {
        if (inventory.scripts.size() >= kMaxScripts) {
            inventory.warnings.append(
                QStringLiteral("Stopped after %1 preload scripts.").arg(kMaxScripts));
            break;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            inventory.warnings.append(
                QStringLiteral("Cannot read preload %1: %2").arg(path, file.errorString()));
            continue;
        }

        const qint64 size = file.size();
        const QByteArray data = file.read(maxBytesPerFile);
        if (size > maxBytesPerFile) {
            inventory.truncated = true;
            inventory.warnings.append(
                QStringLiteral("Preload %1 is %2 bytes; scanned only the first %3 bytes.")
                    .arg(QFileInfo(path).fileName())
                    .arg(size)
                    .arg(maxBytesPerFile));
        }
        if (data.isEmpty() && size > 0) {
            inventory.warnings.append(
                QStringLiteral("Preload %1 could not be read.").arg(QFileInfo(path).fileName()));
            continue;
        }

        const PreloadInventory one = scanText(QString::fromUtf8(data), QFileInfo(path).fileName());
        inventory.scripts.append(one.scripts);
        inventory.exposedApis.append(one.exposedApis);
        inventory.ipcChannels.append(one.ipcChannels);
        inventory.syncChannels.append(one.syncChannels);
        inventory.linuxChannels.append(one.linuxChannels);
        inventory.nativeRequires.append(one.nativeRequires);
        inventory.syncCallSites += one.syncCallSites;
        inventory.scannedBytes += one.scannedBytes;
    }

    inventory.exposedApis.removeDuplicates();
    inventory.ipcChannels.removeDuplicates();
    inventory.syncChannels.removeDuplicates();
    inventory.linuxChannels.removeDuplicates();
    inventory.nativeRequires.removeDuplicates();
    inventory.scripts.removeDuplicates();
    inventory.warnings.removeDuplicates();

    inventory.dynamicSyncCalls = qMax(0, inventory.syncCallSites - inventory.syncChannels.size());
    appendSummaryWarnings(inventory);

    if (inventory.isEmpty()) {
        inventory.warnings.append(QStringLiteral("No preload scripts were found."));
    } else if (inventory.exposedApis.isEmpty()) {
        inventory.warnings.append(
            QStringLiteral("No contextBridge.exposeInMainWorld calls were found; the runtime may "
                           "use another preload mechanism or the scan may have missed it."));
    }
    inventory.warnings.append(
        QStringLiteral("Static scan only: it does not execute the runtime and does not prove that "
                       "the listed APIs can be hosted safely."));
    return inventory;
}

} // namespace ZaloNative
