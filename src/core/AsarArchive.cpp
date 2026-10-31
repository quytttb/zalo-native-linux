#include "AsarArchive.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtEndian>

#include <algorithm>
#include <limits>

namespace ZaloNative {

namespace {

constexpr quint32 kMaxHeaderBytes = 256u * 1024u * 1024u;
constexpr int kMaxTreeDepth = 64;
constexpr int kMaxNodes = 500000;
constexpr int kMaxLinkDepth = 8;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString parentKey(const QString &key)
{
    const qsizetype slash = key.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : key.left(slash);
}

QString safeUnpackedFile(const QString &root, const QString &key)
{
    const QFileInfo rootInfo(root);
    if (rootInfo.isSymLink() || !rootInfo.isDir())
        return {};
    const QString canonicalRoot = rootInfo.canonicalFilePath();
    const QFileInfo fileInfo(root + QLatin1Char('/') + key);
    const QString canonicalFile = fileInfo.canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalFile.isEmpty() || !fileInfo.isFile()
        || !canonicalFile.startsWith(canonicalRoot + QLatin1Char('/')))
        return {};
    return canonicalFile;
}

} // namespace

QString AsarArchive::normalizeKey(const QString &raw)
{
    QString path = raw;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (path.startsWith(QLatin1Char('/')))
        path.remove(0, 1);

    QStringList parts;
    for (const QString &part : path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (part == QLatin1String("."))
            continue;
        if (part == QLatin1String(".."))
            return {};
        parts.append(part);
    }
    return parts.join(QLatin1Char('/'));
}

QString AsarArchive::unpackedRoot() const
{
    return m_path + QStringLiteral(".unpacked");
}

std::optional<AsarArchive> AsarArchive::open(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, QStringLiteral("cannot open %1: %2").arg(path, file.errorString()));
        return std::nullopt;
    }

    const qint64 fileSize = file.size();
    if (fileSize < 16) {
        fail(error, QStringLiteral("%1 is too small to be an ASAR archive").arg(path));
        return std::nullopt;
    }

    const QByteArray prefix = file.read(16);
    if (prefix.size() != 16) {
        fail(error, QStringLiteral("cannot read the ASAR header prefix"));
        return std::nullopt;
    }

    const auto u32 = [&prefix](int offset) {
        return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(prefix.constData() + offset));
    };

    if (u32(0) != 4) {
        fail(error, QStringLiteral("unsupported ASAR header (first field is not 4)"));
        return std::nullopt;
    }

    const quint32 headerLength = u32(4);
    const quint32 payloadLength = u32(8);
    const quint32 jsonLength = u32(12);

    if (headerLength < 8 || headerLength > kMaxHeaderBytes) {
        fail(error, QStringLiteral("implausible ASAR header length %1").arg(headerLength));
        return std::nullopt;
    }
    if (jsonLength > payloadLength || payloadLength > headerLength - 4) {
        fail(error, QStringLiteral("inconsistent ASAR header lengths"));
        return std::nullopt;
    }
    if (static_cast<qint64>(headerLength) + 8 > fileSize) {
        fail(error, QStringLiteral("ASAR header extends beyond the end of the file"));
        return std::nullopt;
    }

    if (!file.seek(16)) {
        fail(error, QStringLiteral("cannot seek to the ASAR JSON header"));
        return std::nullopt;
    }
    const QByteArray json = file.read(jsonLength);
    if (json.size() != static_cast<qint64>(jsonLength)) {
        fail(error, QStringLiteral("truncated ASAR JSON header"));
        return std::nullopt;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(error, QStringLiteral("invalid ASAR JSON header: %1").arg(parseError.errorString()));
        return std::nullopt;
    }

    AsarArchive archive;
    archive.m_open = true;
    archive.m_path = QFileInfo(path).absoluteFilePath();
    archive.m_fileSize = fileSize;
    archive.m_dataOffset = static_cast<qint64>(headerLength) + 8;
    archive.m_header = document.object();

    const QJsonObject root = archive.m_header.value(QStringLiteral("files")).toObject();
    if (root.isEmpty() && !archive.m_header.contains(QStringLiteral("files"))) {
        fail(error, QStringLiteral("ASAR header has no 'files' object"));
        return std::nullopt;
    }

    struct Pending
    {
        QJsonObject node;
        QString prefix;
        int depth;
    };
    QList<Pending> stack;
    stack.append({archive.m_header, QString(), 0});

    while (!stack.isEmpty()) {
        const Pending pending = stack.takeLast();
        if (pending.depth > kMaxTreeDepth) {
            fail(error, QStringLiteral("ASAR directory tree is too deep"));
            return std::nullopt;
        }

        const QJsonObject files = pending.node.value(QStringLiteral("files")).toObject();
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            const QString name = it.key();
            if (name.isEmpty() || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))
                || name == QLatin1String(".") || name == QLatin1String("..")) {
                fail(error, QStringLiteral("unsafe entry name in ASAR header: %1").arg(name));
                return std::nullopt;
            }

            const QString key = pending.prefix.isEmpty() ? name : pending.prefix + QLatin1Char('/') + name;
            const QJsonObject value = it.value().toObject();

            Node node;
            if (value.contains(QStringLiteral("files"))) {
                node.directory = true;
                if (archive.m_nodes.size() >= kMaxNodes) {
                    fail(error, QStringLiteral("ASAR has too many entries"));
                    return std::nullopt;
                }
                archive.m_nodes.insert(key, node);
                stack.append({value, key, pending.depth + 1});
                continue;
            }

            if (value.contains(QStringLiteral("link"))) {
                node.link = value.value(QStringLiteral("link")).toString();
                if (node.link.isEmpty()) {
                    fail(error, QStringLiteral("empty link target for %1").arg(key));
                    return std::nullopt;
                }
            } else {
                node.size = value.value(QStringLiteral("size")).toInteger(-1);
                node.offset = value.value(QStringLiteral("offset")).toString().toLongLong();
                node.unpacked = value.value(QStringLiteral("unpacked")).toBool();
                node.executable = value.value(QStringLiteral("executable")).toBool();
                const QJsonObject integrity = value.value(QStringLiteral("integrity")).toObject();
                node.integrityAlgorithm = integrity.value(QStringLiteral("algorithm")).toString();
                node.integrityHash = integrity.value(QStringLiteral("hash")).toString();

                if (node.size < 0 || node.offset < 0) {
                    fail(error, QStringLiteral("invalid size/offset for %1").arg(key));
                    return std::nullopt;
                }
                if (node.unpacked) {
                    if (safeUnpackedFile(archive.unpackedRoot(), key).isEmpty()) {
                        fail(error, QStringLiteral("missing or unsafe unpacked file %1").arg(key));
                        return std::nullopt;
                    }
                } else {
                    const qint64 available = archive.m_fileSize - archive.m_dataOffset;
                    if (node.offset > available || node.size > available - node.offset) {
                        fail(error, QStringLiteral("payload for %1 points outside the archive").arg(key));
                        return std::nullopt;
                    }
                }
            }

            if (archive.m_nodes.size() >= kMaxNodes) {
                fail(error, QStringLiteral("ASAR has too many entries"));
                return std::nullopt;
            }
            archive.m_nodes.insert(key, node);
        }
    }

    return archive;
}

