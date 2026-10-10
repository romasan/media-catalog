#include "core/CatalogDatabase.h"

#include "core/MetaTags.h"
#include "core/Paths.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlRecord>
#include <QThreadStorage>
#include <QUuid>
#include <QVariant>

#include <algorithm>

namespace mc {

namespace {

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

/// Абсолютный путь **без** разыменования символических ссылок.
///
/// Разыменование здесь недопустимо: имя файла превью — хэш пути, поэтому
/// разные формы одного пути (`/tmp/x` и `/private/tmp/x` на macOS дают разные
/// хэши) приводят к повторной генерации превью. Все места, работающие с путями
/// медиафайлов, используют одну и ту же форму.
QString normalizedPath(const QString &path)
{
    return QFileInfo(path).absoluteFilePath();
}

/// Qt биндит null-QString как SQL NULL, а QString по умолчанию — именно null.
/// Колонки, объявленные NOT NULL, должны получать пустую строку.
QVariant textValue(const QString &value)
{
    return QVariant(value.isNull() ? QString::fromLatin1("") : value);
}

QString normalizedTagName(const QString &name)
{
    return name.trimmed().toLower();
}

QString kindToString(MediaKind kind)
{
    return kind == MediaKind::Video ? QStringLiteral("video") : QStringLiteral("photo");
}

MediaKind kindFromString(const QString &value)
{
    return value == QLatin1String("video") ? MediaKind::Video : MediaKind::Photo;
}

/// Индекс колонки по имени — читаемость важнее скорости на этих объёмах.
int columnIndex(const QSqlQuery &query, const char *name)
{
    return query.record().indexOf(QLatin1String(name));
}

QString sqlInPlaceholders(int count)
{
    QStringList placeholders;
    placeholders.reserve(count);
    for (int i = 0; i < count; ++i) {
        placeholders.append(QStringLiteral("?"));
    }
    return placeholders.join(QLatin1Char(','));
}

} // namespace

namespace {

/// Хранилище соединений по потокам. Ключ — имя базового соединения, значение —
/// имя соединения этого потока.
QThreadStorage<QHash<QString, QString> *> &connectionNames()
{
    static QThreadStorage<QHash<QString, QString> *> storage;
    return storage;
}

QMutex &connectionsMutex()
{
    static QMutex mutex;
    return mutex;
}

} // namespace

CatalogDatabase::CatalogDatabase()
    : m_baseConnectionName(QStringLiteral("mc_%1").arg(reinterpret_cast<quintptr>(this), 0, 16))
{
}

QSqlDatabase CatalogDatabase::threadConnection()
{
    return const_cast<const CatalogDatabase *>(this)->threadConnection();
}

QSqlDatabase CatalogDatabase::threadConnection() const
{
    // Соединение для текущего потока, создаётся при первом обращении.
    QThreadStorage<QHash<QString, QString> *> &storage = connectionNames();
    if (!storage.hasLocalData()) {
        storage.setLocalData(new QHash<QString, QString>());
    }
    QHash<QString, QString> *names = storage.localData();

    QString connectionName = names->value(m_baseConnectionName);
    if (connectionName.isEmpty()) {
        connectionName = QStringLiteral("%1_t%2")
                             .arg(m_baseConnectionName)
                             .arg(reinterpret_cast<quintptr>(QThread::currentThread()), 0, 16);
        QMutexLocker locker(&connectionsMutex());
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(m_databaseFile);
        names->insert(m_baseConnectionName, connectionName);
    }

    QSqlDatabase db = QSqlDatabase::database(connectionName, false);

    // Соединение открывается при первом обращении из потока: у каждого потока
    // своё соединение к тому же файлу.
    if (db.isValid() && !db.isOpen() && !m_databaseFile.isEmpty()) {
        // Если соединение не открылось, ошибка доступна через db.lastError().
        db.open();
    }
    return db;
}

CatalogDatabase::~CatalogDatabase()
{
    close();
}

void CatalogDatabase::close()
{
    m_databaseFile.clear();

    // Закрываем соединение текущего потока; соединения других потоков
    // закрываются при их завершении (QThreadStorage освобождает хранилище).
    QThreadStorage<QHash<QString, QString> *> &storage = connectionNames();
    if (storage.hasLocalData()) {
        QHash<QString, QString> *names = storage.localData();
        const QString connectionName = names->take(m_baseConnectionName);
        if (!connectionName.isEmpty() && QSqlDatabase::contains(connectionName)) {
            {
                QSqlDatabase db = QSqlDatabase::database(connectionName, false);
                if (db.isOpen()) {
                    db.close();
                }
            }
            QMutexLocker locker(&connectionsMutex());
            QSqlDatabase::removeDatabase(connectionName);
        }
    }
}

bool CatalogDatabase::exec(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool CatalogDatabase::exec(const QString &sql)
{
    QSqlDatabase db = threadConnection();
    return exec(db, sql);
}

bool CatalogDatabase::open(const QString &databaseFile)
{
    close();

    const QFileInfo info(databaseFile);
    if (!QDir().mkpath(info.absolutePath())) {
        m_lastError = QStringLiteral("Не удалось создать каталог данных: %1").arg(info.absolutePath());
        return false;
    }

    m_databaseFile = databaseFile;

    QSqlDatabase db = threadConnection();
    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }

    // WAL даёт устойчивость к обрыву записи и параллельное чтение из других
    // соединений (по одному на поток). foreign_keys включаем явно — SQLite по
    // умолчанию их игнорирует.
    exec(db, QStringLiteral("PRAGMA journal_mode = WAL"));
    exec(db, QStringLiteral("PRAGMA synchronous = NORMAL"));
    exec(db, QStringLiteral("PRAGMA foreign_keys = ON"));

    if (!createSchema(db)) {
        return false;
    }

    normalizeTagNames();
    return true;
}

bool CatalogDatabase::createSchema(QSqlDatabase &db)
{
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS catalogs ("
                       "  id   TEXT PRIMARY KEY,"
                       "  path TEXT NOT NULL UNIQUE"
                       ")"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS media ("
                       "  id                TEXT PRIMARY KEY,"
                       "  path              TEXT NOT NULL UNIQUE,"
                       "  name              TEXT NOT NULL,"
                       "  kind              TEXT NOT NULL,"
                       "  size              INTEGER NOT NULL DEFAULT 0,"
                       "  created_at        INTEGER NOT NULL DEFAULT 0,"
                       "  modified_at       INTEGER NOT NULL DEFAULT 0,"
                       "  catalog_id        TEXT,"
                       "  thumbnail_path    TEXT NOT NULL DEFAULT '',"
                       "  thumbnail_retries INTEGER NOT NULL DEFAULT 0,"
                       "  captured_at       INTEGER"
                       ")"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_media_modified ON media(modified_at DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_media_catalog ON media(catalog_id)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS tags ("
                       "  id           TEXT PRIMARY KEY,"
                       "  name         TEXT NOT NULL UNIQUE,"
                       "  last_used_at INTEGER NOT NULL DEFAULT 0"
                       ")"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS media_tags ("
                       "  media_id TEXT NOT NULL REFERENCES media(id) ON DELETE CASCADE,"
                       "  tag_id   TEXT NOT NULL REFERENCES tags(id) ON DELETE CASCADE,"
                       "  PRIMARY KEY (media_id, tag_id)"
                       ")"),
    };

    for (const QString &statement : statements) {
        if (!exec(db, statement)) {
            return false;
        }
    }

    QSqlQuery versionQuery(db);
    if (versionQuery.exec(QStringLiteral("PRAGMA user_version")) && versionQuery.next()) {
        const int version = versionQuery.value(0).toInt();
        if (version > kSchemaVersion) {
            m_lastError = QStringLiteral("База создана более новой версией приложения (схема %1)").arg(version);
            return false;
        }
    }

    return exec(db, QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion));
}

// ==== Каталоги ====

QList<Catalog> CatalogDatabase::catalogs() const
{
    QList<Catalog> result;
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id, path FROM catalogs ORDER BY rowid"));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        Catalog catalog;
        catalog.id = query.value(0).toString();
        catalog.path = query.value(1).toString();
        result.append(catalog);
    }
    return result;
}

Catalog CatalogDatabase::addCatalog(const QString &path)
{
    const QString normalized = normalizedPath(path);

    QSqlDatabase db = threadConnection();
    QSqlQuery findQuery(db);
    findQuery.prepare(QStringLiteral("SELECT id, path FROM catalogs WHERE path = ?"));
    findQuery.addBindValue(textValue(normalized));
    if (findQuery.exec() && findQuery.next()) {
        return Catalog{findQuery.value(0).toString(), findQuery.value(1).toString()};
    }

    Catalog catalog{newId(), normalized};
    QSqlQuery insertQuery(threadConnection());
    insertQuery.prepare(QStringLiteral("INSERT INTO catalogs (id, path) VALUES (?, ?)"));
    insertQuery.addBindValue(textValue(catalog.id));
    insertQuery.addBindValue(textValue(catalog.path));
    if (!insertQuery.exec()) {
        m_lastError = insertQuery.lastError().text();
    }
    return catalog;
}

void CatalogDatabase::removeCatalog(const QString &catalogId)
{
    QStringList mediaIds;
    QSqlQuery idsQuery(threadConnection());
    idsQuery.prepare(QStringLiteral("SELECT id FROM media WHERE catalog_id = ?"));
    idsQuery.addBindValue(textValue(catalogId));
    if (idsQuery.exec()) {
        while (idsQuery.next()) {
            mediaIds.append(idsQuery.value(0).toString());
        }
    }

    removeMediaTagsFor(mediaIds);

    QSqlQuery deleteMedia(threadConnection());
    deleteMedia.prepare(QStringLiteral("DELETE FROM media WHERE catalog_id = ?"));
    deleteMedia.addBindValue(textValue(catalogId));
    deleteMedia.exec();

    QSqlQuery deleteCatalog(threadConnection());
    deleteCatalog.prepare(QStringLiteral("DELETE FROM catalogs WHERE id = ?"));
    deleteCatalog.addBindValue(textValue(catalogId));
    deleteCatalog.exec();

    cleanupOrphanTags();
}

QList<CatalogStats> CatalogDatabase::catalogStats() const
{
    QList<CatalogStats> result;
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT catalog_id,"
        "       SUM(CASE WHEN kind = 'photo' THEN 1 ELSE 0 END),"
        "       SUM(CASE WHEN kind = 'video' THEN 1 ELSE 0 END)"
        "  FROM media GROUP BY catalog_id"));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        CatalogStats stats;
        stats.catalogId = query.value(0).toString();
        stats.photoCount = query.value(1).toInt();
        stats.videoCount = query.value(2).toInt();
        result.append(stats);
    }
    return result;
}

