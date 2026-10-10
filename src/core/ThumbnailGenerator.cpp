#include "core/ThumbnailGenerator.h"

#include "core/CatalogDatabase.h"
#include "core/MetadataReader.h"
#include "core/Paths.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>
#include <QTimer>

namespace mc {

namespace {

/// Размер стороны превью в пикселях — как в Electron-версии.
constexpr int kThumbnailSize = 400;

/// Таймаут на один файл: битые и «зависшие» медиа не должны блокировать пул.
constexpr int kProcessTimeoutMs = 120000;

/// Задача генерации одного превью. Выполняется в потоке пула и не трогает
/// базу — результат возвращается через сигнал в главный поток.
class ThumbnailTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ThumbnailTask(QString ffmpeg, QString inputPath, QString outputPath, MediaKind kind)
        : m_ffmpeg(std::move(ffmpeg))
        , m_inputPath(std::move(inputPath))
        , m_outputPath(std::move(outputPath))
        , m_kind(kind)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (QFileInfo::exists(m_outputPath)) {
            emit finished(true, m_outputPath);
            return;
        }
        if (!QFileInfo::exists(m_inputPath)) {
            emit finished(false, {});
            return;
        }

        // Кадр на 0.5-й секунде для видео, первый кадр для фото.
        // scale+crop даёт квадрат 400×400 без искажения пропорций.
        const QString filter = QStringLiteral(
            "scale=%1:%1:force_original_aspect_ratio=increase,crop=%1:%1")
                                   .arg(kThumbnailSize);

        QStringList arguments = {
            QStringLiteral("-hide_banner"),
            QStringLiteral("-loglevel"), QStringLiteral("error"),
            QStringLiteral("-y"),
            QStringLiteral("-threads"), QStringLiteral("1"),
        };
        if (m_kind == MediaKind::Video) {
            arguments << QStringLiteral("-ss") << QStringLiteral("0.5");
        }
        arguments << QStringLiteral("-i") << m_inputPath
                  << QStringLiteral("-vf") << filter
                  << QStringLiteral("-frames:v") << QStringLiteral("1")
                  << m_outputPath;

        QProcess process;
        QElapsedTimer timer;
        timer.start();
        process.start(m_ffmpeg, arguments);
        process.closeWriteChannel();

        if (!process.waitForStarted(10000)) {
            emit finished(false, {});
            return;
        }

        const bool finishedOk = process.waitForFinished(kProcessTimeoutMs);
        const int elapsed = static_cast<int>(timer.elapsed());

        // ffmpeg возвращает ненулевой код, если не удалось декодировать файл.
        if (finishedOk) {
            process.waitForFinished(1);
        } else {
            process.kill();
            process.waitForFinished(1000);
        }

        const bool success = finishedOk
            && process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0
            && QFileInfo::exists(m_outputPath)
            && QFileInfo(m_outputPath).size() > 0;

        if (!success) {
            // Частично записанный файл может остаться — убираем,
            // иначе он навсегда «залечит» превью битым изображением.
            QFile::remove(m_outputPath);
        }

        emit finished(success, success ? m_outputPath : QString());
    }

signals:
    void finished(bool success, const QString &outputPath);

private:
    QString m_ffmpeg;
    QString m_inputPath;
    QString m_outputPath;
    MediaKind m_kind;
};

} // namespace

