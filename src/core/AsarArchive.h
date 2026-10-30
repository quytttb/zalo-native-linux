#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace ZaloNative {

// Read-only reader for Electron ASAR archives.
//
// An ASAR file is a small Chromium pickle header followed by a concatenation of
// file payloads:
//
//   u32 @ 0    : 4 (size of the next field)
//   u32 @ 4    : header pickle length (H)
//   u32 @ 8    : header pickle payload length
//   u32 @ 12   : JSON header length (J)
//   bytes 16.. : JSON header (J bytes)
//   data base  : 8 + H
//
// Header entries are either directories ({"files": {...}}), links
// ({"link": "..."}) or files ({"size": N, "offset": "N"}). `unpacked` files live
// in "<archive>.unpacked/<path>" instead of inside the archive.
class AsarArchive
{
public:
    AsarArchive() = default;
    AsarArchive(const AsarArchive &) = delete;
    AsarArchive &operator=(const AsarArchive &) = delete;
    AsarArchive(AsarArchive &&) = default;
    AsarArchive &operator=(AsarArchive &&) = default;

    static std::optional<AsarArchive> open(const QString &path, QString *error = nullptr);

    bool isOpen() const { return m_open; }
    QString archivePath() const { return m_path; }
    QString unpackedRoot() const;
    qint64 dataOffset() const { return m_dataOffset; }
    int nodeCount() const { return m_nodes.size(); }

    const QJsonObject &header() const { return m_header; }

    // Keys are normalized, without a leading slash ("index.html", "a/b.js").
    QStringList entries() const;
    bool contains(const QString &path) const;
    bool isFile(const QString &path) const;
    bool isDirectory(const QString &path) const;

    // Reads a regular file. Refuses directories, unsafe paths, links that leave
    // the archive and payloads larger than maxBytes.
    bool readFile(const QString &path, QByteArray *out, QString *error = nullptr,
                  qint64 maxBytes = 512LL * 1024 * 1024) const;

private:
    struct Node
    {
        bool directory = false;
        bool unpacked = false;
        bool executable = false;
        qint64 offset = 0;
        qint64 size = 0;
        QString link;
    };

    static QString normalizeKey(const QString &raw);

    bool m_open = false;
    QString m_path;
    qint64 m_fileSize = 0;
    qint64 m_dataOffset = 0;
    QJsonObject m_header;
    QHash<QString, Node> m_nodes;
};

} // namespace ZaloNative
