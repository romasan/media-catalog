// Точка входа нативного приложения Media Catalog.

#include "app/AppController.h"
#include "core/MediaLibrary.h"

#include <QCommandLineParser>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MediaCatalog"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.0.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("MediaCatalog"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Media Catalog"));

    // Системный шрифт: в QML семейство не задаётся, а платформенное значение
    // по умолчанию может указывать на несуществующее имя вроде "Sans Serif",
    // из-за чего Qt каждый раз подбирает замену.
    QGuiApplication::setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Каталогизация фото и видео"));
    parser.addHelpOption();
    parser.addVersionOption();

    // Каталог данных можно переопределить — удобно для переносимой сборки
    // и для запуска нескольких копий с разными каталогами.
    const QCommandLineOption dataDirOption(
        QStringLiteral("data-dir"),
        QStringLiteral("Каталог данных (по умолчанию — системный каталог приложения)"),
        QStringLiteral("путь"));
    parser.addOption(dataDirOption);

    // Ключ для автоматизации: добавить каталог и запустить сканирование,
    // не открывая диалог. Полезен для скриптов.
    const QCommandLineOption addCatalogOption(
        QStringLiteral("add-catalog"),
        QStringLiteral("Добавить каталог и сразу запустить сканирование"),
        QStringLiteral("путь"));
    parser.addOption(addCatalogOption);
    parser.process(app);

    mc::MediaLibrary library;
    if (!library.initialize(parser.value(dataDirOption))) {
        // Без базы приложение бессмысленно: сообщаем и выходим.
        qCritical("%s", qPrintable(library.lastError()));
        return 1;
    }

    mc::AppController controller(&library);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("Library"), &library);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.loadFromModule("MediaCatalog", "Main");
    if (engine.rootObjects().isEmpty()) {
        qCritical("Не удалось загрузить интерфейс приложения");
        return 1;
    }

    // Сканирование запускаем после появления окна: пользователь сразу видит
    // прогресс, а не пустой экран.
    if (parser.isSet(addCatalogOption)) {
        library.addCatalog(parser.value(addCatalogOption));
    } else {
        library.requestInitialScan();
    }

    return app.exec();
}
