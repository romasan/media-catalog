// Консольные тесты ядра — запускаются без GUI.
//
// Проверяют то, что легко сломать незаметно: совместимость хэша имён превью
// с Electron-версией, разбор EXIF, семантику метатегов, фильтрацию,
// сканирование файловой системы, генерацию превью и обмен данными.

#include "core/CatalogDatabase.h"
#include "core/ExportImport.h"
#include "core/MediaFilter.h"
#include "core/MediaFormats.h"
#include "core/MediaScanner.h"
#include "core/MetaTags.h"
#include "core/MetadataReader.h"
#include "core/PathHash.h"
#include "core/Paths.h"
#include "core/ThumbnailGenerator.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include <cstdio>

using namespace mc;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const QString &what)
{
    ++g_checks;
    if (!condition) {
        ++g_failures;
        QTextStream(stderr) << "  ПРОВАЛ: " << what << '\n';
    }
}

void checkEqual(const QString &actual, const QString &expected, const QString &what)
{
    check(actual == expected, QStringLiteral("%1 (получено «%2», ожидалось «%3»)")
                                  .arg(what, actual, expected));
}

void checkEqual(qint64 actual, qint64 expected, const QString &what)
{
    check(actual == expected, QStringLiteral("%1 (получено %2, ожидалось %3)")
                                  .arg(what).arg(actual).arg(expected));
}

void section(const QString &name)
{
    QTextStream(stdout) << "== " << name << '\n';
}

/// Прогоняет цикл событий указанное время — нужно для асинхронных задач.
void waitFor(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

/// Создаёт настоящий JPEG заданного размера.
QString writeJpeg(const QString &path, int width, int height)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    image.save(path, "JPEG");
    return path;
}

/// Генерирует EXIF-блок с одним тегом даты и подставляет его в JPEG.
/// Возвращает true, если удалось собрать корректный файл.
bool writeJpegWithExifDate(const QString &path, const QByteArray &dateText)
{
    QImage image(16, 16, QImage::Format_RGB32);
    image.fill(Qt::white);
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPEG")) {
        return false;
    }
    buffer.close();

    // Собираем APP1: "Exif\0\0" + TIFF (little-endian) с одним IFD-тегом.
    QByteArray tiff;
    auto appendU16 = [&tiff](quint16 value) {
        tiff.append(static_cast<char>(value & 0xFF));
        tiff.append(static_cast<char>((value >> 8) & 0xFF));
    };
    auto appendU32 = [&tiff](quint32 value) {
        for (int i = 0; i < 4; ++i) {
            tiff.append(static_cast<char>((value >> (8 * i)) & 0xFF));
        }
    };

    tiff.append("II", 2);
    appendU16(42);
    appendU32(8); // смещение первого IFD

    const int entryCount = 1;
    appendU16(entryCount);
    appendU16(0x9003);                       // DateTimeOriginal
    appendU16(2);                            // тип ASCII
    appendU32(static_cast<quint32>(dateText.size() + 1));
    const quint32 valueOffset = 8 + 2 + entryCount * 12 + 4;
    appendU32(valueOffset);
    appendU32(0);                            // следующего IFD нет
    tiff.append(dateText);
    tiff.append('\0');

    QByteArray app1 = QByteArrayLiteral("Exif\0\0") + tiff;
    QByteArray segment;
    segment.append(static_cast<char>(0xFF));
    segment.append(static_cast<char>(0xE1));
    const quint16 segmentLength = static_cast<quint16>(app1.size() + 2);
    segment.append(static_cast<char>((segmentLength >> 8) & 0xFF));
    segment.append(static_cast<char>(segmentLength & 0xFF));
    segment.append(app1);

    // Вставляем APP1 сразу после SOI.
    QByteArray result = jpeg.left(2) + segment + jpeg.mid(2);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(result);
    return true;
}