int CatalogDatabase::mediaCount() const
{
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM media"));
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int CatalogDatabase::mediaCountByKind(MediaKind kind) const
{
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM media WHERE kind = ?"));
    query.addBindValue(textValue(kindToString(kind)));
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int CatalogDatabase::tagCount() const
{
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM tags"));
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int CatalogDatabase::untaggedCount() const
{
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM media m"
        " WHERE NOT EXISTS (SELECT 1 FROM media_tags mt WHERE mt.media_id = m.id)"));
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

void CatalogDatabase::rebuildAllPaths(const QStringList &orderedCatalogPaths)
{
    QList<Catalog> allCatalogs = catalogs();
    QList<MediaFile> allMedia = mediaFiles();

    QSqlQuery update(threadConnection());
    update.prepare(QStringLiteral("UPDATE media SET catalog_id = ? WHERE id = ?"));

    QSqlDatabase transactionDb = threadConnection();
    transactionDb.transaction();
    for (const MediaFile &media : std::as_const(allMedia)) {
        QString ownerId;
        for (const QString &catalogPath : orderedCatalogPaths) {
            if (media.path == catalogPath || media.path.startsWith(catalogPath + QLatin1Char('/'))) {
                for (const Catalog &catalog : std::as_const(allCatalogs)) {
                    if (catalog.path == catalogPath) {
                        ownerId = catalog.id;
                        break;
                    }
                }
                break;
            }
        }
        update.addBindValue(ownerId.isEmpty() ? QVariant() : textValue(ownerId));
        update.addBindValue(textValue(media.id));
        update.exec();
    }
    transactionDb.commit();
}

// ==== Медиафайлы ====

QList<MediaFile> CatalogDatabase::mediaFiles() const
{
    QList<MediaFile> result;
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, path, name, kind, size, created_at, modified_at, catalog_id,"
        "       thumbnail_path, thumbnail_retries, captured_at"
        "  FROM media"));
    if (!query.exec()) {
        return result;
    }

    const int capturedIdx = columnIndex(query, "captured_at");
    while (query.next()) {
        MediaFile media;
        media.id = query.value(0).toString();
        media.path = query.value(1).toString();
        media.name = query.value(2).toString();
        media.kind = kindFromString(query.value(3).toString());
        media.size = query.value(4).toLongLong();
        media.createdAt = query.value(5).toLongLong();
        media.modifiedAt = query.value(6).toLongLong();
        media.catalogId = query.value(7).toString();
        media.thumbnailPath = query.value(8).toString();
        media.thumbnailRetries = query.value(9).toInt();
        const QVariant captured = query.value(capturedIdx);
        media.hasCapturedAt = !captured.isNull();
        media.capturedAt = captured.toLongLong();
        result.append(media);
    }
    return result;
}

