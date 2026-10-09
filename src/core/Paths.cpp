#include "core/Paths.h"

#include "core/PathHash.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace mc {

namespace {

/// Расширение файла в нижнем регистре, включая точку (".jpg").
/// QString::toLower для расширений безопасен — они ASCII.
QString lowerExtension(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix();
    return suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix.toLower();
}

} // namespace

QString AppPaths::thumbnailPathFor(const QString &mediaPath) const
{
    return QDir(thumbnailsDir).filePath(thumbnailHash(mediaPath) + lowerExtension(mediaPath) + QStringLiteral(".jpg"));
}

bool AppPaths::ensureDirectories() const
{
    QDir dir;
    return dir.mkpath(dataDir) && dir.mkpath(thumbnailsDir);
}

AppPaths defaultPaths(const QString &overrideDataDir)
{
    AppPaths paths;
    paths.dataDir = overrideDataDir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : overrideDataDir;
    paths.databaseFile = QDir(paths.dataDir).filePath(QStringLiteral("catalog.sqlite"));
    paths.thumbnailsDir = QDir(paths.dataDir).filePath(QStringLiteral("thumbnails"));
    return paths;
}

} // namespace mc