void testPathHash()
{
    section(QStringLiteral("Хэш пути (совместимость с Electron)"));

    // Эталоны получены запуском оригинального алгоритма на Node.js.
    checkEqual(thumbnailHash(QStringLiteral("/Users/r.bauer/Bag/photos/a.jpg")),
               QStringLiteral("ru1hjf"), QStringLiteral("хэш латинского пути"));
    checkEqual(thumbnailHash(QString()), QStringLiteral("0"), QStringLiteral("хэш пустой строки"));
    checkEqual(thumbnailHash(QStringLiteral("a")), QStringLiteral("2p"), QStringLiteral("хэш одного символа"));
    checkEqual(thumbnailHash(QString::fromUtf8("/tmp/тест/фото.jpg")),
               QStringLiteral("8bv75p"), QStringLiteral("хэш кириллического пути"));
    checkEqual(thumbnailHash(QString::fromUtf8("\xF0\x9F\x96\xBC\xEF\xB8\x8F.png")),
               QStringLiteral("fse4uj"), QStringLiteral("хэш эмодзи (суррогатная пара)"));

    AppPaths paths;
    paths.thumbnailsDir = QStringLiteral("/data/thumbnails");
    checkEqual(paths.thumbnailPathFor(QStringLiteral("/x/1ho96a.jpg")),
               QStringLiteral("/data/thumbnails/5dc1ob.jpg.jpg"),
               QStringLiteral("имя файла превью: хэш + расширение + .jpg"));
    checkEqual(paths.thumbnailPathFor(QStringLiteral("/x/photo.PNG")),
               QStringLiteral("/data/thumbnails/%1.png.jpg").arg(thumbnailHash(QStringLiteral("/x/photo.PNG"))),
               QStringLiteral("расширение приводится к нижнему регистру"));
}

void testLocalPathConversion()
{
    section(QStringLiteral("Преобразование URL в путь"));

    // Диалоги Qt возвращают URL; QFileInfo понял бы его как относительный путь.
    checkEqual(toLocalPath(QStringLiteral("file:///Users/x/photos")),
               QStringLiteral("/Users/x/photos"), QStringLiteral("обычный file-URL"));
    checkEqual(toLocalPath(QStringLiteral("file:///tmp/%D1%84%D0%BE%D1%82%D0%BE")),
               QString::fromUtf8("/tmp/фото"), QStringLiteral("URL с процентным кодированием"));
    checkEqual(toLocalPath(QStringLiteral("/Users/x/photos")),
               QStringLiteral("/Users/x/photos"), QStringLiteral("обычный путь не меняется"));
    checkEqual(toLocalPath(QStringLiteral("отчёт: 2024.jpg")),
               QStringLiteral("отчёт: 2024.jpg"), QStringLiteral("двоеточие в имени не схема"));
    checkEqual(toLocalPath(QString()), QString(), QStringLiteral("пустая строка"));
}

void testFormats()
{
    section(QStringLiteral("Поддерживаемые форматы"));

    check(isSupportedMedia(QStringLiteral("a.JPG")), QStringLiteral("JPG в верхнем регистре поддерживается"));
    check(isSupportedMedia(QStringLiteral("a.heic")), QStringLiteral("HEIC поддерживается"));
    check(!isSupportedMedia(QStringLiteral("a.txt")), QStringLiteral("TXT не поддерживается"));
    check(!isSupportedMedia(QStringLiteral("noext")), QStringLiteral("файл без расширения не поддерживается"));
    check(isVideoFile(QStringLiteral("a.MP4")), QStringLiteral("MP4 распознаётся как видео"));
    check(!isVideoFile(QStringLiteral("a.jpg")), QStringLiteral("JPEG не видео"));
    check(mediaKindForFile(QStringLiteral("a.mkv")) == MediaKind::Video, QStringLiteral("MKV — видео"));
}

