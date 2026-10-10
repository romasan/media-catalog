#include "core/Paths.h"

#include "core/PathHash.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

namespace mc {

namespace {

/// Имя каталога данных (совпадает с QCoreApplication::applicationName()).
constexpr QLatin1String kApplicationDirName("MediaCatalog");

/// Расширение файла в нижнем регистре, включая точку (".jpg").
/// QString::toLower для расширений безопасен — они ASCII.
QString lowerExtension(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix();
    return suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix.toLower();
}

/// Каталог данных по умолчанию.
///
/// Собирается из общего каталога данных и имени приложения, а не из
/// `AppDataLocation`: тот добавляет ещё и имя организации, из-за чего на macOS
/// получался вложенный путь `…/Application Support/MediaCatalog/MediaCatalog`.
QString defaultDataDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty()) {
        return QDir::homePath() + QStringLiteral("/.mediacatalog");
    }
    return QDir(base).filePath(kApplicationDirName);
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

QString toLocalPath(const QString &pathOrUrl)
{
    if (pathOrUrl.isEmpty()) {
        return {};
    }

    // URL разбираем только при явной схеме: иначе двоеточие в имени файла
    // (например, «отчёт: 2024.jpg») было бы принято за схему.
    if (pathOrUrl.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) {
        const QString local = QUrl(pathOrUrl).toLocalFile();
        if (!local.isEmpty()) {
            return local;
        }
    }
    return pathOrUrl;
}

AppPaths defaultPaths(const QString &overrideDataDir)
{
    AppPaths paths;
    paths.dataDir = overrideDataDir.isEmpty() ? defaultDataDir() : overrideDataDir;
    paths.databaseFile = QDir(paths.dataDir).filePath(QStringLiteral("catalog.sqlite"));
    paths.thumbnailsDir = QDir(paths.dataDir).filePath(QStringLiteral("thumbnails"));
    return paths;
}

} // namespace mc
