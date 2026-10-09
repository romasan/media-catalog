#pragma once

#include "core/CatalogDatabase.h"

#include <QList>
#include <QPair>
#include <QString>

namespace mc {

class CatalogDatabase;

/// Обмен данными между устройствами.
///
/// Формат файла:
/// ```json
/// {
///   "version": 1,
///   "exportedAt": 1770000000000,
///   "catalogs": [{ "path": "/photos" }],
///   "tags": [{ "name": "отпуск", "lastUsedAt": 1770000000000 }],
///   "files": [{ "path": "/photos/a.jpg", "createdAt": ..., "modifiedAt": ...,
///               "size": ..., "capturedAt": ... }],
///   "mediaTags": [{ "file": "/photos/a.jpg", "tag": "отпуск" }]
/// }
/// ```
///
/// Связи выражаются путями и именами, а не идентификаторами: после импорта
/// на другом устройстве идентификаторы медиафайлов будут другими.
/// `capturedAt` пишется как `null`, если дата съёмки не вычислялась.
namespace dataTransfer {

/// Сохраняет каталоги, теги и связи. Возвращает false и заполняет error.
bool exportToFile(const CatalogDatabase &database, const QString &filePath, QString *error = nullptr);

/// Разбирает файл и возвращает данные для восстановления.
bool parseFile(const QString &filePath, ImportSummary *summary, QString *error = nullptr);

} // namespace dataTransfer

} // namespace mc