void testMetaTags()
{
    section(QStringLiteral("Метатеги"));

    MediaFile file;
    file.modifiedAt = QDateTime(QDate(2023, 7, 15), QTime(12, 0)).toMSecsSinceEpoch();
    file.kind = MediaKind::Photo;

    const QList<MetaTag> tags = metaTags::forFile(file);
    check(tags.size() == 3, QStringLiteral("у файла три метатега"));
    checkEqual(tags.at(0).id, QStringLiteral("meta:year:2023"), QStringLiteral("идентификатор года"));
    checkEqual(tags.at(1).id, QStringLiteral("meta:season:лето"), QStringLiteral("идентификатор сезона"));
    checkEqual(tags.at(2).id, QStringLiteral("meta:type:фото"), QStringLiteral("идентификатор типа"));

    check(metaTags::matches(file, QStringLiteral("meta:season:лето")), QStringLiteral("файл соответствует сезону"));
    check(!metaTags::matches(file, QStringLiteral("meta:season:зима")), QStringLiteral("файл не соответствует другому сезону"));

    checkEqual(metaTags::seasonForMonth(11), QStringLiteral("зима"), QStringLiteral("декабрь — зима"));
    checkEqual(metaTags::seasonForMonth(0), QStringLiteral("зима"), QStringLiteral("январь — зима"));
    checkEqual(metaTags::seasonForMonth(4), QStringLiteral("весна"), QStringLiteral("май — весна"));
    checkEqual(metaTags::seasonForMonth(7), QStringLiteral("лето"), QStringLiteral("август — лето"));
    checkEqual(metaTags::seasonForMonth(9), QStringLiteral("осень"), QStringLiteral("октябрь — осень"));

    MetaTag parsed;
    check(metaTags::metaTagById(QStringLiteral("meta:year:1999"), &parsed), QStringLiteral("разбор метатега года"));
    checkEqual(parsed.name, QStringLiteral("1999"), QStringLiteral("имя метатега года"));
    check(metaTags::metaTagById(metaTags::untaggedId(), &parsed), QStringLiteral("разбор «без тега»"));
    check(!metaTags::metaTagById(QStringLiteral("meta:bogus:x"), &parsed), QStringLiteral("неизвестный вид метатега отклонён"));
    check(metaTags::isMetaTagId(QStringLiteral("meta:year:2001")), QStringLiteral("префикс метатега распознан"));
    check(!metaTags::isMetaTagId(QStringLiteral("обычный-тег")), QStringLiteral("обычный тег не метатег"));
}

void testExifParsing()
{
    section(QStringLiteral("Разбор EXIF-даты"));

    QTemporaryDir dir;
    check(dir.isValid(), QStringLiteral("временный каталог создан"));

    const QString withExif = dir.filePath(QStringLiteral("exif.jpg"));
    check(writeJpegWithExifDate(withExif, QByteArrayLiteral("2021:03:04 05:06:07")),
          QStringLiteral("JPEG с EXIF записан"));

    qint64 captured = 0;
    check(readCaptureDate(withExif, MediaKind::Photo, &captured), QStringLiteral("дата из EXIF прочитана"));
    const QDateTime expected(QDate(2021, 3, 4), QTime(5, 6, 7));
    checkEqual(captured, expected.toMSecsSinceEpoch(), QStringLiteral("значение даты EXIF"));

    const QString plain = dir.filePath(QStringLiteral("plain.jpg"));
    writeJpeg(plain, 32, 32);
    qint64 ignored = 0;
    check(!readCaptureDate(plain, MediaKind::Photo, &ignored), QStringLiteral("JPEG без EXIF не даёт дату"));
}