QStringList AsarArchive::entries() const
{
    QStringList keys = m_nodes.keys();
    std::sort(keys.begin(), keys.end());
    return keys;
}

bool AsarArchive::contains(const QString &path) const
{
    return m_nodes.contains(normalizeKey(path));
}

bool AsarArchive::isFile(const QString &path) const
{
    const auto it = m_nodes.constFind(normalizeKey(path));
    return it != m_nodes.constEnd() && !it->directory;
}

bool AsarArchive::isDirectory(const QString &path) const
{
    const auto it = m_nodes.constFind(normalizeKey(path));
    return it != m_nodes.constEnd() && it->directory;
}

bool AsarArchive::readFile(const QString &path, QByteArray *out, QString *error, qint64 maxBytes) const
{
    if (!out)
        return fail(error, QStringLiteral("no output buffer"));
    out->clear();

    const QString requestedKey = normalizeKey(path);
    if (requestedKey.isEmpty())
        return fail(error, QStringLiteral("empty archive path"));

    struct Resolved
    {
        QString key;
        Node node;
    };

    const auto resolve = [this](QString initialKey, int depth, QString *error) -> std::optional<Resolved> {
        QString currentKey = initialKey;
        for (int i = 0; i <= depth; ++i) {
            const auto it = m_nodes.constFind(currentKey);
            if (it == m_nodes.constEnd()) {
                fail(error, QStringLiteral("no such entry: %1").arg(initialKey));
                return std::nullopt;
            }
            if (it->directory || it->link.isEmpty())
                return Resolved{currentKey, *it};
            if (i == depth)
                break;
            const QString target = it->link;
            currentKey = target.startsWith(QLatin1Char('/'))
                ? normalizeKey(target)
                : normalizeKey(parentKey(currentKey) + QLatin1Char('/') + target);
            if (currentKey.isEmpty()) {
                fail(error, QStringLiteral("link target is unsafe"));
                return std::nullopt;
            }
        }
        fail(error, QStringLiteral("too many link levels for %1").arg(initialKey));
        return std::nullopt;
    };

    const auto resolved = resolve(requestedKey, kMaxLinkDepth, error);
    if (!resolved)
        return false;
    const Node &node = resolved->node;
    if (node.directory)
        return fail(error, QStringLiteral("%1 is a directory").arg(path));
    if (node.size > maxBytes)
        return fail(error, QStringLiteral("%1 is larger than the allowed read size").arg(path));

    if (node.unpacked) {
        const QString path = safeUnpackedFile(unpackedRoot(), resolved->key);
        if (path.isEmpty())
            return fail(error, QStringLiteral("missing or unsafe unpacked file"));
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return fail(error, QStringLiteral("cannot open unpacked file: %1").arg(file.errorString()));
        if (file.size() != node.size)
            return fail(error, QStringLiteral("unpacked file size mismatch for %1").arg(resolved->key));
        const qint64 readLimit = node.size == std::numeric_limits<qint64>::max()
            ? node.size : node.size + 1;
        *out = file.read(readLimit);
        return out->size() == node.size
            || fail(error, QStringLiteral("unpacked file size mismatch for %1").arg(path));
    }

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, QStringLiteral("cannot open archive: %1").arg(file.errorString()));
    if (!file.seek(m_dataOffset + node.offset))
        return fail(error, QStringLiteral("cannot seek to %1").arg(path));

    *out = file.read(node.size);
    if (out->size() != node.size)
        return fail(error, QStringLiteral("truncated payload for %1").arg(path));
    return true;
}

