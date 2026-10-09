#include "core/ExportImport.h"

#include "core/CatalogDatabase.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace mc::dataTransfer {

namespace {

constexpr int kFormatVersion = 1;

} // namespace

bool exportToFile(const CatalogDatabase &database, const QString &filePath, QString *error)
{
    const QList<MediaFile> media = database.mediaFiles();

    QJsonArray catalogs;
    for (const Catalog &catalog : database.catalogs()) {
        QJsonObject entry;
        entry.insert(QStringLiteral("id"), catalog.id);
        entry.insert(QStringLiteral("path"), catalog.path);
        catalogs.append(entry);
    }

    QJsonArray tags;
    for (const Tag &tag : database.tags()) {
        QJsonObject entry;
        entry.insert(QStringLiteral("id"), tag.id);
        entry.insert(QStringLiteral("name"), tag.name);
        entry.insert(QStringLiteral("lastUsedAt"), static_cast<double>(tag.lastUsedAt));
        tags.append(entry);
    }

    QJsonArray files;
    QHash<QString, QString> pathById;
    for (const MediaFile &file : media) {
        QJsonObject entry;
        entry.insert(QStringLiteral("path"), file.path);
        entry.insert(QStringLiteral("name"), file.name);
        entry.insert(QStringLiteral("size"), static_cast<double>(file.size));
        entry.insert(QStringLiteral("createdAt"), static_cast<double>(file.createdAt));
        entry.insert(QStringLiteral("modifiedAt"), static_cast<double>(file.modifiedAt));
        entry.insert(QStringLiteral("kind"), file.kind == MediaKind::Video
                                                 ? QStringLiteral("video")
                                                 : QStringLiteral("photo"));
        // null означает «дата съёмки не вычислялась» — это отличается от 0.
        entry.insert(QStringLiteral("capturedAt"),
                     file.hasCapturedAt ? QJsonValue(static_cast<double>(file.capturedAt))
                                        : QJsonValue(QJsonValue::Null));
        files.append(entry);
        pathById.insert(file.id, file.path);
    }

    QJsonArray mediaTags;
    for (const Tag &tag : database.tags()) {
        for (const QString &mediaId : database.mediaIdsByTag(tag.id)) {
            const QString path = pathById.value(mediaId);
            if (path.isEmpty()) {
                continue;
            }
            QJsonObject entry;
            entry.insert(QStringLiteral("file"), path);
            entry.insert(QStringLiteral("tag"), tag.name);
            mediaTags.append(entry);
        }
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), kFormatVersion);
    root.insert(QStringLiteral("exportedAt"),
                static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    root.insert(QStringLiteral("catalogs"), catalogs);
    root.insert(QStringLiteral("tags"), tags);
    root.insert(QStringLiteral("files"), files);
    root.insert(QStringLiteral("mediaTags"), mediaTags);

    // Пишем через временный файл: прерванный экспорт не должен оставить
    // обрезанный JSON на месте готового.
    const QString tempPath = filePath + QStringLiteral(".tmp");
    QFile file(tempPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Не удалось записать файл: %1").arg(file.errorString());
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    QFile::remove(filePath);
    if (!QFile::rename(tempPath, filePath)) {
        QFile::remove(tempPath);
        if (error) {
            *error = QStringLiteral("Не удалось переименовать временный файл экспорта");
        }
        return false;
    }
    return true;
}

bool parseFile(const QString &filePath, ImportSummary *summary, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Не удалось открыть файл: %1").arg(file.errorString());
        }
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("Неверный формат файла импорта: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    if (!root.contains(QStringLiteral("catalogs")) || !root.value(QStringLiteral("catalogs")).isArray()) {
        if (error) {
            *error = QStringLiteral("Неверный формат файла импорта: нет списка каталогов");
        }
        return false;
    }

    if (summary) {
        summary->catalogPaths.clear();
        summary->tagNames.clear();
        summary->mediaTagPaths.clear();

        for (const QJsonValue &value : root.value(QStringLiteral("catalogs")).toArray()) {
            const QString path = value.toObject().value(QStringLiteral("path")).toString();
            if (!path.isEmpty()) {
                summary->catalogPaths.append(path);
            }
        }

        for (const QJsonValue &value : root.value(QStringLiteral("tags")).toArray()) {
            const QString name = value.toObject().value(QStringLiteral("name")).toString();
            if (!name.isEmpty()) {
                summary->tagNames.append(name);
            }
        }

        for (const QJsonValue &value : root.value(QStringLiteral("mediaTags")).toArray()) {
            const QJsonObject entry = value.toObject();
            const QString mediaPath = entry.value(QStringLiteral("file")).toString();
            const QString tagName = entry.value(QStringLiteral("tag")).toString();
            if (!mediaPath.isEmpty() && !tagName.isEmpty()) {
                summary->mediaTagPaths.append({mediaPath, tagName});
            }
        }
    }

    return true;
}

} // namespace mc::dataTransfer