QList<MediaFile> CatalogDatabase::mediaFilesByCatalog(const QString &catalogId) const
{
    QList<MediaFile> result;
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, path, name, kind, size, created_at, modified_at, catalog_id,"
        "       thumbnail_path, thumbnail_retries, captured_at"
        "  FROM media WHERE catalog_id = ?"));
    query.addBindValue(textValue(catalogId));
    if (!query.exec()) {
        return result;
    }

    const int capturedIdx = columnIndex(query, "captured_at");
    while (query.next()) {
        MediaFile media;
        media.id = query.value(0).toString();
        media.path = query.value(1).toString();
        media.name = query.value(2).toString();
        media.kind = kindFromString(query.value(3).toString());
        media.size = query.value(4).toLongLong();
        media.createdAt = query.value(5).toLongLong();
        media.modifiedAt = query.value(6).toLongLong();
        media.catalogId = query.value(7).toString();
        media.thumbnailPath = query.value(8).toString();
        media.thumbnailRetries = query.value(9).toInt();
        const QVariant captured = query.value(capturedIdx);
        media.hasCapturedAt = !captured.isNull();
        media.capturedAt = captured.toLongLong();
        result.append(media);
    }
    return result;
}

bool CatalogDatabase::mediaFileById(const QString &mediaId, MediaFile *out) const
{
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, path, name, kind, size, created_at, modified_at, catalog_id,"
        "       thumbnail_path, thumbnail_retries, captured_at"
        "  FROM media WHERE id = ?"));
    query.addBindValue(textValue(mediaId));
    if (!query.exec() || !query.next()) {
        return false;
    }

    const int capturedIdx = columnIndex(query, "captured_at");
    if (out) {
        out->id = query.value(0).toString();
        out->path = query.value(1).toString();
        out->name = query.value(2).toString();
        out->kind = kindFromString(query.value(3).toString());
        out->size = query.value(4).toLongLong();
        out->createdAt = query.value(5).toLongLong();
        out->modifiedAt = query.value(6).toLongLong();
        out->catalogId = query.value(7).toString();
        out->thumbnailPath = query.value(8).toString();
        out->thumbnailRetries = query.value(9).toInt();
        const QVariant captured = query.value(capturedIdx);
        out->hasCapturedAt = !captured.isNull();
        out->capturedAt = captured.toLongLong();
    }
    return true;
}

