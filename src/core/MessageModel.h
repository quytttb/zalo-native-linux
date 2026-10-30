#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QString>
#include <QVector>

namespace ZaloNative {

class MessageModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        SenderRole,
        TextRole,
        TimeRole,
        OutgoingRole,
        StatusRole,
        KindRole,
        AttachmentNameRole,
        AttachmentSizeRole,
    };
    Q_ENUM(Roles)

    explicit MessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMessages(const QJsonArray &messages);
    void appendMessage(const QJsonObject &message);
    void clear();

    Q_INVOKABLE QString lastMessageId() const;

signals:
    void countChanged();

private:
    struct Row
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

    QVector<Row> m_rows;
};

} // namespace ZaloNative
