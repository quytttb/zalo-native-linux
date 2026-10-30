#include "MessageModel.h"

#include <QJsonObject>

namespace ZaloNative {

namespace {

struct Parsed
{
    QString id;
    QString sender;
    QString text;
    QString time;
    QString status;
    QString kind;
    QString attachmentName;
    qint64 attachmentSize = 0;
    bool outgoing = false;
};

Parsed parseMessage(const QJsonObject &object, const QString &idPrefix)
{
    Parsed parsed;
    parsed.id = object.value(QStringLiteral("id")).toString(idPrefix);
    parsed.sender = object.value(QStringLiteral("sender")).toString();
    parsed.text = object.value(QStringLiteral("text")).toString();
    parsed.time = object.value(QStringLiteral("time")).toString();
    parsed.status = object.value(QStringLiteral("status")).toString(QStringLiteral("sent"));
    parsed.kind = object.value(QStringLiteral("kind")).toString(QStringLiteral("text"));
    parsed.attachmentName = object.value(QStringLiteral("attachmentName")).toString();
    parsed.attachmentSize = object.value(QStringLiteral("attachmentSize")).toInteger();
    parsed.outgoing = object.value(QStringLiteral("outgoing")).toBool();
    return parsed;
}

} // namespace

MessageModel::MessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return row.text;
    case IdRole:
        return row.id;
    case SenderRole:
        return row.sender;
    case TimeRole:
        return row.time;
    case OutgoingRole:
        return row.outgoing;
    case StatusRole:
        return row.status;
    case KindRole:
        return row.kind;
    case AttachmentNameRole:
        return row.attachmentName;
    case AttachmentSizeRole:
        return row.attachmentSize;
    default:
        return {};
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {SenderRole, "sender"},
        {TextRole, "text"},
        {TimeRole, "time"},
        {OutgoingRole, "outgoing"},
        {StatusRole, "status"},
        {KindRole, "kind"},
        {AttachmentNameRole, "attachmentName"},
        {AttachmentSizeRole, "attachmentSize"},
    };
}

void MessageModel::setMessages(const QJsonArray &messages)
{
    QVector<Row> rows;
    rows.reserve(messages.size());

    int index = 0;
    for (const QJsonValue &value : messages) {
        if (!value.isObject())
            continue;
        const Parsed parsed = parseMessage(value.toObject(), QStringLiteral("m%1").arg(index++));
        Row row;
        row.id = parsed.id;
        row.sender = parsed.sender;
        row.text = parsed.text;
        row.time = parsed.time;
        row.status = parsed.status;
        row.kind = parsed.kind;
        row.attachmentName = parsed.attachmentName;
        row.attachmentSize = parsed.attachmentSize;
        row.outgoing = parsed.outgoing;
        rows.append(row);
    }

    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    emit countChanged();
}

void MessageModel::appendMessage(const QJsonObject &message)
{
    const Parsed parsed = parseMessage(message, QStringLiteral("local"));
    Row row;
    row.id = parsed.id;
    row.sender = parsed.sender;
    row.text = parsed.text;
    row.time = parsed.time;
    row.status = parsed.status;
    row.kind = parsed.kind;
    row.attachmentName = parsed.attachmentName;
    row.attachmentSize = parsed.attachmentSize;
    row.outgoing = parsed.outgoing;

    const int position = int(m_rows.size());
    beginInsertRows(QModelIndex(), position, position);
    m_rows.append(row);
    endInsertRows();
    emit countChanged();
}

void MessageModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

QString MessageModel::lastMessageId() const
{
    return m_rows.isEmpty() ? QString() : m_rows.last().id;
}

} // namespace ZaloNative
