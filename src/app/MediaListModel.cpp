#include "app/MediaListModel.h"

#include "core/MediaLibrary.h"

#include <QFileInfo>
#include <QLocale>
#include <QUrl>

namespace mc {

namespace {

QVariantMap buildItem(const MediaFile &media, bool selected)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), media.id);
    map.insert(QStringLiteral("name"), media.name);
    map.insert(QStringLiteral("path"), media.path);
    map.insert(QStringLiteral("type"), media.kind == MediaKind::Video
                                           ? QStringLiteral("video")
                                           : QStringLiteral("photo"));
    map.insert(QStringLiteral("isVideo"), media.kind == MediaKind::Video);
    map.insert(QStringLiteral("size"), static_cast<qulonglong>(media.size));
    map.insert(QStringLiteral("sizeText"), QLocale().formattedDataSize(media.size));
    map.insert(QStringLiteral("modifiedAt"), media.modifiedAt);
    map.insert(QStringLiteral("capturedAt"), media.capturedAt);
    map.insert(QStringLiteral("hasCapturedAt"), media.hasCapturedAt);
    map.insert(QStringLiteral("thumbnailPath"), media.thumbnailPath);
    map.insert(QStringLiteral("fileUrl"), QUrl::fromLocalFile(media.path).toString());

    // Пустая ссылка означает «превью ещё нет»: QML покажет заглушку.
    const QString thumbnailUrl = media.thumbnailPath.isEmpty()
        ? QString()
        : QUrl::fromLocalFile(media.thumbnailPath).toString();
    map.insert(QStringLiteral("thumbnailUrl"), thumbnailUrl);

    map.insert(QStringLiteral("selected"), selected);
    return map;
}

} // namespace

MediaListModel::MediaListModel(MediaLibrary *library, QObject *parent)
    : QAbstractListModel(parent)
    , m_library(library)
{
    connect(m_library, &MediaLibrary::libraryChanged, this, &MediaListModel::refresh);
    connect(m_library, &MediaLibrary::thumbnailReady, this,
            [this](const QString &mediaId, const QString &thumbnailPath) {
                const auto it = m_rowById.constFind(mediaId);
                if (it == m_rowById.constEnd()) {
                    return;
                }
                const int row = it.value();
                if (row < 0 || row >= m_media.size()) {
                    return;
                }

                m_media[row].thumbnailPath = thumbnailPath;
                m_media[row].thumbnailRetries = 0;
                m_items[row] = buildItem(m_media.at(row), m_selectedSet.contains(mediaId));

                // Правим одну строку: полный сброс сбил бы прокрутку сетки.
                const QModelIndex changed = index(row, 0);
                emit dataChanged(changed, changed, {ThumbnailPathRole, ThumbnailUrlRole});
            });

    refresh();
}

int MediaListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_media.size();
}

QVariant MediaListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }
    return dataForRow(index.row(), role);
}

QVariant MediaListModel::dataForRow(int row, int role) const
{
    const QVariantMap &item = m_items.at(row);
    switch (role) {
    case IdRole:
        return item.value(QStringLiteral("id"));
    case NameRole:
        return item.value(QStringLiteral("name"));
    case PathRole:
        return item.value(QStringLiteral("path"));
    case TypeRole:
        return item.value(QStringLiteral("type"));
    case IsVideoRole:
        return item.value(QStringLiteral("isVideo"));
    case SizeRole:
        return item.value(QStringLiteral("size"));
    case SizeTextRole:
        return item.value(QStringLiteral("sizeText"));
    case ModifiedAtRole:
        return item.value(QStringLiteral("modifiedAt"));
    case CapturedAtRole:
        return item.value(QStringLiteral("capturedAt"));
    case HasCapturedAtRole:
        return item.value(QStringLiteral("hasCapturedAt"));
    case ThumbnailPathRole:
        return item.value(QStringLiteral("thumbnailPath"));
    case ThumbnailUrlRole:
        return item.value(QStringLiteral("thumbnailUrl"));
    case FileUrlRole:
        return item.value(QStringLiteral("fileUrl"));
    case SelectedRole:
        return item.value(QStringLiteral("selected"));
    default:
        return {};
    }
}

QHash<int, QByteArray> MediaListModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("mediaId")},
        {NameRole, QByteArrayLiteral("name")},
        {PathRole, QByteArrayLiteral("path")},
        {TypeRole, QByteArrayLiteral("type")},
        {IsVideoRole, QByteArrayLiteral("isVideo")},
        {SizeRole, QByteArrayLiteral("size")},
        {SizeTextRole, QByteArrayLiteral("sizeText")},
        {ModifiedAtRole, QByteArrayLiteral("modifiedAt")},
        {CapturedAtRole, QByteArrayLiteral("capturedAt")},
        {HasCapturedAtRole, QByteArrayLiteral("hasCapturedAt")},
        {ThumbnailPathRole, QByteArrayLiteral("thumbnailPath")},
        {ThumbnailUrlRole, QByteArrayLiteral("thumbnailUrl")},
        {FileUrlRole, QByteArrayLiteral("fileUrl")},
        {SelectedRole, QByteArrayLiteral("selected")},
    };
}

