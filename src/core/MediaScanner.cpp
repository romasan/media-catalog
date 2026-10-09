#include "core/MediaScanner.h"

#include "core/CatalogDatabase.h"
#include "core/MediaFormats.h"
#include "core/ThumbnailGenerator.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace mc {

namespace {

/// Папки, которые не нужно обходить: скрытые и системные.
bool isSkippedDirectory(const QString &name)
{
    return name.startsWith(QLatin1Char('.'))
        || name == QLatin1String("@eaDir")
        || name == QLatin1String("System Volume Information");
}

/// Дата создания файла, с откатом на дату изменения, если birthtime недоступен
/// (так ведёт себя и Electron-версия). Возвращает мс с начала эпохи.
qint64 creationDateMs(const QFileInfo &info)
{
    const QDateTime birth = info.birthTime();
    if (birth.isValid() && birth.toMSecsSinceEpoch() > 0) {
        return birth.toMSecsSinceEpoch();
    }
    return info.lastModified().toMSecsSinceEpoch();
}

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

/// Рекурсивный обход каталога с пропуском скрытых и системных папок.
/// QDirIterator здесь не подходит: он не позволяет пропустить поддерево
/// целиком, а обходить служебные папки (`.git`, `@eaDir`) не нужно.
void walkDirectory(const QString &root, const Catalog &catalog, const QStringList &catalogPaths,
                   const QList<Catalog> &catalogs, QHash<QString, QString> &foundFiles)
{
    QDir dir(root);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                                    QDir::DirsFirst | QDir::Name);

    for (const QFileInfo &info : entries) {
        if (info.isDir()) {
            if (isSkippedDirectory(info.fileName())) {
                continue;
            }
            walkDirectory(info.absoluteFilePath(), catalog, catalogPaths, catalogs, foundFiles);
            continue;
        }

        if (!info.isFile() || !isSupportedMedia(info.fileName())) {
            continue;
        }

        const QString path = info.absoluteFilePath();

        // Каталог-владелец: первый (самый глубокий) из отсортированного списка.
        QString ownerId = catalog.id;
        for (const QString &candidate : catalogPaths) {
            if (path == candidate || path.startsWith(candidate + QLatin1Char('/'))) {
                for (const Catalog &entry : catalogs) {
                    if (entry.path == candidate) {
                        ownerId = entry.id;
                        break;
                    }
                }
                break;
            }
        }

        if (!foundFiles.contains(path)) {
            foundFiles.insert(path, ownerId);
        }
    }
}

} // namespace

MediaScanner::MediaScanner(CatalogDatabase &database, ThumbnailGenerator &thumbnails, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_thumbnails(thumbnails)
{
}

ScanResult MediaScanner::scan()
{
    ScanResult result;

    QList<Catalog> catalogs = m_database.catalogs();
    if (catalogs.isEmpty()) {
        return result;
    }

    // Файл принадлежит самому «глубокому» каталогу, который его содержит:
    // так вложенные каталоги не создают дубликатов.
    QStringList catalogPaths;
    catalogPaths.reserve(catalogs.size());
    for (const Catalog &catalog : std::as_const(catalogs)) {
        catalogPaths.append(catalog.path);
    }
    std::sort(catalogPaths.begin(), catalogPaths.end(), [](const QString &a, const QString &b) {
        return a.size() > b.size();
    });

    // ==== Обход файловой системы ====
    // path → каталог-владелец
    QHash<QString, QString> foundFiles;
    QSet<qint64> scannedInodes; // защита от циклов по символическим ссылкам

    int processedCatalogs = 0;
    for (const Catalog &catalog : std::as_const(catalogs)) {
        const QFileInfo rootInfo(catalog.path);
        if (!rootInfo.exists() || !rootInfo.isDir()) {
            ++processedCatalogs;
            emit progress(processedCatalogs, catalogs.size());
            continue;
        }

        walkDirectory(catalog.path, catalog, catalogPaths, catalogs, foundFiles);

        ++processedCatalogs;
        emit progress(processedCatalogs, catalogs.size());
    }

    // ==== Синхронизация с базой ====
    const QList<MediaFile> existing = m_database.mediaFiles();
    QHash<QString, MediaFile> existingByPath;
    existingByPath.reserve(existing.size());
    for (const MediaFile &media : existing) {
        existingByPath.insert(media.path, media);
    }

    QList<MediaFile> toUpsert;
    toUpsert.reserve(foundFiles.size());

    for (auto it = foundFiles.cbegin(); it != foundFiles.cend(); ++it) {
        const QString &path = it.key();
        const QString &ownerId = it.value();

        const QFileInfo info(path);
        const qint64 size = info.size();
        const qint64 modifiedAt = info.lastModified().toMSecsSinceEpoch();

        auto existingIt = existingByPath.find(path);
        if (existingIt == existingByPath.end()) {
            MediaFile media;
            media.id = newId();
            media.path = path;
            media.name = info.fileName();
            media.kind = mediaKindForFile(path);
            media.size = size;
            media.createdAt = creationDateMs(info);
            media.modifiedAt = modifiedAt;
            media.catalogId = ownerId;
            toUpsert.append(media);
            result.addedMedia.append(path);
            continue;
        }

        // Файл уже в базе: пишем только реальные изменения.
        MediaFile media = existingIt.value();
        if (media.modifiedAt != modifiedAt || media.size != size || media.catalogId != ownerId) {
            media.size = size;
            media.modifiedAt = modifiedAt;
            media.createdAt = creationDateMs(info);
            media.catalogId = ownerId;
            toUpsert.append(media);
        }
    }

    // ==== Удалённые файлы ====
    QSet<QString> removedPaths;
    for (const MediaFile &media : existing) {
        if (!foundFiles.contains(media.path)) {
            removedPaths.insert(media.path);
        }
    }

    int addedCount = 0;
    if (!toUpsert.isEmpty()) {
        addedCount = m_database.upsertMedia(toUpsert);
    }

    int removedCount = 0;
    if (!removedPaths.isEmpty()) {
        // Превью удалённых файлов больше не нужны.
        m_thumbnails.removeThumbnailsForPaths(removedPaths);
        removedCount = m_database.removeMediaByPaths(removedPaths).size();
        for (const QString &path : std::as_const(removedPaths)) {
            result.removedMedia.append(path);
        }
    }

    // ==== Очередь превью ====
    // Новые файлы плюс те, что остались без превью после прошлого запуска
    // (приложение могли закрыть во время генерации).
    QList<MediaFile> needThumbnails;
    for (const MediaFile &media : std::as_const(toUpsert)) {
        if (media.thumbnailPath.isEmpty()) {
            needThumbnails.append(media);
        }
    }
    needThumbnails.append(m_database.mediaWithoutThumbnail());
    if (!needThumbnails.isEmpty()) {
        m_thumbnails.enqueue(needThumbnails);
    }

    result.addedFiles = result.addedMedia.size();
    result.removedFiles = removedCount;
    return result;
}

} // namespace mc
