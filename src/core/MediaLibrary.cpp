#include "core/MediaLibrary.h"

#include "core/ExportImport.h"
#include "core/MediaFilter.h"
#include "core/MediaScanner.h"
#include "core/MetaTags.h"
#include "core/MetadataReader.h"
#include "core/ThumbnailGenerator.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>

#include <QtConcurrent/QtConcurrentRun>

namespace mc {

namespace {

QVariantMap mediaToVariantMap(const MediaFile &media)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), media.id);
    map.insert(QStringLiteral("path"), media.path);
    map.insert(QStringLiteral("name"), media.name);
    map.insert(QStringLiteral("type"), media.kind == MediaKind::Video
                                           ? QStringLiteral("video")
                                           : QStringLiteral("photo"));
    map.insert(QStringLiteral("isVideo"), media.kind == MediaKind::Video);
    map.insert(QStringLiteral("size"), static_cast<qulonglong>(media.size));
    map.insert(QStringLiteral("sizeText"), QLocale().formattedDataSize(media.size));
    map.insert(QStringLiteral("createdAt"), media.createdAt);
    map.insert(QStringLiteral("modifiedAt"), media.modifiedAt);
    map.insert(QStringLiteral("catalogId"), media.catalogId);
    map.insert(QStringLiteral("thumbnailPath"), media.thumbnailPath);
    map.insert(QStringLiteral("hasCapturedAt"), media.hasCapturedAt);
    map.insert(QStringLiteral("capturedAt"), media.capturedAt);
    map.insert(QStringLiteral("fileUrl"), QUrl::fromLocalFile(media.path).toString());
    return map;
}

QVariantMap tagToVariantMap(const Tag &tag, int count)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), tag.id);
    map.insert(QStringLiteral("name"), tag.name);
    map.insert(QStringLiteral("lastUsedAt"), tag.lastUsedAt);
    map.insert(QStringLiteral("count"), count);
    return map;
}

QVariantMap metaTagToVariantMap(const MetaTag &tag, int count)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), tag.id);
    map.insert(QStringLiteral("name"), tag.name);
    map.insert(QStringLiteral("group"), tag.group);
    map.insert(QStringLiteral("count"), count);
    map.insert(QStringLiteral("isMeta"), true);
    return map;
}

/// Сортировка «сначала новые» — как в Electron-версии.
QList<MediaFile> sortedByDate(QList<MediaFile> media)
{
    std::sort(media.begin(), media.end(), [](const MediaFile &a, const MediaFile &b) {
        return a.modifiedAt > b.modifiedAt;
    });
    return media;
}

const QString kToastInfo = QStringLiteral("info");
const QString kToastSuccess = QStringLiteral("success");
const QString kToastError = QStringLiteral("error");

} // namespace

MediaLibrary::MediaLibrary(QObject *parent)
    : QObject(parent)
{
}

MediaLibrary::~MediaLibrary()
{
    if (m_scanFuture.isRunning()) {
        m_scanFuture.waitForFinished();
    }
    if (m_scanPool) {
        m_scanPool->waitForDone();
    }
    if (m_metadataPool) {
        m_metadataPool->waitForDone();
    }
}

bool MediaLibrary::initialize(const QString &overrideDataDir)
{
    m_paths = defaultPaths(overrideDataDir);
    if (!m_paths.ensureDirectories()) {
        setLastError(tr("Не удалось создать каталог данных: %1").arg(m_paths.dataDir));
        return false;
    }

    if (!m_database.open(m_paths.databaseFile)) {
        setLastError(tr("Не удалось открыть базу данных: %1").arg(m_database.lastError()));
        return false;
    }

    m_thumbnails = new ThumbnailGenerator(m_database, m_paths, this);
    m_scanner = new MediaScanner(m_database, *m_thumbnails, this);

    connect(m_thumbnails, &ThumbnailGenerator::progress, this, &MediaLibrary::onThumbnailProgress);
    connect(m_thumbnails, &ThumbnailGenerator::thumbnailReady, this, &MediaLibrary::onThumbnailReady);

    m_scanPool = new QThreadPool(this);
    m_scanPool->setMaxThreadCount(1); // сканирование не должно идти параллельно
    m_metadataPool = new QThreadPool(this);
    m_metadataPool->setMaxThreadCount(2);

    if (ffmpegPath().isEmpty()) {
        emit toast(tr("ffmpeg не найден — превью генерироваться не будут"), kToastError);
    }

    recount();
    bumpVersion();
    return true;
}

// ==== Состояние ====