void testDatabase()
{
    section(QStringLiteral("База данных"));

    QTemporaryDir dir;
    CatalogDatabase db;
    check(db.open(dir.filePath(QStringLiteral("test.sqlite"))), QStringLiteral("база открыта"));

    const Catalog catalog = db.addCatalog(dir.path());
    check(!catalog.id.isEmpty(), QStringLiteral("каталог получил идентификатор"));
    checkEqual(db.catalogs().size(), 1, QStringLiteral("каталог сохранён"));
    checkEqual(db.addCatalog(dir.path()).id, catalog.id, QStringLiteral("повторное добавление возвращает тот же каталог"));

    // Имена тегов нормализуются.
    const Tag tag = db.createTag(QStringLiteral("  Отпуск  "));
    checkEqual(tag.name, QStringLiteral("отпуск"), QStringLiteral("имя тега в нижнем регистре без пробелов"));
    checkEqual(db.createTag(QStringLiteral("ОТПУСК")).id, tag.id, QStringLiteral("дубликат тега не создаётся"));

    MediaFile file;
    file.id = QStringLiteral("media-1");
    file.path = dir.filePath(QStringLiteral("a.jpg"));
    file.name = QStringLiteral("a.jpg");
    file.kind = MediaKind::Photo;
    file.size = 100;
    file.createdAt = 1000;
    file.modifiedAt = 2000;
    file.catalogId = catalog.id;

    MediaFile video = file;
    video.id = QStringLiteral("media-2");
    video.path = dir.filePath(QStringLiteral("b.mp4"));
    video.name = QStringLiteral("b.mp4");
    video.kind = MediaKind::Video;
    video.modifiedAt = 3000;

    checkEqual(db.upsertMedia({file, video}), 2, QStringLiteral("вставлены два файла"));
    checkEqual(db.upsertMedia({file}), 0, QStringLiteral("повторная вставка не добавляет записей"));
    checkEqual(db.mediaCount(), 2, QStringLiteral("счётчик файлов"));
    checkEqual(db.mediaCountByKind(MediaKind::Video), 1, QStringLiteral("счётчик видео"));
    checkEqual(db.untaggedCount(), 2, QStringLiteral("счётчик файлов без тегов"));

    db.applyTag(file.id, tag.id);
    checkEqual(db.untaggedCount(), 1, QStringLiteral("после применения тега файл перестал быть без тега"));
    checkEqual(db.mediaTags(file.id).size(), 1, QStringLiteral("тег привязан к файлу"));
    checkEqual(db.tagUsages().first().count, 1, QStringLiteral("счётчик применений тега"));
    check(db.tagUsages().first().tag.lastUsedAt > 0, QStringLiteral("обновлено время последнего применения"));

    db.removeTagFromMedia(file.id, tag.id);
    checkEqual(db.mediaTags(file.id).size(), 0, QStringLiteral("тег отвязан"));
    checkEqual(db.tagUsages().first().tag.lastUsedAt, 0, QStringLiteral("время последнего применения сброшено"));

    // Дата съёмки: NULL (не вычислялась) отличается от 0 (не найдена).
    MediaFile stored;
    check(db.mediaFileById(file.id, &stored), QStringLiteral("файл найден по идентификатору"));
    check(!stored.hasCapturedAt, QStringLiteral("дата съёмки изначально не вычислялась"));
    db.markCapturedAtMissing(file.id);
    check(db.mediaFileById(file.id, &stored), QStringLiteral("файл перечитан"));
    check(stored.hasCapturedAt, QStringLiteral("отсутствие даты съёмки записано"));
    checkEqual(stored.capturedAt, 0, QStringLiteral("значение отсутствующей даты — 0"));
    db.updateCapturedAt(file.id, 1234567);
    check(db.mediaFileById(file.id, &stored), QStringLiteral("файл перечитан снова"));
    checkEqual(stored.capturedAt, 1234567, QStringLiteral("дата съёмки сохранена"));

    checkEqual(db.tagCount(), 1, QStringLiteral("счётчик тегов"));
    db.deleteTag(tag.id);
    checkEqual(db.tagCount(), 0, QStringLiteral("тег удалён"));

    checkEqual(db.removeMediaByPaths({video.path}).size(), 1, QStringLiteral("файл удалён по пути"));
    checkEqual(db.mediaCount(), 1, QStringLiteral("остался один файл"));

    db.removeCatalog(catalog.id);
    checkEqual(db.mediaCount(), 0, QStringLiteral("удаление каталога убирает его файлы"));
}

void testFilter()
{
    section(QStringLiteral("Фильтрация по тегам и метатегам"));

    MediaFile summer;
    summer.id = QStringLiteral("1");
    summer.modifiedAt = QDateTime(QDate(2022, 7, 1), QTime(0, 0)).toMSecsSinceEpoch();
    summer.kind = MediaKind::Photo;

    MediaFile winter;
    winter.id = QStringLiteral("2");
    winter.modifiedAt = QDateTime(QDate(2022, 1, 1), QTime(0, 0)).toMSecsSinceEpoch();
    winter.kind = MediaKind::Video;

    const QList<MediaFile> all = {summer, winter};
    const QSet<QString> tagged = {QStringLiteral("1")};

    FilterCondition filter;
    filter.tagIds = {QStringLiteral("meta:season:лето")};
    checkEqual(filterMedia(all, filter, tagged).size(), 1, QStringLiteral("фильтр по сезону"));

    filter.tagIds = {QStringLiteral("meta:type:видео")};
    checkEqual(filterMedia(all, filter, tagged).size(), 1, QStringLiteral("фильтр по типу файла"));

    filter.tagIds = {metaTags::untaggedId()};
    checkEqual(filterMedia(all, filter, tagged).size(), 1, QStringLiteral("фильтр «без тега»"));
    checkEqual(filterMedia(all, filter, tagged).first().id, QStringLiteral("2"), QStringLiteral("«без тега» — это зимний файл"));

    // И: год 2022 И сезон лето.
    filter.mode = FilterMode::All;
    filter.tagIds = {QStringLiteral("meta:year:2022"), QStringLiteral("meta:season:лето")};
    checkEqual(filterMedia(all, filter, tagged).size(), 1, QStringLiteral("режим И"));

    // ИЛИ: лето ИЛИ видео — подходят оба файла.
    filter.mode = FilterMode::Any;
    checkEqual(filterMedia(all, filter, tagged).size(), 2, QStringLiteral("режим ИЛИ"));

    // И с несовместимыми условиями — пусто.
    filter.mode = FilterMode::All;
    filter.tagIds = {QStringLiteral("meta:season:лето"), QStringLiteral("meta:season:зима")};
    checkEqual(filterMedia(all, filter, tagged).size(), 0, QStringLiteral("противоречивый фильтр И пуст"));

    // Пустой фильтр пропускает всё.
    FilterCondition empty;
    checkEqual(filterMedia(all, empty, tagged).size(), 2, QStringLiteral("пустой фильтр пропускает всё"));
}