void MediaListModel::refresh()
{
    beginResetModel();
    m_media = m_library->filteredMedia();
    rebuildMaps();

    // Отбрасываем выделение файлов, которых больше нет в списке.
    QStringList stillPresent;
    for (const QString &id : std::as_const(m_selectedIds)) {
        if (m_rowById.contains(id)) {
            stillPresent.append(id);
        }
    }
    if (stillPresent.size() != m_selectedIds.size()) {
        m_selectedIds = stillPresent;
        m_selectedSet = QSet<QString>(stillPresent.cbegin(), stillPresent.cend());
        emit selectionChanged();
    }
    endResetModel();

    emit countChanged();
}

void MediaListModel::rebuildMaps()
{
    m_photoCount = 0;
    m_videoCount = 0;
    for (const MediaFile &media : std::as_const(m_media)) {
        if (media.kind == MediaKind::Video) {
            ++m_videoCount;
        } else {
            ++m_photoCount;
        }
    }

    m_items.clear();
    m_items.reserve(m_media.size());
    m_rowById.clear();
    m_rowById.reserve(m_media.size());
    for (int row = 0; row < m_media.size(); ++row) {
        const MediaFile &media = m_media.at(row);
        m_items.append(buildItem(media, m_selectedSet.contains(media.id)));
        m_rowById.insert(media.id, row);
    }
}

void MediaListModel::setSelectedIds(const QStringList &ids)
{
    if (m_selectedIds == ids) {
        return;
    }
    m_selectedIds = ids;
    m_selectedSet = QSet<QString>(ids.cbegin(), ids.cend());

    // Обновляем роль selected у затронутых строк.
    for (int row = 0; row < m_media.size(); ++row) {
        const bool selected = m_selectedSet.contains(m_media.at(row).id);
        if (m_items[row].value(QStringLiteral("selected")).toBool() != selected) {
            m_items[row].insert(QStringLiteral("selected"), selected);
            const QModelIndex changed = index(row, 0);
            emit dataChanged(changed, changed, {SelectedRole});
        }
    }

    emit selectionChanged();
}

QString MediaListModel::idAt(int row) const
{
    if (row < 0 || row >= m_media.size()) {
        return {};
    }
    return m_media.at(row).id;
}

int MediaListModel::rowForId(const QString &mediaId) const
{
    return m_rowById.value(mediaId, -1);
}

void MediaListModel::toggleSelection(const QString &mediaId)
{
    QStringList ids = m_selectedIds;
    if (ids.contains(mediaId)) {
        ids.removeAll(mediaId);
    } else {
        ids.append(mediaId);
    }
    setSelectedIds(ids);
}

void MediaListModel::selectOnly(const QString &mediaId)
{
    setSelectedIds({mediaId});
}

void MediaListModel::selectRange(const QString &fromId, const QString &toId)
{
    const int from = rowForId(fromId);
    const int to = rowForId(toId);
    if (from < 0 || to < 0) {
        return;
    }
    const int first = qMin(from, to);
    const int last = qMax(from, to);

    QStringList ids;
    ids.reserve(last - first + 1);
    for (int row = first; row <= last; ++row) {
        ids.append(m_media.at(row).id);
    }
    setSelectedIds(ids);
}

void MediaListModel::clearSelection()
{
    setSelectedIds({});
}

void MediaListModel::selectAll()
{
    QStringList ids;
    ids.reserve(m_media.size());
    for (const MediaFile &media : std::as_const(m_media)) {
        ids.append(media.id);
    }
    setSelectedIds(ids);
}

QStringList MediaListModel::allIds() const
{
    QStringList ids;
    ids.reserve(m_media.size());
    for (const MediaFile &media : std::as_const(m_media)) {
        ids.append(media.id);
    }
    return ids;
}

QVariantMap MediaListModel::dateRangeInRange(int first, int last) const
{
    if (m_media.isEmpty()) {
        return {};
    }
    first = qBound(0, first, m_media.size() - 1);
    last = qBound(0, last, m_media.size() - 1);
    if (last < first) {
        return {};
    }

    qint64 minValue = std::numeric_limits<qint64>::max();
    qint64 maxValue = std::numeric_limits<qint64>::min();
    for (int row = first; row <= last; ++row) {
        const qint64 modifiedAt = m_media.at(row).modifiedAt;
        minValue = qMin(minValue, modifiedAt);
        maxValue = qMax(maxValue, modifiedAt);
    }

    QVariantMap range;
    range.insert(QStringLiteral("min"), minValue);
    range.insert(QStringLiteral("max"), maxValue);
    return range;
}

} // namespace mc