bool MediaLibrary::isGeneratingThumbnails() const
{
    return m_thumbnails && !m_thumbnails->isIdle();
}

bool MediaLibrary::hasCatalogs() const
{
    return !m_database.catalogs().isEmpty();
}

int MediaLibrary::photoCount() const { return m_photoCount; }
int MediaLibrary::videoCount() const { return m_videoCount; }
int MediaLibrary::mediaCount() const { return m_mediaCount; }
int MediaLibrary::tagCount() const { return m_tagCount; }

qreal MediaLibrary::thumbnailProgress() const { return m_thumbnailProgress; }

void MediaLibrary::recount()
{
    m_photoCount = m_database.mediaCountByKind(MediaKind::Photo);
    m_videoCount = m_database.mediaCountByKind(MediaKind::Video);
    m_mediaCount = m_photoCount + m_videoCount;
    m_tagCount = m_database.tagCount();
}

void MediaLibrary::bumpVersion()
{
    ++m_version;
    recount();
    emit libraryChanged();
}

void MediaLibrary::setLastError(const QString &error)
{
    m_lastError = error;
    emit lastErrorChanged();
}

// ==== Каталоги ====

QVariantList MediaLibrary::catalogs() const
{
    QVariantList result;
    for (const Catalog &catalog : m_database.catalogs()) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), catalog.id);
        map.insert(QStringLiteral("path"), catalog.path);
        map.insert(QStringLiteral("name"), QDir(catalog.path).dirName());
        result.append(map);
    }
    return result;
}

QString MediaLibrary::defaultCatalogDirectory() const
{
    const QList<Catalog> existing = m_database.catalogs();
    if (!existing.isEmpty()) {
        return QFileInfo(existing.first().path).absolutePath();
    }
    return QDir::homePath();
}

void MediaLibrary::addCatalog(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    m_database.addCatalog(path);
    bumpVersion();
    startScanThread();
}

void MediaLibrary::removeCatalog(const QString &catalogId)
{
    const QList<MediaFile> media = m_database.mediaFilesByCatalog(catalogId);

    QSet<QString> paths;
    for (const MediaFile &file : media) {
        paths.insert(file.path);
    }
    m_thumbnails->removeThumbnailsForPaths(paths);

    m_database.removeCatalog(catalogId);
    bumpVersion();
}

void MediaLibrary::requestInitialScan()
{
    if (!hasCatalogs()) {
        return;
    }
    startScanThread();
}

void MediaLibrary::startScanThread()
{
    if (m_scanning) {
        // Сканирование уже идёт: по завершении запустим ещё одно,
        // чтобы учесть только что добавленные каталоги.
        m_scanQueued = true;
        return;
    }

    m_scanning = true;
    emit scanningChanged();

    QPointer<MediaLibrary> self(this);
    // Сканирование выполняется в отдельном потоке; результат возвращается
    // в главный поток через invokeMethod, чтобы не трогать базу из чужого потока.
    m_scanFuture = QtConcurrent::run(m_scanPool, [this, self]() {
        const ScanResult result = m_scanner->scan();
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(this, [this]() { runScan(); }, Qt::QueuedConnection);
    });
}

void MediaLibrary::runScan()
{
    m_scanning = false;
    emit scanningChanged();

    bumpVersion();

    if (m_scanQueued) {
        m_scanQueued = false;
        startScanThread();
    }
}

// ==== Медиа ====

QList<MediaFile> MediaLibrary::filteredMedia() const
{
    const QList<MediaFile> all = m_database.mediaFiles();
    if (m_filter.isEmpty()) {
        return sortedByDate(all);
    }

    // Обычные теги разрешаются через базу, метатеги вычисляются из свойств файла.
    QHash<QString, QSet<QString>> mediaIdsByTag;
    for (const QString &tagId : m_filter.tagIds) {
        if (metaTags::isMetaTagId(tagId)) {
            continue;
        }
        mediaIdsByTag.insert(tagId, m_database.mediaIdsByTag(tagId));
    }

    const QSet<QString> tagged = m_database.taggedMediaIds();

    QList<MediaFile> media;
    media.reserve(all.size());
    for (const MediaFile &file : all) {
        if (mediaMatchesFilter(file, m_filter, tagged, mediaIdsByTag)) {
            media.append(file);
        }
    }
    return sortedByDate(media);
}

QVariantMap MediaLibrary::mediaAt(int index) const
{
    const QList<MediaFile> media = filteredMedia();
    if (index < 0 || index >= media.size()) {
        return {};
    }
    return mediaToVariantMap(media.at(index));
}

