#pragma once

#include "core/CatalogDatabase.h"
#include "core/MediaTypes.h"
#include "core/Paths.h"

#include <QFuture>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QThreadPool;
class QTimer;
QT_END_NAMESPACE

namespace mc {

class MediaScanner;
class ThumbnailGenerator;

/// Единая точка входа в данные для UI.
///
/// Владеет базой, сканером и генератором превью и предоставляет QML
/// готовые к вызову операции. Списочные модели (`MediaListModel`,
/// `TagListModel`) читают данные отсюда и перестраиваются по сигналу
/// `libraryChanged()`.
class MediaLibrary : public QObject
{
    Q_OBJECT

    /// Увеличивается при любом изменении данных — модели реагируют на него.
    Q_PROPERTY(int version READ version NOTIFY libraryChanged)
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(bool generatingThumbnails READ isGeneratingThumbnails NOTIFY thumbnailProgressChanged)
    Q_PROPERTY(bool hasCatalogs READ hasCatalogs NOTIFY libraryChanged)
    Q_PROPERTY(int photoCount READ photoCount NOTIFY libraryChanged)
    Q_PROPERTY(int videoCount READ videoCount NOTIFY libraryChanged)
    Q_PROPERTY(int mediaCount READ mediaCount NOTIFY libraryChanged)
    Q_PROPERTY(int tagCount READ tagCount NOTIFY libraryChanged)
    Q_PROPERTY(qreal thumbnailProgress READ thumbnailProgress NOTIFY thumbnailProgressChanged)
    Q_PROPERTY(QString dataDirectory READ dataDirectory CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit MediaLibrary(QObject *parent = nullptr);
    ~MediaLibrary() override;

    /// Открывает базу и, если каталоги уже добавлены, запускает сканирование.
    bool initialize(const QString &overrideDataDir = QString());

    int version() const { return m_version; }
    bool isScanning() const { return m_scanning; }
    bool isGeneratingThumbnails() const;
    bool hasCatalogs() const;
    int photoCount() const;
    int videoCount() const;
    int mediaCount() const;
    int tagCount() const;
    qreal thumbnailProgress() const;
    QString dataDirectory() const { return m_paths.dataDir; }
    QString lastError() const { return m_lastError; }

    CatalogDatabase &database() { return m_database; }
    const AppPaths &paths() const { return m_paths; }
    const CatalogDatabase &database() const { return m_database; }

    // ==== Каталоги ====

    Q_INVOKABLE QVariantList catalogs() const;
    /// Статистика по каталогам: сколько фото и видео найдено в каждом.
    Q_INVOKABLE QVariantList catalogStats() const;
    Q_INVOKABLE void addCatalog(const QString &path);
    Q_INVOKABLE void removeCatalog(const QString &catalogId);
    Q_INVOKABLE void requestInitialScan();
    /// Возвращает предполагаемую папку по умолчанию для диалога выбора.
    Q_INVOKABLE QString defaultCatalogDirectory() const;

    // ==== Медиа ====

    /// Отфильтрованные и отсортированные (сначала новые) файлы.
    QList<MediaFile> filteredMedia() const;
    Q_INVOKABLE QVariantMap mediaAt(int index) const;
    Q_INVOKABLE QVariantMap mediaById(const QString &mediaId) const;
    Q_INVOKABLE void requestCaptureDate(const QString &mediaId);
    Q_INVOKABLE void showInFolder(const QString &filePath);
    /// Дата съёмки, уже известная из базы (0, если не вычислялась).
    qint64 knownCaptureDate(const QString &mediaId) const;

    // ==== Теги ====

    Q_INVOKABLE QVariantList tags() const;
    Q_INVOKABLE QVariantList metaTags() const;
    Q_INVOKABLE QVariantList mediaTags(const QString &mediaId) const;
    /// Пользовательские теги файла (без метатегов) — для массового тегирования.
    Q_INVOKABLE QStringList mediaUserTagIds(const QString &mediaId) const;
    Q_INVOKABLE QVariantList mediaMetaTags(const QString &mediaId) const;
    Q_INVOKABLE QVariantList searchTags(const QString &query) const;
    Q_INVOKABLE QString createTag(const QString &name);
    Q_INVOKABLE void deleteTag(const QString &tagId);
    Q_INVOKABLE void applyTag(const QString &mediaId, const QString &tagId);
    Q_INVOKABLE void applyTagToMedia(const QStringList &mediaIds, const QString &tagId);
    Q_INVOKABLE void removeTagFromMedia(const QString &mediaId, const QString &tagId);

    // ==== Фильтр ====

    FilterCondition filter() const { return m_filter; }
    Q_INVOKABLE void setFilterTagIds(const QStringList &tagIds);
    Q_INVOKABLE void setFilterMode(const QString &mode);
    Q_INVOKABLE QStringList filterTagIds() const { return m_filter.tagIds; }
    Q_INVOKABLE QString filterMode() const;
    /// Добавляет тег в фильтр с правилом взаимоисключения метатега «без тега».
    Q_INVOKABLE void addTagToFilter(const QString &tagId);
    Q_INVOKABLE void removeTagFromFilter(const QString &tagId);
    Q_INVOKABLE void clearFilter();
    /// Метатег по идентификатору — для отображения чипов фильтра.
    Q_INVOKABLE QVariantMap metaTagById(const QString &metaTagId) const;
    /// Отображаемое имя тега по идентификатору (метатеги и обычные теги).
    Q_INVOKABLE QString tagName(const QString &tagId) const;

    // ==== Экспорт / импорт ====

    /// true — экспорт, false — импорт.
    Q_INVOKABLE bool transferData(bool exportMode, const QString &filePath);

signals:
    void libraryChanged();
    void scanningChanged();
    void thumbnailProgressChanged();
    void scanFinished(const mc::ScanResult &result);
    void thumbnailReady(const QString &mediaId, const QString &thumbnailPath);
    void captureDateReady(const QString &mediaId, qint64 capturedAt, bool found);
    void toast(const QString &message, const QString &type);
    void lastErrorChanged();

private slots:
    void onThumbnailProgress(const mc::ThumbnailProgress &progress);
    void onThumbnailReady(const QString &mediaId, const QString &thumbnailPath);

private:
    void setLastError(const QString &error);
    void runScan();
    void startScanThread();
    void bumpVersion();

    AppPaths m_paths;
    CatalogDatabase m_database;
    MediaScanner *m_scanner = nullptr;
    ThumbnailGenerator *m_thumbnails = nullptr;
    QThreadPool *m_scanPool = nullptr;
    QThreadPool *m_metadataPool = nullptr;
    QFuture<void> m_scanFuture;
    FilterCondition m_filter;
    QSet<QString> m_pendingCaptureDates;
    bool m_scanQueued = false;
    bool m_scanning = false;
    int m_version = 0;
    int m_photoCount = 0;
    int m_videoCount = 0;
    int m_mediaCount = 0;
    int m_tagCount = 0;
    qreal m_thumbnailProgress = 0.0;
    QString m_lastError;

    void recount();
};

} // namespace mc