ThumbnailGenerator::ThumbnailGenerator(CatalogDatabase &database, const AppPaths &paths, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_paths(paths)
    , m_ffmpeg(ffmpegPath())
{
    QDir().mkpath(m_paths.thumbnailsDir);

    m_pool = new QThreadPool(this);
    // Один ffmpeg на доступное ядро: внутри процесса -threads 1,
    // поэтому перегрузки системы не возникает.
    m_pool->setMaxThreadCount(concurrency());

    m_progressTimer = new QTimer(this);
    m_progressTimer->setSingleShot(true);
    m_progressTimer->setInterval(200);
    connect(m_progressTimer, &QTimer::timeout, this, &ThumbnailGenerator::emitProgress);
}

ThumbnailGenerator::~ThumbnailGenerator()
{
    stop();
}

void ThumbnailGenerator::stop()
{
    m_stopped = true;
    m_queue.clear();
    m_knownPaths.clear();
    if (m_pool) {
        m_pool->waitForDone();
    }
}

int ThumbnailGenerator::concurrency() const
{
    const int cores = QThread::idealThreadCount();
    return cores > 0 ? cores : 1;
}

bool ThumbnailGenerator::isIdle() const
{
    return m_activeCount == 0 && m_queue.isEmpty();
}

void ThumbnailGenerator::enqueue(const MediaFile &file)
{
    enqueue(QList<MediaFile>{file});
}

void ThumbnailGenerator::enqueue(const QList<MediaFile> &files)
{
    if (m_stopped || m_ffmpeg.isEmpty()) {
        return; // ffmpeg не найден — превью сгенерировать нечем
    }

    int added = 0;
    for (const MediaFile &file : files) {
        if (!file.thumbnailPath.isEmpty() || m_knownPaths.contains(file.path)) {
            continue;
        }
        Job job;
        job.mediaId = file.id;
        job.path = file.path;
        job.kind = file.kind;
        job.cachePath = m_paths.thumbnailPathFor(file.path);
        m_queue.enqueue(job);
        m_knownPaths.insert(file.path);
        ++added;
    }

    if (added == 0) {
        return;
    }

    if (m_activeCount == 0) {
        // Очередь была пуста — начинаем новый «сеанс» прогресса.
        m_total = m_queue.size();
        m_processed = 0;
    } else {
        m_total += added;
    }

    emitProgress();
    processQueue();
}

void ThumbnailGenerator::emitProgress()
{
    emit progress(ThumbnailProgress{m_total, m_processed});
}

void ThumbnailGenerator::processQueue()
{
    while (m_activeCount < m_pool->maxThreadCount() && !m_queue.isEmpty()) {
        const Job job = m_queue.dequeue();
        ++m_activeCount;
        startJob(job);
    }

    if (m_activeCount == 0 && m_queue.isEmpty() && m_total > 0) {
        m_total = 0;
        m_processed = 0;
        emitProgress();
    }
}

void ThumbnailGenerator::startJob(const Job &job)
{
    auto *task = new ThumbnailTask(m_ffmpeg, job.path, job.cachePath, job.kind);
    connect(task, &ThumbnailTask::finished, this, [this, job](bool success, const QString &outputPath) {
        if (success) {
            m_database.updateThumbnail(job.mediaId, outputPath);
            emit thumbnailReady(job.mediaId, outputPath);
        }
        finishJob(job, success);
    });
    m_pool->start(task);
}

void ThumbnailGenerator::finishJob(const Job &job, bool success)
{
    --m_activeCount;

    if (success) {
        m_knownPaths.remove(job.path);
        ++m_processed;
    } else {
        // Разрешаем ровно один повтор, дальше файл помечается безнадёжным.
        const bool retryAvailable = m_database.incrementThumbnailRetries(job.mediaId);
        if (retryAvailable) {
            m_queue.enqueue(job); // путь остаётся в m_knownPaths
        } else {
            m_knownPaths.remove(job.path);
            ++m_processed;
        }
    }

    // Троттлинг: не чаще одного сигнала в 200 мс.
    if (!m_progressTimer->isActive()) {
        m_progressTimer->start();
    }
    processQueue();
}

void ThumbnailGenerator::removeThumbnailsForPaths(const QSet<QString> &mediaPaths)
{
    for (const QString &path : mediaPaths) {
        QFile::remove(m_paths.thumbnailPathFor(path));
    }
}

} // namespace mc

#include "ThumbnailGenerator.moc"
