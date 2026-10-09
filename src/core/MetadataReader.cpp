#include "core/MetadataReader.h"

#include "core/MediaFormats.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTimeZone>

#include <cstdint>

namespace mc {

namespace {

/// Читает 16-битное значение с учётом порядка байтов.
std::uint16_t readU16(const QByteArray &data, int offset, bool littleEndian)
{
    if (offset < 0 || offset + 2 > data.size()) {
        return 0;
    }
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(data.constData() + offset);
    if (littleEndian) {
        return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
    }
    return static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
}

std::uint32_t readU32(const QByteArray &data, int offset, bool littleEndian)
{
    if (offset < 0 || offset + 4 > data.size()) {
        return 0;
    }
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(data.constData() + offset);
    if (littleEndian) {
        return static_cast<std::uint32_t>(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
    }
    return static_cast<std::uint32_t>((bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3]);
}

/// Разбирает EXIF-строку даты `YYYY:MM:DD HH:MM:SS` (иногда с `-` вместо `:`).
/// Возвращает false, если строка не похожа на дату.
bool parseExifDateString(const QString &raw, qint64 *outMs)
{
    const QString value = raw.trimmed();
    if (value.size() < 19) {
        return false;
    }

    // Разделители даты бывают `:` (стандарт EXIF) или `-`.
    QString normalized = value.left(19);
    normalized.replace(4, 1, QLatin1Char('-'));
    normalized.replace(7, 1, QLatin1Char('-'));

    const QStringList parts = normalized.split(QLatin1Char(' '));
    if (parts.size() != 2) {
        return false;
    }

    const QDate date = QDate::fromString(parts.at(0), QStringLiteral("yyyy-MM-dd"));
    const QTime time = QTime::fromString(parts.at(1), QStringLiteral("HH:mm:ss"));
    if (!date.isValid() || !time.isValid()) {
        return false;
    }

    // EXIF не хранит часовой пояс: трактуем время как локальное,
    // как это делает exifr в Electron-версии.
    const QDateTime dateTime(date, time);
    if (!dateTime.isValid()) {
        return false;
    }

    *outMs = dateTime.toMSecsSinceEpoch();
    return true;
}

/// Ищет в TIFF-блоке первый из интересующих EXIF-тегов с датой.
/// Порядок важен: DateTimeOriginal → CreateDate → ModifyDate.
bool searchTiffForDate(const QByteArray &data, bool littleEndian, std::uint32_t ifdOffset,
                       int depth, qint64 *outMs)
{
    if (depth > 4 || ifdOffset == 0) {
        return false;
    }

    const int entryCount = readU16(data, static_cast<int>(ifdOffset), littleEndian);
    if (entryCount <= 0) {
        return false;
    }

    // Теги с датой; порядок задаёт приоритет.
    constexpr std::uint16_t kDateTimeOriginal = 0x9003;
    constexpr std::uint16_t kCreateDate = 0x9004;
    constexpr std::uint16_t kDateTime = 0x0132;
    constexpr std::uint16_t kExifIfdPointer = 0x8769;

    QString best;
    std::uint32_t exifIfdPointer = 0;

    for (int i = 0; i < entryCount; ++i) {
        const int entryOffset = static_cast<int>(ifdOffset) + 2 + i * 12;
        if (entryOffset + 12 > data.size()) {
            break;
        }

        const std::uint16_t tag = readU16(data, entryOffset, littleEndian);
        if (tag != kDateTimeOriginal && tag != kCreateDate && tag != kDateTime
            && tag != kExifIfdPointer) {
            continue;
        }

        const std::uint16_t type = readU16(data, entryOffset + 2, littleEndian);
        const std::uint32_t count = readU32(data, entryOffset + 4, littleEndian);

        if (tag == kExifIfdPointer) {
            exifIfdPointer = readU32(data, entryOffset + 8, littleEndian);
            continue;
        }

        // Дата — ASCII (тип 2), 19 байт. Если не влезает в 4 байта,
        // значение лежит по смещению от начала TIFF-блока.
        if (type != 2 || count < 19) {
            continue;
        }

        const int valueOffset = (count > 4)
            ? static_cast<int>(readU32(data, entryOffset + 8, littleEndian))
            : entryOffset + 8;

        if (valueOffset < 0 || valueOffset + 19 > data.size()) {
            continue;
        }

        const QString raw = QString::fromLatin1(data.mid(valueOffset, 19));
        if (best.isEmpty() || tag == kDateTimeOriginal) {
            best = raw;
        }
        if (tag == kDateTimeOriginal) {
            break; // самый приоритетный тег найден
        }
    }

    if (!best.isEmpty() && parseExifDateString(best, outMs)) {
        return true;
    }

    // Дата не нашлась в этом IFD — идём в EXIF SubIFD, если он есть.
    if (exifIfdPointer != 0) {
        return searchTiffForDate(data, littleEndian, exifIfdPointer, depth + 1, outMs);
    }
    return false;
}

} // namespace

QString ffprobePath()
{
    const QByteArray override = qgetenv("MEDIACATALOG_FFPROBE");
    if (!override.isEmpty()) {
        return QString::fromLocal8Bit(override);
    }

    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }

