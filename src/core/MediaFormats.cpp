#include "core/MediaFormats.h"

#include <QFileInfo>
#include <QSet>

namespace mc {

namespace {

QString lowerSuffix(const QString &filePath)
{
    return QFileInfo(filePath).suffix().toLower();
}

const QSet<QString> &supportedExtensions()
{
    static const QSet<QString> value = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp"),
        QStringLiteral("tiff"), QStringLiteral("tif"), QStringLiteral("heic"),
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("avi"),
        QStringLiteral("mkv"), QStringLiteral("webm"), QStringLiteral("m4v"),
    };
    return value;
}

const QSet<QString> &videoExtensions()
{
    static const QSet<QString> value = {
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("avi"),
        QStringLiteral("mkv"), QStringLiteral("webm"), QStringLiteral("m4v"),
    };
    return value;
}

} // namespace

bool isSupportedMedia(const QString &filePath)
{
    return supportedExtensions().contains(lowerSuffix(filePath));
}

bool isVideoFile(const QString &filePath)
{
    return videoExtensions().contains(lowerSuffix(filePath));
}

MediaKind mediaKindForFile(const QString &filePath)
{
    return isVideoFile(filePath) ? MediaKind::Video : MediaKind::Photo;
}

} // namespace mc
