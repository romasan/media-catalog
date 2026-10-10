#pragma once

#include "core/MediaTypes.h"
#include "core/Paths.h"

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>

QT_BEGIN_NAMESPACE
class QThreadPool;
class QTimer;
QT_END_NAMESPACE

namespace mc {

class CatalogDatabase;


/// Генерация превью 400×400 через ffmpeg.
///
/// Модель работы повторяет Electron-версию:
///   - пул процессов размером с число доступных ядер;
///   - каждый ffmpeg запускается с `-threads 1`, то есть параллельность
///     достигается числом процессов, а не многопоточностью внутри ffmpeg;
///   - имя файла превью — хэш пути (см. PathHash.h), поэтому кэш превью
///     совместим со старой версией приложения;
///   - упавший файл ретраится не более одного раза (счётчик в базе);
///   - уже существующий файл превью не генерируется повторно.
///
/// Объект живёт в главном потоке, поэтому обращения к базе безопасны.
class ThumbnailGenerator : public QObject
{
    Q_OBJECT

public:
    ThumbnailGenerator(CatalogDatabase &database, const AppPaths &paths, QObject *parent = nullptr);
    ~ThumbnailGenerator() override;

    /// Ставит файлы в очередь. Уже стоящие в очереди или имеющие превью
    /// пропускаются.
    void enqueue(const QList<MediaFile> &files);
    void enqueue(const MediaFile &file);

    /// Удаляет файлы превью для указанных путей.
    void removeThumbnailsForPaths(const QSet<QString> &mediaPaths);

    /// Число одновременно работающих ffmpeg-процессов.
    int concurrency() const;

    /// Останавливает очередь и дожидается завершения запущенных процессов.
    /// После вызова новые задачи не принимаются.
    void stop();

    bool isIdle() const;

signals:
    void progress(const mc::ThumbnailProgress &progress);
    void thumbnailReady(const QString &mediaId, const QString &thumbnailPath);

private:
    struct Job {
        QString mediaId;
        QString path;
        QString cachePath;
        MediaKind kind = MediaKind::Photo;
    };

    void processQueue();
    void startJob(const Job &job);
    void finishJob(const Job &job, bool success);
    void emitProgress();

    CatalogDatabase &m_database;
    AppPaths m_paths;
    QString m_ffmpeg;
    QThreadPool *m_pool = nullptr;
    QQueue<Job> m_queue;
    QSet<QString> m_knownPaths; ///< пути в очереди или в работе
    int m_activeCount = 0;
    int m_total = 0;
    int m_processed = 0;
    QTimer *m_progressTimer = nullptr;
    bool m_stopped = false;
};

} // namespace mc