QList<MediaFile> CatalogDatabase::mediaFilesByPaths(const QSet<QString> &paths) const
{
    QList<MediaFile> result;
    if (paths.isEmpty()) {
        return result;
    }

    const QStringList pathList = paths.values();
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
                      "SELECT id, path, name, kind, size, created_at, modified_at, catalog_id,"
                      "       thumbnail_path, thumbnail_retries, captured_at"
                      "  FROM media WHERE path IN (%1)")
                      .arg(sqlInPlaceholders(pathList.size())));
    for (const QString &path : pathList) {
        query.addBindValue(textValue(path));
    }
    if (!query.exec()) {
        return result;
    }

    const int capturedIdx = columnIndex(query, "captured_at");
    while (query.next()) {
        MediaFile media;
        media.id = query.value(0).toString();
        media.path = query.value(1).toString();
        media.name = query.value(2).toString();
        media.kind = kindFromString(query.value(3).toString());
        media.size = query.value(4).toLongLong();
        media.createdAt = query.value(5).toLongLong();
        media.modifiedAt = query.value(6).toLongLong();
        media.catalogId = query.value(7).toString();
        media.thumbnailPath = query.value(8).toString();
        media.thumbnailRetries = query.value(9).toInt();
        const QVariant captured = query.value(capturedIdx);
        media.hasCapturedAt = !captured.isNull();
        media.capturedAt = captured.toLongLong();
        result.append(media);
    }
    return result;
}