    const QStringList candidates = {
        QStringLiteral("/opt/homebrew/bin/ffprobe"),
        QStringLiteral("/usr/local/bin/ffprobe"),
        QStringLiteral("/usr/bin/ffprobe"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QString();
}

QString ffmpegPath()
{
    const QByteArray override = qgetenv("MEDIACATALOG_FFMPEG");
    if (!override.isEmpty()) {
        return QString::fromLocal8Bit(override);
    }

    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }

    const QStringList candidates = {
        QStringLiteral("/opt/homebrew/bin/ffmpeg"),
        QStringLiteral("/usr/local/bin/ffmpeg"),
        QStringLiteral("/usr/bin/ffmpeg"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QString();
}

namespace exif {

bool dateFromJpeg(const QByteArray &data, qint64 *outMs)
{
    if (data.size() < 4 || static_cast<std::uint8_t>(data.at(0)) != 0xFF
        || static_cast<std::uint8_t>(data.at(1)) != 0xD8) {
        return false;
    }

    int offset = 2;
    while (offset + 4 <= data.size()) {
        if (static_cast<std::uint8_t>(data.at(offset)) != 0xFF) {
            ++offset; // пропускаем заполнители
            continue;
        }

        const auto marker = static_cast<std::uint8_t>(data.at(offset + 1));
        // Маркеры без полезной нагрузки.
        if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            offset += 2;
            continue;
        }
        if (marker == 0xDA) {
            break; // начало сжатых данных — метаданных дальше нет
        }

        const int segmentLength = readU16(data, offset + 2, false);
        if (segmentLength < 2) {
            break;
        }

        if (marker == 0xE1) { // APP1
            const int payload = offset + 4;
            const int payloadLength = segmentLength - 2;
            const QByteArray header = data.mid(payload, 6);
            if (header == QByteArrayLiteral("Exif\0\0")) {
                const QByteArray tiff = data.mid(payload + 6, payloadLength - 6);
                return dateFromTiff(tiff, outMs);
            }
        }

        offset += 2 + segmentLength;
    }

    return false;
}

bool dateFromTiff(const QByteArray &data, qint64 *outMs)
{
    if (data.size() < 8) {
        return false;
    }

    bool littleEndian = false;
    if (data.startsWith(QByteArrayLiteral("II"))) {
        littleEndian = true;
    } else if (data.startsWith(QByteArrayLiteral("MM"))) {
        littleEndian = false;
    } else {
        return false;
    }

    if (readU16(data, 2, littleEndian) != 42) {
        return false;
    }

    const std::uint32_t firstIfd = readU32(data, 4, littleEndian);
    return searchTiffForDate(data, littleEndian, firstIfd, 0, outMs);
}

} // namespace exif

bool readCaptureDate(const QString &filePath, MediaKind kind, qint64 *outMs)
{
    if (!QFileInfo::exists(filePath)) {
        return false;
    }
    if (outMs) {
        *outMs = 0;
    }

    if (kind == MediaKind::Video || isVideoFile(filePath)) {
        const QString probe = ffprobePath();
        if (probe.isEmpty()) {
            return false;
        }

        QProcess process;
        process.start(probe, {QStringLiteral("-v"), QStringLiteral("quiet"),
                              QStringLiteral("-print_format"), QStringLiteral("json"),
                              QStringLiteral("-show_entries"),
                              QStringLiteral("format_tags=creation_time"),
                              filePath});
        if (!process.waitForFinished(15000) || process.exitCode() != 0) {
            return false;
        }

        const QJsonObject root = QJsonDocument::fromJson(process.readAllStandardOutput()).object();
        const QString creationTime = root.value(QStringLiteral("format")).toObject()
                                         .value(QStringLiteral("tags")).toObject()
                                         .value(QStringLiteral("creation_time")).toString();
        if (creationTime.isEmpty()) {
            return false;
        }

        // creation_time в контейнерах задаётся по UTC.
        QDateTime dateTime = QDateTime::fromString(creationTime, Qt::ISODate);
        if (!dateTime.isValid()) {
            dateTime = QDateTime::fromString(creationTime, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            if (dateTime.isValid()) {
                dateTime.setTimeZone(QTimeZone::UTC);
            }
        } else if (dateTime.timeSpec() == Qt::LocalTime) {
            // Строка без зоны — считаем UTC (поведение Electron-версии).
            dateTime.setTimeZone(QTimeZone::UTC);
        }

        if (!dateTime.isValid()) {
            return false;
        }
        if (outMs) {
            *outMs = dateTime.toMSecsSinceEpoch();
        }
        return true;
    }

    // Фото: EXIF лежит в начале файла, целиком читать многомегабайтный
    // файл не нужно — ограничиваемся первыми 256 КБ.
    constexpr qint64 kMaxHeaderBytes = 256 * 1024;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray header = file.read(kMaxHeaderBytes);
    file.close();

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) {
        return exif::dateFromJpeg(header, outMs);
    }
    if (suffix == QLatin1String("tif") || suffix == QLatin1String("tiff")) {
        return exif::dateFromTiff(header, outMs);
    }
    // HEIC/WebP/PNG содержат EXIF в других контейнерах — пока не поддерживаем.
    return false;
}

} // namespace mc
