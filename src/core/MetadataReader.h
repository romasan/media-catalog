#pragma once

#include "core/MediaTypes.h"

#include <QString>

namespace mc {

/// Путь к ffprobe/ffmpeg. Пустая строка означает «не найден».
/// Переопределяется переменной окружения MEDIACATALOG_FFPROBE / _FFMPEG.
QString ffprobePath();
QString ffmpegPath();

/// Дата съёмки из метаданных файла, мс с начала эпохи.
/// Видео: `creation_time` контейнера через ffprobe.
/// Фото: EXIF-теги DateTimeOriginal → CreateDate → ModifyDate.
/// Возвращает false, если дату определить не удалось.
bool readCaptureDate(const QString &filePath, MediaKind kind, qint64 *outMs);

/// Минимальный парсер EXIF-даты для JPEG и TIFF.
/// Вынесен отдельно, чтобы его можно было протестировать без файловой системы.
namespace exif {

/// Разбирает содержимое JPEG (целиком) и возвращает дату съёмки.
bool dateFromJpeg(const QByteArray &data, qint64 *outMs);

/// Разбирает TIFF/EXIF-блок (данные начиная с байтов порядка II/MM).
bool dateFromTiff(const QByteArray &data, qint64 *outMs);

} // namespace exif

} // namespace mc
