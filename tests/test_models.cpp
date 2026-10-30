#include "ConversationFilterModel.h"
#include "ConversationModel.h"
#include "MessageModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

using namespace ZaloNative;

namespace {

QJsonObject conversation(const QString &id, const QString &title, const QString &snippet,
                         qint64 timestamp, bool pinned = false, int unread = 0)
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("title"), title},
        {QStringLiteral("snippet"), snippet},
        {QStringLiteral("time"), QStringLiteral("09:00")},
        {QStringLiteral("lastActivityTs"), timestamp},
        {QStringLiteral("pinned"), pinned},
        {QStringLiteral("unread"), unread},
    };
}

} // namespace

class ModelsTest : public QObject
{
    Q_OBJECT

private slots:
    void sortsPinnedAndRecentConversations();
    void filtersByTitleAndSnippet();
    void updatesPreviewWithoutReordering();
    void tracksUnreadState();
    void exposesMessageRoles();
};

void ModelsTest::sortsPinnedAndRecentConversations()
{
    ConversationModel model;
    model.setConversations(QJsonArray{
        conversation(QStringLiteral("a"), QStringLiteral("An"), QStringLiteral("old"), 100),
        conversation(QStringLiteral("b"), QStringLiteral("Bình"), QStringLiteral("new"), 300),
        conversation(QStringLiteral("c"), QStringLiteral("Chi"), QStringLiteral("pinned"), 200, true),
    });

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.idAt(0), QStringLiteral("c"));
    QCOMPARE(model.idAt(1), QStringLiteral("b"));
    QCOMPARE(model.idAt(2), QStringLiteral("a"));
    QCOMPARE(model.rowForId(QStringLiteral("b")), 1);
    QCOMPARE(model.rowForId(QStringLiteral("missing")), -1);

    const QModelIndex index = model.index(1, 0);
    QCOMPARE(index.data(ConversationModel::TitleRole).toString(), QStringLiteral("Bình"));
    QCOMPARE(index.data(ConversationModel::AvatarTextRole).toString(), QStringLiteral("B"));
    QVERIFY(index.data(ConversationModel::AvatarColorRole).toString().startsWith(QLatin1Char('#')));
}

void ModelsTest::filtersByTitleAndSnippet()
{
    ConversationModel source;
    source.setConversations(QJsonArray{
        conversation(QStringLiteral("a"), QStringLiteral("Lan Nguyễn"), QStringLiteral("hẹn chiều nay"), 100),
        conversation(QStringLiteral("b"), QStringLiteral("Minh Trần"), QStringLiteral("báo cáo tuần"), 90),
        conversation(QStringLiteral("c"), QStringLiteral("Nhóm"), QStringLiteral("Lan gửi tài liệu"), 80),
    });

    ConversationFilterModel filter;
    filter.setSourceModel(&source);
    QCOMPARE(filter.rowCount(), 3);

    filter.setFilterText(QStringLiteral("lan"));
    QCOMPARE(filter.rowCount(), 2);
    QCOMPARE(filter.index(0, 0).data(ConversationModel::IdRole).toString(), QStringLiteral("a"));
    QCOMPARE(filter.index(1, 0).data(ConversationModel::IdRole).toString(), QStringLiteral("c"));

    filter.setFilterText(QStringLiteral("BÁO CÁO"));
    QCOMPARE(filter.rowCount(), 1);
    QCOMPARE(filter.index(0, 0).data(ConversationModel::IdRole).toString(), QStringLiteral("b"));

    filter.setFilterText(QStringLiteral("   "));
    QCOMPARE(filter.rowCount(), 3);

    filter.setFilterText(QStringLiteral("không có"));
    QCOMPARE(filter.rowCount(), 0);
}

void ModelsTest::updatesPreviewWithoutReordering()
{
    ConversationModel model;
    model.setConversations(QJsonArray{
        conversation(QStringLiteral("a"), QStringLiteral("An"), QStringLiteral("old"), 100),
        conversation(QStringLiteral("b"), QStringLiteral("Bình"), QStringLiteral("new"), 300),
    });

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    model.touch(QStringLiteral("a"), QStringLiteral("Bạn: mới"), QStringLiteral("10:00"), 400);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(model.idAt(0), QStringLiteral("b"));
    QCOMPARE(model.idAt(1), QStringLiteral("a"));
    QCOMPARE(model.index(1, 0).data(ConversationModel::SnippetRole).toString(),
             QStringLiteral("Bạn: mới"));
}

void ModelsTest::tracksUnreadState()
{
    ConversationModel model;
    model.setConversations(QJsonArray{
        conversation(QStringLiteral("a"), QStringLiteral("An"), QStringLiteral("x"), 100, false, 5),
    });
    QCOMPARE(model.index(0, 0).data(ConversationModel::UnreadRole).toInt(), 5);

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    model.markRead(QStringLiteral("a"));
    QCOMPARE(model.index(0, 0).data(ConversationModel::UnreadRole).toInt(), 0);
    QCOMPARE(changed.count(), 1);

    model.markRead(QStringLiteral("a"));
    QCOMPARE(changed.count(), 1);

    model.markRead(QStringLiteral("missing"));
    QCOMPARE(changed.count(), 1);
}

void ModelsTest::exposesMessageRoles()
{
    MessageModel model;
    model.setMessages(QJsonArray{
        QJsonObject{
            {QStringLiteral("id"), QStringLiteral("m1")},
            {QStringLiteral("sender"), QStringLiteral("Lan")},
            {QStringLiteral("text"), QStringLiteral("xin chào")},
            {QStringLiteral("time"), QStringLiteral("09:00")},
            {QStringLiteral("outgoing"), false},
            {QStringLiteral("kind"), QStringLiteral("text")},
        },
        QJsonObject{
            {QStringLiteral("sender"), QStringLiteral("Bạn")},
            {QStringLiteral("text"), QStringLiteral("chào Lan")},
            {QStringLiteral("outgoing"), true},
            {QStringLiteral("kind"), QStringLiteral("file")},
            {QStringLiteral("attachmentName"), QStringLiteral("bao-cao.pdf")},
            {QStringLiteral("attachmentSize"), 2048},
        },
    });

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.index(0, 0).data(MessageModel::SenderRole).toString(), QStringLiteral("Lan"));
    QCOMPARE(model.index(0, 0).data(MessageModel::TextRole).toString(), QStringLiteral("xin chào"));
    QCOMPARE(model.index(1, 0).data(MessageModel::OutgoingRole).toBool(), true);
    QCOMPARE(model.index(1, 0).data(MessageModel::AttachmentNameRole).toString(),
             QStringLiteral("bao-cao.pdf"));
    QCOMPARE(model.index(1, 0).data(MessageModel::AttachmentSizeRole).toLongLong(), 2048LL);
    QCOMPARE(model.lastMessageId(), QStringLiteral("m1"));

    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    model.appendMessage(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("m3")},
        {QStringLiteral("text"), QStringLiteral("thêm")},
    });
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.lastMessageId(), QStringLiteral("m3"));

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.lastMessageId(), QString());
}

QTEST_GUILESS_MAIN(ModelsTest)
#include "test_models.moc"