QList<MediaFile> CatalogDatabase::mediaWithoutThumbnail() const
{
    QList<MediaFile> result;
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, path, name, kind, size, created_at, modified_at, catalog_id,"
        "       thumbnail_path, thumbnail_retries, captured_at"
        "  FROM media WHERE thumbnail_path = '' AND thumbnail_retries <= 1"));
    if (!query.exec()) {
        return result;
    }

    const int capturedIdx = columnIndex(query, "captured_at");
    while (query.next()) {
        MediaFile media;
        media.id = query.value(0).toString();
        media.path = query.value(1).toString();
        media.name = query.value(2).toString();
        media.kind = kindFromString(query.value(3).toString());
        media.size = query.value(4).toLongLong();
        media.createdAt = query.value(5).toLongLong();
        media.modifiedAt = query.value(6).toLongLong();
        media.catalogId = query.value(7).toString();
        media.thumbnailPath = query.value(8).toString();
        media.thumbnailRetries = query.value(9).toInt();
        const QVariant captured = query.value(capturedIdx);
        media.hasCapturedAt = !captured.isNull();
        media.capturedAt = captured.toLongLong();
        result.append(media);
    }
    return result;
}

int CatalogDatabase::upsertMedia(const QList<MediaFile> &files)
{
    if (files.isEmpty()) {
        return 0;
    }

    int added = 0;
    QSqlDatabase transactionDb = threadConnection();
    const bool inTransaction = transactionDb.transaction();

    for (const MediaFile &media : files) {
        const QString path = normalizedPath(media.path);
        const QString name = media.name.isEmpty() ? QFileInfo(path).fileName() : media.name;
        const QString catalogId = media.catalogId;

        // Проверяем наличие записи отдельным запросом: путь — уникальный ключ,
        // а id нового файла генерируется здесь.
        bool exists = false;
        {
            QSqlQuery existsQuery(threadConnection());
            existsQuery.prepare(QStringLiteral("SELECT 1 FROM media WHERE path = ?"));
            existsQuery.addBindValue(textValue(path));
            if (!existsQuery.exec()) {
                m_lastError = existsQuery.lastError().text();
            } else if (existsQuery.next()) {
                exists = true;
            }
        }

        if (exists) {
            QSqlQuery updateQuery(threadConnection());
            updateQuery.prepare(QStringLiteral(
                "UPDATE media SET name = ?, kind = ?, size = ?, created_at = ?, modified_at = ?,"
                "                 catalog_id = ?"
                " WHERE path = ?"));
            updateQuery.addBindValue(textValue(name));
            updateQuery.addBindValue(textValue(kindToString(media.kind)));
            updateQuery.addBindValue(media.size);
            updateQuery.addBindValue(media.createdAt);
            updateQuery.addBindValue(media.modifiedAt);
            updateQuery.addBindValue(textValue(catalogId));
            updateQuery.addBindValue(textValue(path));
            if (!updateQuery.exec()) {
                m_lastError = updateQuery.lastError().text();
            }
            continue;
        }

        QSqlQuery insertQuery(threadConnection());
        insertQuery.prepare(QStringLiteral(
            "INSERT INTO media (id, path, name, kind, size, created_at, modified_at, catalog_id,"
            "                   thumbnail_path, thumbnail_retries, captured_at)"
            " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        insertQuery.addBindValue(textValue(media.id.isEmpty() ? newId() : media.id));
        insertQuery.addBindValue(textValue(path));
        insertQuery.addBindValue(textValue(name));
        insertQuery.addBindValue(textValue(kindToString(media.kind)));
        insertQuery.addBindValue(media.size);
        insertQuery.addBindValue(media.createdAt);
        insertQuery.addBindValue(media.modifiedAt);
        insertQuery.addBindValue(textValue(catalogId));
        insertQuery.addBindValue(textValue(media.thumbnailPath));
        insertQuery.addBindValue(media.thumbnailRetries);
        // NULL означает «дата съёмки ещё не вычислялась» — это значимое состояние.
        if (media.hasCapturedAt) {
            insertQuery.addBindValue(media.capturedAt);
        } else {
            insertQuery.addBindValue(QVariant());
        }

        if (insertQuery.exec()) {
            ++added;
        } else {
            m_lastError = insertQuery.lastError().text();

        }
    }

    if (inTransaction && !threadConnection().commit()) {
        m_lastError = transactionDb.lastError().text();
    }
    return added;
}

QStringList CatalogDatabase::removeMediaByPaths(const QSet<QString> &paths)
{
    QStringList removedIds;
    if (paths.isEmpty()) {
        return removedIds;
    }

    QSqlQuery idsQuery(threadConnection());
    idsQuery.prepare(QStringLiteral("SELECT id FROM media WHERE path IN (%1)")
                         .arg(sqlInPlaceholders(paths.size())));
    for (const QString &path : paths) {
        idsQuery.addBindValue(textValue(path));
    }
    if (idsQuery.exec()) {
        while (idsQuery.next()) {
            removedIds.append(idsQuery.value(0).toString());
        }
    }

    removeMediaTagsFor(removedIds);

    QSqlQuery deleteQuery(threadConnection());
    deleteQuery.prepare(QStringLiteral("DELETE FROM media WHERE path IN (%1)")
                            .arg(sqlInPlaceholders(paths.size())));
    for (const QString &path : paths) {
        deleteQuery.addBindValue(textValue(path));
    }
    deleteQuery.exec();

    cleanupOrphanTags();
    return removedIds;
}

bool CatalogDatabase::updateThumbnail(const QString &mediaId, const QString &thumbnailPath)
{
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("UPDATE media SET thumbnail_path = ? WHERE id = ?"));
    query.addBindValue(textValue(thumbnailPath));
    query.addBindValue(textValue(mediaId));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool CatalogDatabase::incrementThumbnailRetries(const QString &mediaId)
{
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE media SET thumbnail_retries = thumbnail_retries + 1 WHERE id = ?"));
    query.addBindValue(textValue(mediaId));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    QSqlQuery readQuery(threadConnection());
    readQuery.prepare(QStringLiteral("SELECT thumbnail_retries FROM media WHERE id = ?"));
    readQuery.addBindValue(textValue(mediaId));
    if (readQuery.exec() && readQuery.next()) {
        return readQuery.value(0).toInt() <= 1;
    }
    return false;
}

bool CatalogDatabase::updateCapturedAt(const QString &mediaId, qint64 capturedAt)
{
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("UPDATE media SET captured_at = ? WHERE id = ?"));
    query.addBindValue(capturedAt);
    query.addBindValue(textValue(mediaId));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool CatalogDatabase::markCapturedAtMissing(const QString &mediaId)
{
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("UPDATE media SET captured_at = 0 WHERE id = ?"));
    query.addBindValue(textValue(mediaId));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

// ==== Теги ====

QList<Tag> CatalogDatabase::tags() const
{
    QList<Tag> result;
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id, name, last_used_at FROM tags ORDER BY name"));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        Tag tag;
        tag.id = query.value(0).toString();
        tag.name = query.value(1).toString();
        tag.lastUsedAt = query.value(2).toLongLong();
        result.append(tag);
    }
    return result;
}

QList<TagUsage> CatalogDatabase::tagUsages() const
{
    QList<TagUsage> result;
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT t.id, t.name, t.last_used_at, COUNT(mt.media_id)"
        "  FROM tags t LEFT JOIN media_tags mt ON mt.tag_id = t.id"
        " GROUP BY t.id, t.name, t.last_used_at"));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        TagUsage usage;
        usage.tag.id = query.value(0).toString();
        usage.tag.name = query.value(1).toString();
        usage.tag.lastUsedAt = query.value(2).toLongLong();
        usage.count = query.value(3).toInt();
        result.append(usage);
    }

    // Сначала недавно использованные, затем по имени.
    std::sort(result.begin(), result.end(), [](const TagUsage &a, const TagUsage &b) {
        if (a.tag.lastUsedAt != b.tag.lastUsedAt) {
            return a.tag.lastUsedAt > b.tag.lastUsedAt;
        }
        return a.tag.name.localeAwareCompare(b.tag.name) < 0;
    });
    return result;
}