void testScannerAndThumbnails()
{
    section(QStringLiteral("Сканирование и генерация превью"));

    QTemporaryDir workDir;
    QTemporaryDir dataDir;
    check(workDir.isValid() && dataDir.isValid(), QStringLiteral("временные каталоги созданы"));

    // Готовим дерево: два фото, подпапка с ещё одним фото, скрытая папка,
    // неподдерживаемый файл и «системная» папка @eaDir.
    writeJpeg(workDir.filePath(QStringLiteral("one.jpg")), 64, 48);
    writeJpeg(workDir.filePath(QStringLiteral("two.png")), 32, 32);

    QDir(workDir.path()).mkpath(QStringLiteral("nested"));
    writeJpeg(workDir.filePath(QStringLiteral("nested/three.jpg")), 40, 40);

    QDir(workDir.path()).mkpath(QStringLiteral(".hidden"));
    writeJpeg(workDir.filePath(QStringLiteral(".hidden/skip.jpg")), 16, 16);

    QDir(workDir.path()).mkpath(QStringLiteral("@eaDir"));
    writeJpeg(workDir.filePath(QStringLiteral("@eaDir/skip.jpg")), 16, 16);

    QFile notes(workDir.filePath(QStringLiteral("notes.txt")));
    notes.open(QIODevice::WriteOnly);
    notes.write("не медиа");
    notes.close();

    AppPaths paths;
    paths.dataDir = dataDir.path();
    paths.databaseFile = QDir(dataDir.path()).filePath(QStringLiteral("catalog.sqlite"));
    paths.thumbnailsDir = QDir(dataDir.path()).filePath(QStringLiteral("thumbnails"));

    CatalogDatabase db;
    check(db.open(paths.databaseFile), QStringLiteral("база теста открыта"));
    db.addCatalog(workDir.path());

    ThumbnailGenerator thumbnails(db, paths);
    MediaScanner scanner(db, thumbnails);

    const ScanResult first = scanner.scan();
    checkEqual(first.addedFiles, 3, QStringLiteral("найдены три медиафайла, служебные папки пропущены"));
    checkEqual(db.mediaCount(), 3, QStringLiteral("три записи в базе"));

    // Повторное сканирование не должно ничего добавлять.
    const ScanResult second = scanner.scan();
    checkEqual(second.addedFiles, 0, QStringLiteral("повторное сканирование ничего не добавляет"));
    checkEqual(second.removedFiles, 0, QStringLiteral("повторное сканирование ничего не удаляет"));

    // Удаляем файл — сканер должен это заметить.
    QFile::remove(workDir.filePath(QStringLiteral("two.png")));
    const ScanResult third = scanner.scan();
    checkEqual(third.removedFiles, 1, QStringLiteral("удалённый файл убран из базы"));

    if (ffmpegPath().isEmpty()) {
        QTextStream(stdout) << "  (ffmpeg не найден — проверка превью пропущена)\n";
        return;
    }

    // Ждём завершения очереди превью.
    QSignalSpy readySpy(&thumbnails, &ThumbnailGenerator::thumbnailReady);
    thumbnails.enqueue(db.mediaWithoutThumbnail());
    for (int i = 0; i < 200 && !thumbnails.isIdle(); ++i) {
        waitFor(100);
    }
    waitFor(100);

    check(thumbnails.isIdle(), QStringLiteral("очередь превью опустела"));
    checkEqual(readySpy.count(), 2, QStringLiteral("сгенерированы превью для двух оставшихся фото"));

    for (const MediaFile &media : db.mediaFiles()) {
        check(!media.thumbnailPath.isEmpty(), QStringLiteral("путь превью записан в базу: %1").arg(media.name));
        check(QFileInfo::exists(media.thumbnailPath), QStringLiteral("файл превью существует"));

        // Превью должно быть квадратным 400×400.
        QImage image(media.thumbnailPath);
        check(!image.isNull(), QStringLiteral("превью читается как изображение"));
        checkEqual(image.width(), 400, QStringLiteral("ширина превью"));
        checkEqual(image.height(), 400, QStringLiteral("высота превью"));

        // Повторная постановка не должна генерировать превью заново.
        checkEqual(media.thumbnailPath, paths.thumbnailPathFor(media.path),
                   QStringLiteral("имя превью совпадает с ожидаемым"));
    }

    // Удаление превью при удалении файла.
    const QString victimPath = workDir.filePath(QStringLiteral("one.jpg"));
    const QString victimThumb = paths.thumbnailPathFor(victimPath);
    QFile::remove(victimPath);
    scanner.scan();
    check(!QFileInfo::exists(victimThumb), QStringLiteral("превью удалённого файла стёрто"));
}

