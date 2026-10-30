#include "ConversationModel.h"

#include <QJsonObject>

#include <algorithm>

namespace ZaloNative {

namespace {

const QStringList kAvatarPalette = {
    QStringLiteral("#3D6DFF"), QStringLiteral("#7C5CFF"), QStringLiteral("#0FA3A3"),
    QStringLiteral("#D96C3B"), QStringLiteral("#C24B8B"), QStringLiteral("#4C8C3F"),
    QStringLiteral("#B08900"), QStringLiteral("#5A6B8C"),
};

QString initialsFor(const QString &title)
{
    const QStringList words = title.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString out;
    for (const QString &word : words) {
        if (out.size() >= 2)
            break;
        const QChar first = word.at(0);
        if (first.isLetterOrNumber())
            out.append(first.toUpper());
    }
    return out.isEmpty() ? QStringLiteral("?") : out;
}

QString colorFor(const QString &id)
{
    return kAvatarPalette.at(int(qHash(id) % uint(kAvatarPalette.size())));
}

} // namespace

ConversationModel::ConversationModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ConversationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant ConversationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return row.title;
    case IdRole:
        return row.id;
    case SnippetRole:
        return row.snippet;
    case TimeRole:
        return row.time;
    case UnreadRole:
        return row.unread;
    case PinnedRole:
        return row.pinned;
    case MutedRole:
        return row.muted;
    case OnlineRole:
        return row.online;
    case AvatarTextRole:
        return row.avatarText;
    case AvatarColorRole:
        return row.avatarColor;
    case LastActivityTsRole:
        return row.timestamp;
    default:
        return {};
    }
}

QHash<int, QByteArray> ConversationModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {SnippetRole, "snippet"},
        {TimeRole, "time"},
        {UnreadRole, "unread"},
        {PinnedRole, "pinned"},
        {MutedRole, "muted"},
        {OnlineRole, "online"},
        {AvatarTextRole, "avatarText"},
        {AvatarColorRole, "avatarColor"},
        {LastActivityTsRole, "lastActivityTs"},
    };
}

void ConversationModel::setConversations(const QJsonArray &conversations)
{
    QVector<Row> rows;
    rows.reserve(conversations.size());

    for (const QJsonValue &value : conversations) {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        if (id.isEmpty())
            continue;

        Row row;
        row.id = id;
        row.title = object.value(QStringLiteral("title")).toString(id);
        row.snippet = object.value(QStringLiteral("snippet")).toString();
        row.time = object.value(QStringLiteral("time")).toString();
        row.unread = std::max(0, object.value(QStringLiteral("unread")).toInt());
        row.pinned = object.value(QStringLiteral("pinned")).toBool();
        row.muted = object.value(QStringLiteral("muted")).toBool();
        row.online = object.value(QStringLiteral("online")).toBool();
        row.avatarText = object.value(QStringLiteral("avatarText")).toString(initialsFor(row.title));
        row.avatarColor = object.value(QStringLiteral("avatarColor")).toString(colorFor(id));
        row.timestamp = object.value(QStringLiteral("lastActivityTs")).toInteger();
        rows.append(row);
    }

    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        if (a.pinned != b.pinned)
            return a.pinned;
        return a.timestamp > b.timestamp;
    });

    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    emit countChanged();
}

void ConversationModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

QString ConversationModel::idAt(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    return m_rows.at(row).id;
}

int ConversationModel::rowForId(const QString &id) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id == id)
            return i;
    }
    return -1;
}

void ConversationModel::touch(const QString &id, const QString &snippet, const QString &time, qint64 timestamp)
{
    const int row = rowForId(id);
    if (row < 0)
        return;

    Row &entry = m_rows[row];
    entry.snippet = snippet;
    entry.time = time;
    entry.timestamp = timestamp;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed, {SnippetRole, TimeRole, LastActivityTsRole});
}

void ConversationModel::markRead(const QString &id)
{
    const int row = rowForId(id);
    if (row < 0 || m_rows.at(row).unread == 0)
        return;

    m_rows[row].unread = 0;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed, {UnreadRole});
}

} // namespace ZaloNative