QList<TagUsage> CatalogDatabase::searchTags(const QString &query, int limit) const
{
    const QString normalized = query.trimmed().toLower();

    QList<TagUsage> result;
    for (const TagUsage &usage : tagUsages()) {
        if (normalized.isEmpty() || usage.tag.name.contains(normalized)) {
            result.append(usage);
        }
        if (limit >= 0 && result.size() >= limit) {
            break;
        }
    }
    return result;
}

QList<Tag> CatalogDatabase::mediaTags(const QString &mediaId) const
{
    QList<Tag> result;
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT t.id, t.name, t.last_used_at"
        "  FROM tags t JOIN media_tags mt ON mt.tag_id = t.id"
        " WHERE mt.media_id = ? ORDER BY t.name"));
    query.addBindValue(textValue(mediaId));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        Tag tag;
        tag.id = query.value(0).toString();
        tag.name = query.value(1).toString();
        tag.lastUsedAt = query.value(2).toLongLong();
        result.append(tag);
    }
    return result;
}

Tag CatalogDatabase::createTag(const QString &name)
{
    const QString normalized = normalizedTagName(name);

    Tag existing;
    if (findTagByName(normalized, &existing)) {
        return existing;
    }

    Tag tag{newId(), normalized, 0};
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT INTO tags (id, name, last_used_at) VALUES (?, ?, 0)"));
    query.addBindValue(textValue(tag.id));
    query.addBindValue(textValue(tag.name));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
    }
    return tag;
}