void testExportImport()
{
    section(QStringLiteral("Экспорт и импорт"));

    QTemporaryDir dir;
    CatalogDatabase db;
    check(db.open(dir.filePath(QStringLiteral("src.sqlite"))), QStringLiteral("исходная база открыта"));

    const QString mediaDir = dir.filePath(QStringLiteral("media"));
    QDir().mkpath(mediaDir);
    const QString photoPath = QDir(mediaDir).filePath(QStringLiteral("holiday.jpg"));
    writeJpeg(photoPath, 24, 24);

    const Catalog catalog = db.addCatalog(mediaDir);
    MediaFile file;
    file.id = QStringLiteral("m1");
    file.path = photoPath;
    file.name = QStringLiteral("holiday.jpg");
    file.kind = MediaKind::Photo;
    file.modifiedAt = 555000;
    file.catalogId = catalog.id;
    db.upsertMedia({file});

    const Tag tag = db.createTag(QStringLiteral("море"));
    db.applyTag(file.id, tag.id);

    const QString exportPath = dir.filePath(QStringLiteral("export.json"));
    QString error;
    check(dataTransfer::exportToFile(db, exportPath, &error), QStringLiteral("экспорт выполнен: %1").arg(error));
    check(QFileInfo::exists(exportPath), QStringLiteral("файл экспорта создан"));

    ImportSummary summary;
    check(dataTransfer::parseFile(exportPath, &summary, &error), QStringLiteral("файл экспорта разобран"));
    checkEqual(summary.catalogPaths.size(), 1, QStringLiteral("каталог в экспорте"));
    checkEqual(summary.tagNames.size(), 1, QStringLiteral("тег в экспорте"));
    checkEqual(summary.mediaTagPaths.size(), 1, QStringLiteral("связь тега в экспорте"));

    // Импорт в пустую базу.
    CatalogDatabase target;
    check(target.open(dir.filePath(QStringLiteral("dst.sqlite"))), QStringLiteral("целевая база открыта"));
    for (const QString &catalogPath : summary.catalogPaths) {
        target.addCatalog(catalogPath);
    }
    for (const QString &tagName : summary.tagNames) {
        target.createTag(tagName);
    }

    // Файл должен быть в базе до восстановления связей.
    MediaFile imported = file;
    imported.id = QStringLiteral("m2");
    target.upsertMedia({imported});
    target.applyImportedRelations(summary);

    checkEqual(target.tagCount(), 1, QStringLiteral("тег импортирован"));
    checkEqual(target.mediaTags(QStringLiteral("m2")).size(), 1, QStringLiteral("связь тега восстановлена по пути"));
    checkEqual(target.mediaTags(QStringLiteral("m2")).first().name, QStringLiteral("море"), QStringLiteral("имя импортированного тега"));

    // Битый файл не должен приниматься за валидный экспорт.
    const QString badPath = dir.filePath(QStringLiteral("bad.json"));
    QFile bad(badPath);
    bad.open(QIODevice::WriteOnly);
    bad.write("{ это не json");
    bad.close();
    check(!dataTransfer::parseFile(badPath, &summary, &error), QStringLiteral("битый файл отклонён"));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MediaCatalogTests"));

    QTextStream(stdout) << "Тесты ядра Media Catalog\n\n";

    testPathHash();
    testLocalPathConversion();
    testFormats();
    testMetaTags();
    testExifParsing();
    testDatabase();
    testFilter();
    testScannerAndThumbnails();
    testExportImport();

    QTextStream(stdout) << "\nПроверок: " << g_checks << ", провалов: " << g_failures << '\n';
    return g_failures == 0 ? 0 : 1;
}
