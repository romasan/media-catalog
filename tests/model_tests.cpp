// Тесты слоя моделей: путь «файлы на диске → сканирование → превью →
// данные, которые получает QML».

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

    // ==== Удаление каталога ====
    const QString catalogId = library.catalogs().first().toMap()
                                  .value(QStringLiteral("id")).toString();
    library.removeCatalog(catalogId);
    checkEqual(library.mediaCount(), 0, QStringLiteral("удаление каталога очистило файлы"));
    checkEqual(mediaModel.rowCount(), 0, QStringLiteral("модель медиа опустела"));

    QTextStream(stdout) << "\nПроверок: " << g_checks << ", провалов: " << g_failures << '\n';
    return g_failures == 0 ? 0 : 1;
}