bool CatalogDatabase::findTagByName(const QString &name, Tag *out) const
{
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id, name, last_used_at FROM tags WHERE name = ?"));
    query.addBindValue(textValue(normalizedTagName(name)));
    if (!query.exec() || !query.next()) {
        return false;
    }
    if (out) {
        out->id = query.value(0).toString();
        out->name = query.value(1).toString();
        out->lastUsedAt = query.value(2).toLongLong();
    }
    return true;
}

void CatalogDatabase::deleteTag(const QString &tagId)
{
    QSqlQuery deleteLinks(threadConnection());
    deleteLinks.prepare(QStringLiteral("DELETE FROM media_tags WHERE tag_id = ?"));
    deleteLinks.addBindValue(textValue(tagId));
    deleteLinks.exec();

    QSqlQuery deleteTag(threadConnection());
    deleteTag.prepare(QStringLiteral("DELETE FROM tags WHERE id = ?"));
    deleteTag.addBindValue(textValue(tagId));
    deleteTag.exec();
}

void CatalogDatabase::applyTag(const QString &mediaId, const QString &tagId)
{
    applyTagToMedia({mediaId}, tagId);
}

void CatalogDatabase::applyTagToMedia(const QStringList &mediaIds, const QString &tagId)
{
    if (mediaIds.isEmpty()) {
        return;
    }

    QSqlDatabase transactionDb = threadConnection();
    transactionDb.transaction();
    QSqlQuery insert(threadConnection());
    insert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO media_tags (media_id, tag_id) VALUES (?, ?)"));
    for (const QString &mediaId : mediaIds) {
        insert.addBindValue(textValue(mediaId));
        insert.addBindValue(textValue(tagId));
        insert.exec();
    }

    QSqlQuery touch(threadConnection());
    touch.prepare(QStringLiteral("UPDATE tags SET last_used_at = ? WHERE id = ?"));
    touch.addBindValue(QDateTime::currentMSecsSinceEpoch());
    touch.addBindValue(textValue(tagId));
    touch.exec();
    transactionDb.commit();
}

void CatalogDatabase::removeTagFromMedia(const QString &mediaId, const QString &tagId)
{
    QSqlQuery deleteLink(threadConnection());
    deleteLink.prepare(QStringLiteral("DELETE FROM media_tags WHERE media_id = ? AND tag_id = ?"));
    deleteLink.addBindValue(textValue(mediaId));
    deleteLink.addBindValue(textValue(tagId));
    deleteLink.exec();

    // Тег больше не используется — сбрасываем время последнего применения,
    // но сам тег сохраняем.
    QSqlQuery countQuery(threadConnection());
    countQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM media_tags WHERE tag_id = ?"));
    countQuery.addBindValue(textValue(tagId));
    if (countQuery.exec() && countQuery.next() && countQuery.value(0).toInt() == 0) {
        QSqlQuery reset(threadConnection());
        reset.prepare(QStringLiteral("UPDATE tags SET last_used_at = 0 WHERE id = ? AND last_used_at > 0"));
        reset.addBindValue(textValue(tagId));
        reset.exec();
    }
}

QSet<QString> CatalogDatabase::taggedMediaIds() const
{
    QSet<QString> result;
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT DISTINCT media_id FROM media_tags"));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        result.insert(query.value(0).toString());
    }
    return result;
}

QSet<QString> CatalogDatabase::mediaIdsByTag(const QString &tagId) const
{
    QSet<QString> result;
    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT media_id FROM media_tags WHERE tag_id = ?"));
    query.addBindValue(textValue(tagId));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        result.insert(query.value(0).toString());
    }
    return result;
}

// ==== Экспорт / импорт ====

