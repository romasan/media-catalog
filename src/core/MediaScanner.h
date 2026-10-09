#pragma once

#include "core/MediaTypes.h"

#include <QObject>
#include <QStringList>

namespace mc {

class CatalogDatabase;
class ThumbnailGenerator;

/// Рекурсивный обход каталогов и синхронизация базы с файловой системой.
///
/// Синхронизация:
///   1. обход всех каталогов, сбор поддерживаемых файлов;
///   2. добавление отсутствующих в базе записей;
///   3. обновление изменившихся (mtime или размер);
///   4. удаление записей, чьи файлы больше не найдены;
///   5. пересчёт принадлежности файлов каталогам (вложенные каталоги);
///   6. постановка новых файлов в очередь генерации превью.
class MediaScanner : public QObject
{
    Q_OBJECT

public:
    MediaScanner(CatalogDatabase &database, ThumbnailGenerator &thumbnails, QObject *parent = nullptr);

    /// Выполняет полный проход синхронизации. Метод блокирующий:
    /// вызывающий код должен запускать его в рабочем потоке.
    ScanResult scan();

signals:
    /// Прогресс обхода: сколько каталогов уже обработано.
    void progress(int processed, int total);

private:
    CatalogDatabase &m_database;
    ThumbnailGenerator &m_thumbnails;
};

} // namespace mc
