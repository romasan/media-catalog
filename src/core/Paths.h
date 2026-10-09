#pragma once

#include <QString>

namespace mc {

/// Пути к runtime-данным приложения.
///
/// Раскладка намеренно повторяет Electron-версию (`<dataDir>/thumbnails`,
/// `<dataDir>/catalog.*`), чтобы превью, сгенерированные старой версией,
/// можно было переиспользовать без перегенерации.
///
/// Каталог данных по умолчанию — `QStandardPaths::AppDataLocation`:
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

} // namespace mc