void CatalogDatabase::applyImportedRelations(const ImportSummary &summary)
{
    // Связи приходят путями файлов: после сканирования идентификаторы
    // медиафайлов могли измениться.
    QSqlDatabase transactionDb = threadConnection();
    transactionDb.transaction();
    for (const auto &[mediaPath, tagName] : summary.mediaTagPaths) {
        Tag tag;
        if (!findTagByName(tagName, &tag)) {
            continue;
        }

        QSqlQuery mediaQuery(threadConnection());
        mediaQuery.prepare(QStringLiteral("SELECT id FROM media WHERE path = ?"));
        mediaQuery.addBindValue(textValue(normalizedPath(mediaPath)));
        if (!mediaQuery.exec() || !mediaQuery.next()) {
            continue;
        }

        QSqlQuery link(threadConnection());
        link.prepare(QStringLiteral("INSERT OR IGNORE INTO media_tags (media_id, tag_id) VALUES (?, ?)"));
        link.addBindValue(textValue(mediaQuery.value(0).toString()));
        link.addBindValue(textValue(tag.id));
        link.exec();
    }
    transactionDb.commit();

    cleanupOrphanTags();
}

// ==== Вспомогательные ====

void CatalogDatabase::removeMediaTagsFor(const QStringList &mediaIds)
{
    if (mediaIds.isEmpty()) {
        return;
    }

    QSqlDatabase db = threadConnection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM media_tags WHERE media_id IN (%1)")
                      .arg(sqlInPlaceholders(mediaIds.size())));
    for (const QString &mediaId : mediaIds) {
        query.addBindValue(textValue(mediaId));
    }
    query.exec();
}

void CatalogDatabase::cleanupOrphanTags()
{
    // Тег без связей удаляем, если он уже использовался (last_used_at > 0).
    // Теги, созданные пользователем и пока не применённые, сохраняем.
    exec(QStringLiteral(
        "DELETE FROM tags WHERE last_used_at > 0"
        "  AND NOT EXISTS (SELECT 1 FROM media_tags WHERE media_tags.tag_id = tags.id)"));
}

void CatalogDatabase::normalizeTagNames()
{
    QList<Tag> allTags = tags();
    if (allTags.isEmpty()) {
        return;
    }

    // Группируем теги по нормализованному имени, сохраняя самый «свежий»
    // lastUsedAt и перенаправляя связи на выживший тег.
    QHash<QString, Tag> byName;
    QList<QPair<QString, QString>> redirects; // дубликат → выживший

    for (const Tag &tag : std::as_const(allTags)) {
        const QString normalized = normalizedTagName(tag.name);
        if (normalized.isEmpty()) {
            continue;
        }

        auto it = byName.find(normalized);
        if (it == byName.end()) {
            Tag survivor = tag;
            survivor.name = normalized;
            byName.insert(normalized, survivor);
            if (survivor.name != tag.name) {
                redirects.append({tag.id, survivor.id});
            }
        } else {
            if (tag.lastUsedAt > it->lastUsedAt) {
                it->lastUsedAt = tag.lastUsedAt;
            }
            redirects.append({tag.id, it->id});
        }
    }

    if (redirects.isEmpty()) {
        return;
    }

    QSqlDatabase transactionDb = threadConnection();
    transactionDb.transaction();
    for (const auto &[duplicateId, survivorId] : redirects) {
        QSqlQuery redirect(threadConnection());
        // OR IGNORE: связь могла уже существовать у выжившего тега.
        redirect.prepare(QStringLiteral("UPDATE OR IGNORE media_tags SET tag_id = ? WHERE tag_id = ?"));
        redirect.addBindValue(textValue(survivorId));
        redirect.addBindValue(textValue(duplicateId));
        redirect.exec();

        QSqlQuery dropLinks(threadConnection());
        dropLinks.prepare(QStringLiteral("DELETE FROM media_tags WHERE tag_id = ?"));
        dropLinks.addBindValue(textValue(duplicateId));
        dropLinks.exec();

        QSqlQuery dropTag(threadConnection());
        dropTag.prepare(QStringLiteral("DELETE FROM tags WHERE id = ?"));
        dropTag.addBindValue(textValue(duplicateId));
        dropTag.exec();
    }

    // Фиксируем объединённые lastUsedAt и нормализованные имена.
    for (auto it = byName.cbegin(); it != byName.cend(); ++it) {
        QSqlQuery update(threadConnection());
        update.prepare(QStringLiteral("UPDATE tags SET name = ?, last_used_at = ? WHERE id = ?"));
        update.addBindValue(textValue(it->name));
        update.addBindValue(it->lastUsedAt);
        update.addBindValue(textValue(it->id));
        update.exec();
    }
    transactionDb.commit();
}

} // namespace mc
