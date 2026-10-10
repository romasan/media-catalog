#pragma once

#include <QString>

namespace mc {

/// Пути к runtime-данным приложения.
///
/// Раскладка намеренно повторяет Electron-версию (`<dataDir>/thumbnails`,
/// `<dataDir>/catalog.*`), чтобы превью, сгенерированные старой версией,
/// можно было переиспользовать без перегенерации.
///
/// Каталог данных по умолчанию (общий каталог данных ОС + имя приложения):
///   - macOS   `~/Library/Application Support/MediaCatalog`
///   - Windows `%APPDATA%/MediaCatalog`
///   - Linux   `~/.local/share/MediaCatalog`
struct AppPaths {
    QString dataDir;        ///< корень runtime-данных
    QString databaseFile;   ///< `<dataDir>/catalog.sqlite`
    QString thumbnailsDir;  ///< `<dataDir>/thumbnails`

    /// Путь превью для медиафайла: `<thumbnailsDir>/<hash><ext>.jpg`.
    /// Имя повторяет алгоритм Electron-версии, см. PathHash.h.
    QString thumbnailPathFor(const QString &mediaPath) const;

    /// Создаёт недостающие каталоги. Возвращает false, если создать не удалось.
    bool ensureDirectories() const;
};

/// Пути по умолчанию. `overrideDataDir` (из `--data-dir`) имеет приоритет.
AppPaths defaultPaths(const QString &overrideDataDir = QString());

/// Приводит строку к локальному пути файловой системы.
///
/// Диалоги Qt (`FolderDialog`, `FileDialog`) возвращают URL вида
/// `file:///Users/x/photos`. Если передать такой URL в `QFileInfo`, он будет
/// понят как относительный путь и превратится в мусор вида
/// `<рабочий каталог>/file:/Users/x/photos` — папка не найдётся, и
/// сканирование молча пропустит каталог. Функция принимает и URL, и обычный
/// путь.
QString toLocalPath(const QString &pathOrUrl);

} // namespace mc
