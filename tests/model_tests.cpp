// Тесты слоя моделей: путь «файлы на диске → сканирование → превью →
// данные, которые получает QML».

#include <cstdio>

#include "app/MediaListModel.h"
#include "app/TagListModel.h"
#include "core/MediaLibrary.h"
#include "core/MetaTags.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
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
    fflush(stdout);
    ++g_checks;
    if (!condition) {
        ++g_failures;
        QTextStream(stderr) << "  ПРОВАЛ: " << what << '\n';
    }
}

void checkEqual(int actual, int expected, const QString &what)
{
    check(actual == expected, QStringLiteral("%1 (получено %2, ожидалось %3)")
                                  .arg(what).arg(actual).arg(expected));
}

void checkEqual(const QString &actual, const QString &expected, const QString &what)
{
    check(actual == expected, QStringLiteral("%1 (получено «%2», ожидалось «%3»)")
                                  .arg(what, actual, expected));
}

void waitFor(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

/// Ждёт, пока превью перестанут генерироваться (не дольше 60 секунд).
bool waitForThumbnails(MediaLibrary &library)
{
    for (int i = 0; i < 600; ++i) {
        if (!library.isGeneratingThumbnails()) {
            waitFor(150); // даём моделям обработать последний сигнал
            return true;
        }
        waitFor(100);
    }
    return false;
}

QString writePng(const QString &path, int size)
{
    QImage image(size, size, QImage::Format_RGB32);
    image.fill(Qt::darkMagenta);
    image.save(path, "PNG");
    return path;
}

/// Ждёт завершения сканирования, запущенного библиотекой.
bool waitForScan(MediaLibrary &library)
{
    for (int i = 0; i < 300; ++i) {
        if (!library.isScanning()) {
            waitFor(100);
            return true;
        }
        waitFor(100);
    }
    return false;
}


/// Регрессия: имя файла превью должно вычисляться из того же пути, который
/// хранится в базе. На macOS разыменование символических ссылок превращает
/// /tmp в /private/tmp: если формы пути расходятся, хэш имени превью не
/// совпадает с путём, по которому приложение ищет готовое превью, и генерация
/// выполняется повторно при каждом запуске.
void testPathFormConsistency(MediaLibrary &library, const QString &photosRoot)
{
    const QString linkRoot = QDir(QFileInfo(photosRoot).absolutePath())
                                 .filePath(QStringLiteral("photos-link"));
    QFile::remove(linkRoot);
    if (!QFile::link(photosRoot, linkRoot)) {
        QTextStream(stdout) << "  (ссылку создать не удалось — проверка пропущена)\n";
        return;
    }

    // Добавляем каталог через символическую ссылку: сканер увидит иную форму
    // пути, чем при добавлении напрямую.
    library.addCatalog(linkRoot);
    check(waitForScan(library), QStringLiteral("сканирование по ссылке завершилось"));

    // Ждём превью, чтобы проверить их пути.
    for (int i = 0; i < 600 && library.isGeneratingThumbnails(); ++i) {
        waitFor(100);
    }
    waitFor(200);

    const QList<MediaFile> media = library.database().mediaFiles();
    check(!media.isEmpty(), QStringLiteral("файлы найдены и по ссылке"));

    int checked = 0;
    for (const MediaFile &file : media) {
        // Ключевой инвариант: сохранённый путь превью совпадает с тем,
        // который вычисляется из пути файла.
        const QString expected = library.paths().thumbnailPathFor(file.path);
        if (file.thumbnailPath.isEmpty()) {
            continue;
        }
        ++checked;
        checkEqual(file.thumbnailPath, expected,
                   QStringLiteral("путь превью вычислен из пути файла (%1)").arg(file.name));
        check(QFileInfo::exists(file.thumbnailPath),
              QStringLiteral("файл превью существует (%1)").arg(file.name));
    }
    check(checked > 0, QStringLiteral("превью сгенерированы для файлов, найденных по ссылке"));

    // Повторное сканирование не должно ничего добавлять.
    const int before = library.mediaCount();
    library.requestInitialScan();
    check(waitForScan(library), QStringLiteral("повторное сканирование завершилось"));
    checkEqual(library.mediaCount(), before,
               QStringLiteral("повторное сканирование не задублировало файлы"));

    // Убираем только свой каталог: добавленный через ссылку.
    for (const QVariant &value : library.catalogs()) {
        const QVariantMap catalog = value.toMap();
        if (catalog.value(QStringLiteral("path")).toString() == linkRoot) {
            library.removeCatalog(catalog.value(QStringLiteral("id")).toString());
        }
    }
    QFile::remove(linkRoot);
}


/// Регрессия: диалоги Qt возвращают URL (`file:///…`), а не путь. Если передать
/// URL в QFileInfo, он понимается как относительный путь и превращается в
/// мусор вида «<рабочий каталог>/file:/…» — папка не находится и сканирование
/// молча ничего не делает.
void testCatalogFromUrl(MediaLibrary &library, const QString &photosRoot)
{
    // Берём отдельную подпапку, чтобы не пересекаться с уже добавленным
    // каталогом: одинаковый путь приложение считает дубликатом.
    const QString urlRoot = QDir(photosRoot).filePath(QStringLiteral("sub"));
    check(QFileInfo(urlRoot).isDir(), QStringLiteral("подпапка для проверки URL существует"));

    const QString url = QStringLiteral("file://") + urlRoot;
    library.addCatalog(url);
    check(waitForScan(library), QStringLiteral("сканирование после добавления по URL завершилось"));

    // Ищем именно свой каталог.
    QVariantMap entry;
    for (const QVariant &value : library.catalogs()) {
        const QVariantMap catalog = value.toMap();
        if (catalog.value(QStringLiteral("path")).toString() == urlRoot) {
            entry = catalog;
        }
    }

    check(!entry.isEmpty(), QStringLiteral("каталог добавлен по URL"));
    if (entry.isEmpty()) {
        return;
    }

    const QString stored = entry.value(QStringLiteral("path")).toString();
    checkEqual(stored, urlRoot, QStringLiteral("URL превращён в локальный путь"));
    check(!stored.contains(QStringLiteral("file:")), QStringLiteral("в пути нет схемы file:"));

    // И файлы должны найтись.
    check(library.mediaCount() > 0, QStringLiteral("файлы найдены после добавления по URL"));

    library.removeCatalog(entry.value(QStringLiteral("id")).toString());
    check(library.hasCatalogs(), QStringLiteral("основной каталог не затронут"));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MediaCatalogModelTests"));
    QTextStream(stdout) << "Тесты моделей Media Catalog\n\n";

    QTemporaryDir workDir;
    QTemporaryDir dataDir;
    check(workDir.isValid() && dataDir.isValid(), QStringLiteral("временные каталоги созданы"));

    // ==== Подготовка файлов ====
    QDir(workDir.path()).mkpath(QStringLiteral("sub"));
    writePng(workDir.filePath(QStringLiteral("a.png")), 64);
    writePng(workDir.filePath(QStringLiteral("b.png")), 48);
    writePng(workDir.filePath(QStringLiteral("sub/c.png")), 32);

    // ==== Инициализация библиотеки ====
    MediaLibrary library;
    check(library.initialize(dataDir.path()), QStringLiteral("библиотека инициализирована: %1")
                                                  .arg(library.lastError()));
    check(!library.hasCatalogs(), QStringLiteral("на старте каталогов нет"));
    checkEqual(library.mediaCount(), 0, QStringLiteral("на старте файлов нет"));

    MediaListModel mediaModel(&library);
    TagListModel tagModel(&library);
    TagListModel metaTagModel(&library);
    metaTagModel.setMetaMode(true);

    checkEqual(mediaModel.rowCount(), 0, QStringLiteral("модель медиа пуста"));

    // ==== Добавление каталога и сканирование ====
    library.addCatalog(workDir.path());
    check(library.hasCatalogs(), QStringLiteral("каталог добавлен"));
    check(waitForScan(library), QStringLiteral("сканирование завершилось"));

    checkEqual(library.mediaCount(), 3, QStringLiteral("найдены три файла"));
    checkEqual(mediaModel.rowCount(), 3, QStringLiteral("модель показывает три файла"));
    checkEqual(mediaModel.data(mediaModel.index(0, 0), MediaListModel::NameRole).toString().isEmpty()
                   ? 0 : 1,
               1, QStringLiteral("у первой строки есть имя"));

    // ==== Данные строки для полноэкранного просмотра ====
    // Просмотр берёт строку по индексу и читает её свойства, поэтому карта
    // должна содержать те же имена ролей, что и делегат сетки.
    {
        const QVariantMap row = mediaModel.get(0);
        check(!row.isEmpty(), QStringLiteral("get(0) возвращает данные"));
        check(row.contains(QStringLiteral("mediaId")), QStringLiteral("в карте есть mediaId"));
        check(row.contains(QStringLiteral("fileUrl")), QStringLiteral("в карте есть fileUrl"));
        check(row.contains(QStringLiteral("isVideo")), QStringLiteral("в карте есть isVideo"));
        check(row.contains(QStringLiteral("sizeText")), QStringLiteral("в карте есть sizeText"));
        check(row.value(QStringLiteral("fileUrl")).toString().startsWith(QStringLiteral("file:")),
              QStringLiteral("fileUrl — ссылка на файл"));
        check(mediaModel.get(-1).isEmpty(), QStringLiteral("get(-1) возвращает пустую карту"));
        check(mediaModel.get(mediaModel.rowCount()).isEmpty(),
              QStringLiteral("get за пределами списка возвращает пустую карту"));
    }

    // ==== Сортировка: сначала новые ====
    {
        auto modifiedAt = [&](int row) {
            return mediaModel.data(mediaModel.index(row, 0), MediaListModel::ModifiedAtRole).toLongLong();
        };
        check(modifiedAt(0) >= modifiedAt(1) && modifiedAt(1) >= modifiedAt(2),
              QStringLiteral("файлы отсортированы по дате изменения, новые сверху"));
    }

    // ==== Генерация превью и обновление модели ====
    if (library.isGeneratingThumbnails()) {
        check(waitForThumbnails(library), QStringLiteral("генерация превью завершилась"));
    } else {
        // Сканирование могло успеть поставить очередь превью и завершить её.
        waitFor(300);
    }

    int rowsWithThumbnail = 0;
    for (int row = 0; row < mediaModel.rowCount(); ++row) {
        const QString url = mediaModel.data(mediaModel.index(row, 0),
                                            MediaListModel::ThumbnailUrlRole).toString();
        if (!url.isEmpty()) {
            ++rowsWithThumbnail;
        }
    }
    checkEqual(rowsWithThumbnail, 3, QStringLiteral("модель получила превью для всех файлов"));

    // ==== Теги ====
    const QString firstId = mediaModel.data(mediaModel.index(0, 0), MediaListModel::IdRole).toString();
    const QString secondId = mediaModel.data(mediaModel.index(1, 0), MediaListModel::IdRole).toString();

    const QString tagId = library.createTag(QStringLiteral("Отпуск"));
    check(!tagId.isEmpty(), QStringLiteral("тег создан"));
    checkEqual(tagModel.rowCount(), 1, QStringLiteral("модель тегов показывает тег"));
    checkEqual(tagModel.data(tagModel.index(0, 0), TagListModel::NameRole).toString(),
               QStringLiteral("отпуск"), QStringLiteral("имя тега нормализовано"));
    checkEqual(library.createTag(QStringLiteral("отпуск")), tagId,
               QStringLiteral("повторное создание возвращает тот же тег"));

    library.applyTag(firstId, tagId);
    library.applyTag(secondId, tagId);
    checkEqual(tagModel.data(tagModel.index(0, 0), TagListModel::CountRole).toInt(), 2,
               QStringLiteral("счётчик применений тега"));

    // ==== Теги, которые использует полноэкранный просмотр ====
    {
        // Пользовательские теги файла — для чипов в просмотре.
        const QVariantList chips = library.userTags(firstId);
        checkEqual(chips.size(), 1, QStringLiteral("userTags возвращает теги файла"));
        if (!chips.isEmpty()) {
            const QVariantMap chip = chips.constFirst().toMap();
            check(chip.contains(QStringLiteral("id")), QStringLiteral("у чипа есть id"));
            check(chip.contains(QStringLiteral("name")), QStringLiteral("у чипа есть name"));
        }

        // Метатеги не должны попадать в пользовательские теги.
        for (const QVariant &value : chips) {
            const QString id = value.toMap().value(QStringLiteral("id")).toString();
            check(!id.startsWith(QStringLiteral("meta:")),
                  QStringLiteral("userTags не содержит метатегов"));
        }

        // Все теги — для автодополнения при вводе.
        const QVariantList all = library.allTags();
        check(!all.isEmpty(), QStringLiteral("allTags возвращает теги"));
        check(all.constFirst().toMap().contains(QStringLiteral("count")),
              QStringLiteral("у тега есть число применений"));

        // Применение тега должно быть видно сразу: чип в просмотре читает
        // userTags повторно, поэтому список обязан измениться без перехода
        // на другой файл.
        const QString probeTag = library.createTag(QStringLiteral("проверка-сразу"));
        const int before = library.userTags(firstId).size();
        library.applyTag(firstId, probeTag);
        const int after = library.userTags(firstId).size();
        checkEqual(after, before + 1,
                   QStringLiteral("тег появляется в userTags сразу после применения"));

        bool foundImmediately = false;
        for (const QVariant &value : library.userTags(firstId)) {
            if (value.toMap().value(QStringLiteral("id")).toString() == probeTag)
                foundImmediately = true;
        }
        check(foundImmediately, QStringLiteral("применённый тег сразу есть в списке"));

        library.removeTagFromMedia(firstId, probeTag);
        checkEqual(library.userTags(firstId).size(), before,
                   QStringLiteral("снятый тег сразу исчезает из списка"));

        // Добавление и снятие тега в фильтре — поведение клика по чипу.
        library.addTagToFilter(tagId);
        check(library.filterTagIds().contains(tagId),
              QStringLiteral("клик по тегу добавляет его в фильтр"));
        library.removeTagFromFilter(tagId);
        check(!library.filterTagIds().contains(tagId),
              QStringLiteral("снятие тега убирает его из фильтра"));
    }

    // ==== Фильтрация ====
    library.addTagToFilter(tagId);
    checkEqual(mediaModel.rowCount(), 2, QStringLiteral("фильтр по тегу оставил два файла"));

    library.clearFilter();
    checkEqual(mediaModel.rowCount(), 3, QStringLiteral("сброс фильтра вернул все файлы"));

    // Метатег «фото» должен отобрать все три файла.
    QString photoMetaTagId;
    for (const QVariant &value : library.metaTags()) {
        const QVariantMap map = value.toMap();
        if (map.value(QStringLiteral("id")).toString() == QStringLiteral("meta:type:фото")) {
            photoMetaTagId = map.value(QStringLiteral("id")).toString();
        }
    }
    check(!photoMetaTagId.isEmpty(), QStringLiteral("метатег «фото» присутствует"));
    library.addTagToFilter(photoMetaTagId);
    checkEqual(mediaModel.rowCount(), 3, QStringLiteral("метатег «фото» отобрал все фото"));

    // «Без тега» должен вытеснить обычные теги из фильтра.
    library.addTagToFilter(metaTags::untaggedId());
    const QStringList filterIds = library.filterTagIds();
    check(filterIds.contains(metaTags::untaggedId()), QStringLiteral("«без тега» добавлен в фильтр"));
    check(!filterIds.contains(tagId), QStringLiteral("обычный тег убран из фильтра"));

    // В режиме «любой» под условия подходят все фото; в режиме «все» —
    // только файл без тега.
    checkEqual(library.filterMode(), QStringLiteral("OR"), QStringLiteral("режим фильтра по умолчанию — любой"));
    checkEqual(mediaModel.rowCount(), 3, QStringLiteral("режим «любой»: подходит каждое фото"));

    library.setFilterMode(QStringLiteral("AND"));
    checkEqual(library.filterMode(), QStringLiteral("AND"), QStringLiteral("режим переключён на «все»"));
    checkEqual(mediaModel.rowCount(), 1, QStringLiteral("режим «все»: остался единственный файл без тега"));

    library.setFilterMode(QStringLiteral("OR"));

    library.clearFilter();

    // ==== Выделение ====
    mediaModel.selectAll();
    checkEqual(mediaModel.selectedCount(), 3, QStringLiteral("выделены все файлы"));
    checkEqual(mediaModel.selectedIds().size(), 3, QStringLiteral("список выделенных совпадает"));

    mediaModel.selectRange(firstId, secondId);
    checkEqual(mediaModel.selectedCount(), 2, QStringLiteral("Shift-выделение диапазона"));

    mediaModel.toggleSelection(firstId);
    checkEqual(mediaModel.selectedCount(), 1, QStringLiteral("повторный клик снимает выделение"));

    mediaModel.clearSelection();
    checkEqual(mediaModel.selectedCount(), 0, QStringLiteral("выделение снято"));

    // ==== Диапазон дат видимых карточек ====
    const QVariantMap range = mediaModel.dateRangeInRange(0, mediaModel.rowCount() - 1);
    check(range.value(QStringLiteral("min")).toLongLong()
              <= range.value(QStringLiteral("max")).toLongLong(),
          QStringLiteral("диапазон дат корректен"));

    // ==== Метатеги файла ====
    const QVariantList fileMetaTags = library.mediaMetaTags(firstId);
    checkEqual(fileMetaTags.size(), 3, QStringLiteral("у файла три метатега"));

    // ==== Добавление каталога по URL из диалога (регрессия) ====
    testCatalogFromUrl(library, workDir.path());

    // ==== Согласованность форм пути (регрессия) ====
    testPathFormConsistency(library, workDir.path());

    // ==== Удаление каталога ====
    // Каталог основного дерева должен быть на месте: регрессионные проверки
    // выше убирают только созданные ими каталоги.
    const QVariantList remainingCatalogs = library.catalogs();
    check(!remainingCatalogs.isEmpty(), QStringLiteral("каталог для удаления найден"));
    if (remainingCatalogs.isEmpty()) {
        return 1;
    }
    const QString catalogId = remainingCatalogs.constFirst().toMap()
                                  .value(QStringLiteral("id")).toString();
    library.removeCatalog(catalogId);
    checkEqual(library.mediaCount(), 0, QStringLiteral("удаление каталога очистило файлы"));
    checkEqual(mediaModel.rowCount(), 0, QStringLiteral("модель медиа опустела"));

    // Останавливаем фоновые задачи до выхода: это проверяет и корректность
    // завершения, и отсутствие обращений к базе после разрушения.
    library.shutdown();

    QTextStream(stdout) << "Проверок: " << g_checks << ", провалов: " << g_failures << '\n';
    return g_failures == 0 ? 0 : 1;
}
