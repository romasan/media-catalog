#pragma once

#include "core/MediaTypes.h"

#include <QObject>
#include <QString>
#include <QVariantList>

// Типы подключаем полностью: Q_DECLARE_OPAQUE_POINTER здесь недопустим —
// QML перестаёт видеть свойства и методы объектов, объявленных в Q_PROPERTY,
// и вместо них показывает непрозрачный QVariant.
#include "app/MediaListModel.h"
#include "app/TagListModel.h"
#include "core/MediaLibrary.h"

namespace mc {

/// Мост между C++ и QML: владеет моделями, выделением и диалогами.
class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(mc::MediaLibrary *library READ library CONSTANT)
    Q_PROPERTY(mc::MediaListModel *mediaModel READ mediaModel CONSTANT)
    Q_PROPERTY(mc::TagListModel *tagModel READ tagModel CONSTANT)
    Q_PROPERTY(mc::TagListModel *metaTagModel READ metaTagModel CONSTANT)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(bool anyCatalog READ anyCatalog NOTIFY anyCatalogChanged)
    Q_PROPERTY(QString defaultCatalogDirectory READ defaultCatalogDirectory NOTIFY anyCatalogChanged)
    Q_PROPERTY(QString lastExportDirectory READ lastExportDirectory NOTIFY lastExportDirectoryChanged)
    Q_PROPERTY(QString applicationVersion READ applicationVersion CONSTANT)

public:
    explicit AppController(MediaLibrary *library, QObject *parent = nullptr);

    MediaLibrary *library() const { return m_library; }
    MediaListModel *mediaModel() const { return m_mediaModel; }
    TagListModel *tagModel() const { return m_tagModel; }
    TagListModel *metaTagModel() const { return m_metaTagModel; }

    int selectedCount() const;
    bool hasSelection() const;
    bool anyCatalog() const;
    QString defaultCatalogDirectory() const;
    QString lastExportDirectory() const { return m_lastExportDirectory; }
    QString applicationVersion() const;

    Q_INVOKABLE void addCatalog(const QString &path);
    Q_INVOKABLE void removeCatalog(const QString &catalogId);
    Q_INVOKABLE QStringList selectedIds() const;
    Q_INVOKABLE void applyTagToSelection(const QString &tagId);
    Q_INVOKABLE void createTagAndApplyToSelection(const QString &name);
    Q_INVOKABLE void revealInFileManager(const QString &filePath);
    Q_INVOKABLE void requestCaptureDate(const QString &mediaId) const;
    Q_INVOKABLE QString formatDateTime(qint64 timestampMs) const;
    Q_INVOKABLE QString formatFileSize(qint64 bytes) const;
    /// Экспорт/импорт с диалогом выбора файла на стороне QML.
    Q_INVOKABLE bool transferData(bool exportMode, const QString &filePath);
    Q_INVOKABLE void notifyExportDirectory(const QString &filePath);

signals:
    void selectionChanged();
    void anyCatalogChanged();
    void lastExportDirectoryChanged();

private:
    void updateSelectionState();

    MediaLibrary *m_library = nullptr;
    MediaListModel *m_mediaModel = nullptr;
    TagListModel *m_tagModel = nullptr;
    TagListModel *m_metaTagModel = nullptr;
    int m_selectedCount = 0;
    bool m_anyCatalog = false;
    QString m_lastExportDirectory;
};

} // namespace mc