QVariantMap MediaLibrary::mediaById(const QString &mediaId) const
{
    MediaFile media;
    if (!m_database.mediaFileById(mediaId, &media)) {
        return {};
    }
    return mediaToVariantMap(media);
}

qint64 MediaLibrary::knownCaptureDate(const QString &mediaId) const
{
    MediaFile media;
    if (m_database.mediaFileById(mediaId, &media) && media.hasCapturedAt) {
        return media.capturedAt;
    }
    return 0;
}

void MediaLibrary::requestCaptureDate(const QString &mediaId)
{
    MediaFile media;
    if (!m_database.mediaFileById(mediaId, &media)) {
        emit captureDateReady(mediaId, 0, false);
        return;
    }

    if (media.hasCapturedAt) {
        emit captureDateReady(mediaId, media.capturedAt, media.capturedAt > 0);
        return;
    }

    if (m_pendingCaptureDates.contains(mediaId)) {
        return; // запрос уже выполняется
    }
    m_pendingCaptureDates.insert(mediaId);

    const QString path = media.path;
    const MediaKind kind = media.kind;

    QPointer<MediaLibrary> self(this);
    QtConcurrent::run(m_metadataPool, [this, self, mediaId, path, kind]() {
        qint64 capturedAt = 0;
        const bool found = readCaptureDate(path, kind, &capturedAt);

        if (!self) {
            return;
        }

        // Запись в базу — только в главном потоке.
        QMetaObject::invokeMethod(this, [this, mediaId, capturedAt, found]() {
            m_pendingCaptureDates.remove(mediaId);
            if (found) {
                m_database.updateCapturedAt(mediaId, capturedAt);
            } else {
                // Помечаем «вычислялось и не найдено», чтобы не читать файл снова.
                m_database.markCapturedAtMissing(mediaId);
            }
            emit captureDateReady(mediaId, found ? capturedAt : 0, found);
        }, Qt::QueuedConnection);
    });
}

void MediaLibrary::showInFolder(const QString &filePath)
{
    // На macOS Finder выделяет файл, на Windows Проводник открывает папку.
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath).absoluteFilePath()));
}

// ==== Теги ====

QVariantList MediaLibrary::tags() const
{
    QVariantList result;
    for (const TagUsage &usage : m_database.tagUsages()) {
        result.append(tagToVariantMap(usage.tag, usage.count));
    }
    return result;
}

QVariantList MediaLibrary::metaTags() const
{
    QVariantList result;
    const QList<MediaFile> media = m_database.mediaFiles();
    const QSet<QString> tagged = m_database.taggedMediaIds();
    const int untagged = m_database.untaggedCount();

    for (const MetaTag &tag : metaTags::all(media)) {
        int count = 0;
        if (tag.id == metaTags::untaggedId()) {
            count = untagged;
        } else {
            for (const MediaFile &file : media) {
                if (metaTags::matches(file, tag.id)) {
                    ++count;
                }
            }
        }
        result.append(metaTagToVariantMap(tag, count));
    }
    Q_UNUSED(tagged);
    return result;
}

QVariantList MediaLibrary::mediaTags(const QString &mediaId) const
{
    QVariantList result;
    for (const Tag &tag : m_database.mediaTags(mediaId)) {
        result.append(tagToVariantMap(tag, 0));
    }
    return result;
}

QStringList MediaLibrary::mediaUserTagIds(const QString &mediaId) const
{
    QStringList ids;
    for (const Tag &tag : m_database.mediaTags(mediaId)) {
        ids.append(tag.id);
    }
    return ids;
}

QVariantList MediaLibrary::mediaMetaTags(const QString &mediaId) const
{
    QVariantList result;
    MediaFile media;
    if (!m_database.mediaFileById(mediaId, &media)) {
        return result;
    }
    for (const MetaTag &tag : metaTags::forFile(media)) {
        result.append(metaTagToVariantMap(tag, 0));
    }
    return result;
}

QVariantList MediaLibrary::searchTags(const QString &query) const
{
    QVariantList result;
    for (const TagUsage &usage : m_database.searchTags(query, 30)) {
        result.append(tagToVariantMap(usage.tag, usage.count));
    }
    return result;
}

QString MediaLibrary::createTag(const QString &name)
{
    const Tag tag = m_database.createTag(name);
    bumpVersion();
    return tag.id;
}

void MediaLibrary::deleteTag(const QString &tagId)
{
    m_database.deleteTag(tagId);
    m_filter.tagIds.removeAll(tagId);
    bumpVersion();
}

