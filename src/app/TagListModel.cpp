#include "app/TagListModel.h"

#include "core/MediaLibrary.h"

namespace mc {

TagListModel::TagListModel(MediaLibrary *library, QObject *parent)
    : QAbstractListModel(parent)
    , m_library(library)
{
    connect(m_library, &MediaLibrary::libraryChanged, this, &TagListModel::refresh);
    refresh();
}

int TagListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant TagListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const QVariantMap &item = m_items.at(index.row());
    switch (role) {
    case IdRole:
        return item.value(QStringLiteral("id"));
    case NameRole:
        return item.value(QStringLiteral("name"));
    case CountRole:
        return item.value(QStringLiteral("count"));
    case GroupRole:
        return item.value(QStringLiteral("group"));
    case IsMetaRole:
        return item.value(QStringLiteral("isMeta"));
    case LastUsedAtRole:
        return item.value(QStringLiteral("lastUsedAt"));
    default:
        return {};
    }
}

QHash<int, QByteArray> TagListModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("tagId")},
        {NameRole, QByteArrayLiteral("name")},
        {CountRole, QByteArrayLiteral("count")},
        {GroupRole, QByteArrayLiteral("group")},
        {IsMetaRole, QByteArrayLiteral("isMeta")},
        {LastUsedAtRole, QByteArrayLiteral("lastUsedAt")},
    };
}

void TagListModel::setQuery(const QString &query)
{
    if (m_query == query) {
        return;
    }
    m_query = query;
    emit queryChanged();
    refresh();
}

void TagListModel::setMetaMode(bool metaMode)
{
    if (m_metaMode == metaMode) {
        return;
    }
    m_metaMode = metaMode;
    emit metaModeChanged();
    refresh();
}

QStringList TagListModel::names() const
{
    QStringList result;
    result.reserve(m_items.size());
    for (const QVariantMap &item : m_items) {
        result.append(item.value(QStringLiteral("name")).toString());
    }
    return result;
}

void TagListModel::refresh()
{
    beginResetModel();

    m_items.clear();
    if (m_metaMode) {
        const QVariantList meta = m_library->metaTags();
        m_items.reserve(meta.size());
        for (const QVariant &value : meta) {
            const QVariantMap map = value.toMap();
            const QString name = map.value(QStringLiteral("name")).toString();
            if (m_query.isEmpty() || name.contains(m_query, Qt::CaseInsensitive)) {
                m_items.append(map);
            }
        }
    } else {
        const QVariantList tags = m_query.isEmpty()
            ? m_library->tags()
            : m_library->searchTags(m_query);
        m_items.reserve(tags.size());
        for (const QVariant &value : tags) {
            QVariantMap map = value.toMap();
            map.insert(QStringLiteral("isMeta"), false);
            map.insert(QStringLiteral("group"), QStringLiteral("Теги"));
            m_items.append(map);
        }
    }

    endResetModel();
    emit countChanged();
}

} // namespace mc
