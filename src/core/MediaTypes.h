#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <cstdint>

namespace mc {

/// Тип медиафайла.
enum class MediaKind : std::uint8_t {
    Photo,
    Video,
};

/// Каталог — папка, добавленная пользователем для рекурсивного сканирования.
struct Catalog {
    QString id;
    QString path;
};

/// Медиафайл, проиндексированный сканером.
/// Дата съёмки опциональна, поэтому наличие значения выражено отдельным флагом:
/// «не вычислено» и «значение 0» — разные состояния.
struct MediaFile {
    QString id;
    QString path;
    QString name;
    MediaKind kind = MediaKind::Photo;
    qint64 size = 0;
    qint64 createdAt = 0;   ///< дата создания файла (birthtime), мс
    qint64 modifiedAt = 0;  ///< дата изменения файла (mtime), мс
    QString catalogId;
    QString thumbnailPath;  ///< пусто, пока превью не сгенерировано
    int thumbnailRetries = 0;
    qint64 capturedAt = 0;  ///< дата съёмки из метаданных (EXIF/creation_time), мс
    bool hasCapturedAt = false;
};

/// Пользовательский тег. Имя всегда хранится в нижнем регистре.
struct Tag {
    QString id;
    QString name;
    qint64 lastUsedAt = 0;
};

struct TagUsage {
    Tag tag;
    int count = 0;
};

/// Виды автоматических метатегов.
enum class MetaTagKind : std::uint8_t {
    Year,
    Season,
    Type,
    Untagged,
};

struct MetaTag {
    QString id;
    QString name;
    MetaTagKind kind = MetaTagKind::Year;
    QString group;

    bool operator==(const MetaTag &other) const { return id == other.id; }
};

struct MetaTagUsage {
    MetaTag metaTag;
    int count = 0;
};

struct CatalogStats {
    QString catalogId;
    int photoCount = 0;
    int videoCount = 0;
};

/// Результат одного прохода сканирования.
struct ScanResult {
    int addedFiles = 0;
    int removedFiles = 0;
    int addedFolders = 0;
    int removedFolders = 0;
    QStringList addedMedia;
    QStringList removedMedia;

    bool isEmpty() const
    {
        return addedFiles == 0 && removedFiles == 0
            && addedFolders == 0 && removedFolders == 0;
    }
};

struct ThumbnailProgress {
    int total = 0;
    int processed = 0;
};

/// Режим фильтрации по нескольким тегам.
enum class FilterMode : std::uint8_t {
    All,   ///< И (все выбранные теги)
    Any,   ///< ИЛИ (любой из выбранных тегов)
};

struct FilterCondition {
    QStringList tagIds;
    FilterMode mode = FilterMode::Any;

    bool isEmpty() const { return tagIds.isEmpty(); }
};

} // namespace mc

/// ScanResult передаётся между потоками через сигнал, поэтому нужен метатип.
Q_DECLARE_METATYPE(mc::ScanResult)
Q_DECLARE_METATYPE(mc::ThumbnailProgress)
