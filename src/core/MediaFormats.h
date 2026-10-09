#pragma once

#include "core/MediaTypes.h"

#include <QString>

namespace mc {

/// Проверяет, поддерживается ли файл каталогом (по расширению).
/// Набор расширений совпадает с Electron-версией.
bool isSupportedMedia(const QString &filePath);

/// Видео ли это (по расширению).
bool isVideoFile(const QString &filePath);

/// Тип медиафайла по расширению.
MediaKind mediaKindForFile(const QString &filePath);

} // namespace mc