void MediaLibrary::applyTag(const QString &mediaId, const QString &tagId)
{
    m_database.applyTag(mediaId, tagId);
    bumpVersion();
}

void MediaLibrary::applyTagToMedia(const QStringList &mediaIds, const QString &tagId)
{
    m_database.applyTagToMedia(mediaIds, tagId);
    emit toast(tr("Тег применён к файлам: %1").arg(mediaIds.size()), kToastSuccess);
    bumpVersion();
}

void MediaLibrary::removeTagFromMedia(const QString &mediaId, const QString &tagId)
{
    m_database.removeTagFromMedia(mediaId, tagId);
    bumpVersion();
}

// ==== Фильтр ====

void MediaLibrary::setFilterTagIds(const QStringList &tagIds)
{
    m_filter.tagIds = tagIds;
    bumpVersion();
}

void MediaLibrary::setFilterMode(const QString &mode)
{
    m_filter.mode = (mode.compare(QLatin1String("AND"), Qt::CaseInsensitive) == 0)
        ? FilterMode::All
        : FilterMode::Any;
    bumpVersion();
}

QString MediaLibrary::filterMode() const
{
    return m_filter.mode == FilterMode::All ? QStringLiteral("AND") : QStringLiteral("OR");
}

void MediaLibrary::addTagToFilter(const QString &tagId)
{
    if (m_filter.tagIds.contains(tagId)) {
        return;
    }

    if (tagId == metaTags::untaggedId()) {
        // «Без тега» несовместим с обычными тегами: остаются только метатеги.
        QStringList metaOnly;
        for (const QString &existing : std::as_const(m_filter.tagIds)) {
            if (metaTags::isMetaTagId(existing)) {
                metaOnly.append(existing);
            }
        }
        metaOnly.append(tagId);
        m_filter.tagIds = metaOnly;
        bumpVersion();
        return;
    }

    QStringList updated;
    for (const QString &existing : std::as_const(m_filter.tagIds)) {
        if (existing != metaTags::untaggedId()) {
            updated.append(existing);
        }
    }
    updated.append(tagId);
    m_filter.tagIds = updated;
    bumpVersion();
}

void MediaLibrary::removeTagFromFilter(const QString &tagId)
{
    m_filter.tagIds.removeAll(tagId);
    bumpVersion();
}

void MediaLibrary::clearFilter()
{
    m_filter.tagIds.clear();
    bumpVersion();
}

QVariantMap MediaLibrary::metaTagById(const QString &metaTagId) const
{
    MetaTag tag;
    if (!metaTags::metaTagById(metaTagId, &tag)) {
        return {};
    }
    return metaTagToVariantMap(tag, 0);
}

QString MediaLibrary::tagName(const QString &tagId) const
{
    MetaTag meta;
    if (metaTags::metaTagById(tagId, &meta)) {
        return meta.name;
    }
    for (const Tag &tag : m_database.tags()) {
        if (tag.id == tagId) {
            return tag.name;
        }
    }
    return tagId;
}

// ==== Экспорт / импорт ====

bool MediaLibrary::transferData(bool exportMode, const QString &filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }

    QString error;
    if (exportMode) {
        if (!dataTransfer::exportToFile(m_database, filePath, &error)) {
            setLastError(error);
            emit toast(tr("Ошибка экспорта: %1").arg(error), kToastError);
            return false;
        }
        emit toast(tr("Данные экспортированы"), kToastSuccess);
        return true;
    }

    ImportSummary summary;
    if (!dataTransfer::parseFile(filePath, &summary, &error)) {
        setLastError(error);
        emit toast(tr("Ошибка импорта: %1").arg(error), kToastError);
        return false;
    }

    for (const QString &catalogPath : std::as_const(summary.catalogPaths)) {
        m_database.addCatalog(catalogPath);
    }

    for (const QString &tagName : std::as_const(summary.tagNames)) {
        m_database.createTag(tagName);
    }

    m_database.applyImportedRelations(summary);

    bumpVersion();
    emit toast(tr("Данные импортированы"), kToastSuccess);
    startScanThread();
    return true;
}

// ==== Сигналы превью ====

void MediaLibrary::onThumbnailProgress(const ThumbnailProgress &progress)
{
    m_thumbnailProgress = progress.total > 0
        ? static_cast<qreal>(progress.processed) / static_cast<qreal>(progress.total)
        : 0.0;
    emit thumbnailProgressChanged();
}

void MediaLibrary::onThumbnailReady(const QString &mediaId, const QString &thumbnailPath)
{
    emit thumbnailReady(mediaId, thumbnailPath);
}

} // namespace mc
