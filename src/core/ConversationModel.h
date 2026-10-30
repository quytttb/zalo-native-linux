#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QString>
#include <QVector>

namespace ZaloNative {

class ConversationModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        SnippetRole,
        TimeRole,
        UnreadRole,
        PinnedRole,
        MutedRole,
        OnlineRole,
        AvatarTextRole,
        AvatarColorRole,
        LastActivityTsRole,
    };
    Q_ENUM(Roles)

    explicit ConversationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setConversations(const QJsonArray &conversations);
    void clear();

    Q_INVOKABLE QString idAt(int row) const;
    Q_INVOKABLE int rowForId(const QString &id) const;

    // Updates the preview in place without reordering, so the current selection
    // survives sending a message.
    void touch(const QString &id, const QString &snippet, const QString &time, qint64 timestamp);
    void markRead(const QString &id);

signals:
    void countChanged();

private:
    struct Row
    {
        QString id;
        QString title;
        QString snippet;
        QString time;
        QString avatarText;
        QString avatarColor;
        int unread = 0;
        bool pinned = false;
        bool muted = false;
        bool online = false;
        qint64 timestamp = 0;
    };

    QVector<Row> m_rows;
};

} // namespace ZaloNative
