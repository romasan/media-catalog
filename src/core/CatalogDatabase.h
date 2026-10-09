#pragma once

#include "core/MediaTypes.h"

#include <QList>
#include <QPair>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

namespace mc {

/// Идентификаторы для восстановления связей тегов при импорте.
/// Отношения выражаются путями файлов, потому что id медиафайлов
/// после сканирования могут отличаться.
struct ImportSummary {
    QStringList catalogPaths;
    QStringList tagNames;
    /// Пары «путь файла» → «имя тега».
    QList<QPair<QString, QString>> mediaTagPaths;
};

/// Хранилище каталога на SQLite.
///
/// Схема (PRAGMA user_version = 1):
///   catalogs(id TEXT PK, path TEXT UNIQUE)
///   media(id TEXT PK, path TEXT UNIQUE, name, kind, size, created_at,
///         modified_at, catalog_id FK, thumbnail_path, thumbnail_retries,
///         captured_at NULL)
///   tags(id TEXT PK, name TEXT UNIQUE, last_used_at)
///   media_tags(media_id FK, tag_id FK, PK(media_id, tag_id))
///
/// Инварианты:
///   - имена тегов приводятся к нижнему регистру и обрезаются по краям;
///   - удаление каталога/файла/тега вычищает осиротевшие связи и теги;
///   - captured_at = NULL означает «дата съёмки ещё не вычислялась»,
///     что отличается от «вычислялась и не найдена» (0).
///
/// Класс не потокобезопасен: все обращения идут из потока владельца.
class CatalogDatabase
{
public:
    /// Схема, которую понимает эта версия приложения.
    static constexpr int kSchemaVersion = 1;

    CatalogDatabase();
    ~CatalogDatabase();

    CatalogDatabase(const CatalogDatabase &) = delete;
    CatalogDatabase &operator=(const CatalogDatabase &) = delete;

    /// Открывает (при необходимости создаёт) базу по указанному пути.
    /// Возвращает false и заполняет lastError() при неудаче.
    bool open(const QString &databaseFile);
    void close();

    QString lastError() const { return m_lastError; }

    // ==== Каталоги ====

    QList<Catalog> catalogs() const;
    /// Добавляет каталог по абсолютному пути. Повторное добавление
    /// возвращает существующую запись.
    Catalog addCatalog(const QString &path);
    void removeCatalog(const QString &catalogId);
    QList<CatalogStats> catalogStats() const;
    /// Дешёвые счётчики для интерфейса (COUNT в SQL, без выгрузки строк).
    int mediaCount() const;
    int mediaCountByKind(MediaKind kind) const;
    int tagCount() const;
    int untaggedCount() const;
    /// Пересчитывает привязку файлов к каталогам по порядку каталогов:
    /// файл принадлежит первому каталогу, который его содержит.
    /// Нужно, потому что один файл может лежать во вложенных каталогах.
    void rebuildAllPaths(const QStringList &orderedCatalogPaths);

    // ==== Медиафайлы ====

    QList<MediaFile> mediaFiles() const;
    QList<MediaFile> mediaFilesByCatalog(const QString &catalogId) const;
    bool mediaFileById(const QString &mediaId, MediaFile *out) const;
    QList<MediaFile> mediaFilesByPaths(const QSet<QString> &paths) const;
    /// Файлы, для которых превью ещё не записано и не исчерпан лимит попыток.
    QList<MediaFile> mediaWithoutThumbnail() const;
    /// Вставляет новые и обновляет изменившиеся файлы. Возвращает число
    /// действительно добавленных записей.
    int upsertMedia(const QList<MediaFile> &files);
    /// Удаляет файлы по путям вместе со связями тегов и осиротевшими тегами.
    /// Возвращает идентификаторы удалённых записей.
    QStringList removeMediaByPaths(const QSet<QString> &paths);
    bool updateThumbnail(const QString &mediaId, const QString &thumbnailPath);
    /// Фиксирует неудачную попытку генерации превью.
    /// Возвращает true, если файл ещё можно попробовать сгенерировать.
    bool incrementThumbnailRetries(const QString &mediaId);
    /// Сохраняет дату съёмки из метаданных файла.
    bool updateCapturedAt(const QString &mediaId, qint64 capturedAt);
    bool markCapturedAtMissing(const QString &mediaId);

    /// Отфильтрованные, отсортированные (сначала новые) и обрезанные записи.
    /// total получает полное число совпадений до применения limit/offset.
    QList<MediaFile> queryMedia(const FilterCondition &filter, int *total = nullptr,
                                int limit = -1, int offset = 0) const;

    // ==== Теги ====

    QList<Tag> tags() const;
    /// Теги с числом применений, отсортированные по последнему использованию
    /// (новые сверху), затем по имени.
    QList<TagUsage> tagUsages() const;
    QList<TagUsage> searchTags(const QString &query, int limit = 30) const;
    QList<Tag> mediaTags(const QString &mediaId) const;
    Tag createTag(const QString &name);
    bool findTagByName(const QString &name, Tag *out) const;
    void deleteTag(const QString &tagId);
    void applyTag(const QString &mediaId, const QString &tagId);
    void applyTagToMedia(const QStringList &mediaIds, const QString &tagId);
    void removeTagFromMedia(const QString &mediaId, const QString &tagId);
    QSet<QString> taggedMediaIds() const;
    QSet<QString> mediaIdsByTag(const QString &tagId) const;

    // ==== Экспорт / импорт ====

    /// Восстанавливает связи тегов, выраженные именами тегов и путями файлов.
    void applyImportedRelations(const ImportSummary &summary);

private:
    bool createSchema();
    /// Приводит имена тегов к нижнему регистру и склеивает дубликаты.
    void normalizeTagNames();
    void cleanupOrphanTags();
    void removeMediaTagsFor(const QStringList &mediaIds);
    bool exec(const QString &sql);

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_lastError;
};

} // namespace mc
