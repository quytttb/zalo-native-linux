#include "ConversationFilterModel.h"

#include "ConversationModel.h"

namespace ZaloNative {

ConversationFilterModel::ConversationFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void ConversationFilterModel::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;
    m_filterText = text;
    invalidateFilter();
    emit filterTextChanged();
}

bool ConversationFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filterText.trimmed().isEmpty())
        return true;

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString title = index.data(ConversationModel::TitleRole).toString();
    const QString snippet = index.data(ConversationModel::SnippetRole).toString();
    return title.contains(m_filterText, Qt::CaseInsensitive)
        || snippet.contains(m_filterText, Qt::CaseInsensitive);
}

} // namespace ZaloNative