QJsonObject AsarArchive::verifyUnpacked(qint64 maxBytes) const
{
    int total = 0;
    int verified = 0;
    int sizeMismatch = 0;
    int hashMismatch = 0;
    int missingIntegrity = 0;
    int unreadable = 0;
    int tooLarge = 0;

    for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
        const Node &node = it.value();
        if (!node.unpacked || node.directory || !node.link.isEmpty())
            continue;
        ++total;
        const QString path = safeUnpackedFile(unpackedRoot(), it.key());
        QFile file(path);
        if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
            ++unreadable;
            continue;
        }
        if (maxBytes < 0 || file.size() > maxBytes) {
            ++tooLarge;
            continue;
        }

        const QByteArray expected = node.integrityHash.toLatin1().toLower();
        const bool hasHash = node.integrityAlgorithm.compare(QLatin1String("SHA256"),
                                                               Qt::CaseInsensitive) == 0
            && expected.size() == 64
            && std::all_of(expected.cbegin(), expected.cend(), [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            });
        if (!hasHash)
            ++missingIntegrity;

        QCryptographicHash hash(QCryptographicHash::Sha256);
        qint64 bytesRead = 0;
        bool exceeded = false;
        while (!file.atEnd()) {
            const qint64 remaining = maxBytes - bytesRead;
            const qint64 chunkSize = remaining < 65536 ? remaining + 1 : 65536;
            const QByteArray chunk = file.read(chunkSize);
            if (chunk.isEmpty())
                break;
            if (chunk.size() > remaining) {
                exceeded = true;
                break;
            }
            bytesRead += chunk.size();
            hash.addData(chunk);
        }
        if (exceeded) {
            ++tooLarge;
            continue;
        }
        if (file.error() != QFileDevice::NoError || bytesRead != file.size()) {
            ++unreadable;
            continue;
        }
        const bool sizeMatches = bytesRead == node.size;
        const bool hashMatches = hasHash && hash.result().toHex() == expected;
        if (!sizeMatches)
            ++sizeMismatch;
        if (hasHash && !hashMatches)
            ++hashMismatch;
        if (sizeMatches && hashMatches)
            ++verified;
    }

    return {{QStringLiteral("ok"), m_open && total == verified},
            {QStringLiteral("total"), total},
            {QStringLiteral("verified"), verified},
            {QStringLiteral("sizeMismatch"), sizeMismatch},
            {QStringLiteral("hashMismatch"), hashMismatch},
            {QStringLiteral("missingIntegrity"), missingIntegrity},
            {QStringLiteral("unreadable"), unreadable},
            {QStringLiteral("tooLarge"), tooLarge}};
}

} // namespace ZaloNative
