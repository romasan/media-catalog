#pragma once

#include "core/MediaTypes.h"

#include <QAbstractListModel>
#include <QVector>

namespace mc {

class MediaLibrary;

/// Список тегов для менеджера тегов и автодополнения.
///
/// Модель умеет показывать либо пользовательские теги (`metaMode = false`),
/// либо метатеги (`metaMode = true`), а также фильтровать по подстроке.
class TagListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool metaMode READ metaMode WRITE setMetaMode NOTIFY metaModeChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        CountRole,
        GroupRole,
        IsMetaRole,
        LastUsedAtRole,
    };
    Q_ENUM(Role)

    explicit TagListModel(MediaLibrary *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const { return m_query; }
    void setQuery(const QString &query);
    bool metaMode() const { return m_metaMode; }
    void setMetaMode(bool metaMode);

    /// Имена тегов, встречающиеся в текущем списке — нужны автодополнению.
    Q_INVOKABLE QStringList names() const;

public slots:
    void refresh();

signals:
    void countChanged();
    void queryChanged();
    void metaModeChanged();

private:
    MediaLibrary *m_library = nullptr;
    QVector<QVariantMap> m_items;
    QString m_query;
    bool m_metaMode = false;
};

} // namespace mc
