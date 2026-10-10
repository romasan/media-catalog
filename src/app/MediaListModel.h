#pragma once

#include "core/MediaTypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace mc {

class MediaLibrary;

/// Список медиафайлов для сетки QML.
///
/// Строится из `MediaLibrary::filteredMedia()` (уже отфильтровано и
/// отсортировано) и полностью пересобирается по сигналу `libraryChanged`.
/// Обновление одного превью не сбрасывает модель: строка правится точечно,
/// иначе сетка теряла бы позицию прокрутки при каждой готовой картинке.
class MediaListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    /// Идентификаторы выбранных файлов (для массового тегирования).
    Q_PROPERTY(QStringList selectedIds READ selectedIds WRITE setSelectedIds NOTIFY selectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    /// Сколько фото и видео в текущей выдаче — для счётчика в панели фильтра.
    Q_PROPERTY(int photoCount READ photoCount NOTIFY countChanged)
    Q_PROPERTY(int videoCount READ videoCount NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        PathRole,
        TypeRole,
        IsVideoRole,
        SizeRole,
        SizeTextRole,
        ModifiedAtRole,
        CapturedAtRole,
        HasCapturedAtRole,
        ThumbnailPathRole,
        ThumbnailUrlRole,
        FileUrlRole,
        SelectedRole,
    };
    Q_ENUM(Role)

    explicit MediaListModel(MediaLibrary *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QStringList selectedIds() const { return m_selectedIds; }
    void setSelectedIds(const QStringList &ids);
    int selectedCount() const { return m_selectedIds.size(); }
    int photoCount() const { return m_photoCount; }
    int videoCount() const { return m_videoCount; }

    Q_INVOKABLE QString idAt(int row) const;
    Q_INVOKABLE int rowForId(const QString &mediaId) const;
    Q_INVOKABLE void toggleSelection(const QString &mediaId);
    Q_INVOKABLE void selectOnly(const QString &mediaId);
    /// Выделяет непрерывный диапазон — для Shift+клика.
    Q_INVOKABLE void selectRange(const QString &fromId, const QString &toId);
    Q_INVOKABLE void clearSelection();
    /// Выделяет все видимые (то есть все отфильтрованные) файлы.
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE QStringList allIds() const;
    /// Минимум и максимум даты изменения по строкам [first, last] —
    /// для подписи с диапазоном дат видимых карточек.
    Q_INVOKABLE QVariantMap dateRangeInRange(int first, int last) const;

public slots:
    void refresh();

signals:
    void countChanged();
    void selectionChanged();

private:
    void rebuildMaps();
    QVariant dataForRow(int row, int role) const;

    MediaLibrary *m_library = nullptr;
    QList<MediaFile> m_media;
    QVector<QVariantMap> m_items;
    QHash<QString, int> m_rowById;
    QStringList m_selectedIds;
    QSet<QString> m_selectedSet;
    int m_photoCount = 0;
    int m_videoCount = 0;
};

} // namespace mc
