#include "app/AppController.h"

#include "app/MediaListModel.h"
#include "app/TagListModel.h"
#include "core/MediaLibrary.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QLocale>

namespace mc {

AppController::AppController(MediaLibrary *library, QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_mediaModel(new MediaListModel(library, this))
    , m_tagModel(new TagListModel(library, this))
    , m_metaTagModel(new TagListModel(library, this))
{
    m_metaTagModel->setMetaMode(true);

    connect(m_mediaModel, &MediaListModel::selectionChanged,
            this, &AppController::updateSelectionState);
    connect(m_library, &MediaLibrary::libraryChanged, this, [this]() {
        const bool hasCatalog = m_library->hasCatalogs();
        if (hasCatalog != m_anyCatalog) {
            m_anyCatalog = hasCatalog;
            emit anyCatalogChanged();
        }
    });

    m_anyCatalog = m_library->hasCatalogs();
}

int AppController::selectedCount() const
{
    return m_mediaModel->selectedCount();
}

bool AppController::hasSelection() const
{
    return m_mediaModel->selectedCount() > 0;
}

bool AppController::anyCatalog() const
{
    return m_anyCatalog;
}

QString AppController::defaultCatalogDirectory() const
{
    return m_library->defaultCatalogDirectory();
}

QString AppController::applicationVersion() const
{
    return QCoreApplication::applicationVersion();
}

void AppController::updateSelectionState()
{
    const int count = m_mediaModel->selectedCount();
    if (count != m_selectedCount) {
        m_selectedCount = count;
        emit selectionChanged();
    }
}

void AppController::addCatalog(const QString &path)
{
    m_library->addCatalog(path);
}

void AppController::removeCatalog(const QString &catalogId)
{
    m_library->removeCatalog(catalogId);
}

QStringList AppController::selectedIds() const
{
    return m_mediaModel->selectedIds();
}

void AppController::applyTagToSelection(const QString &tagId)
{
    const QStringList ids = m_mediaModel->selectedIds();
    if (ids.isEmpty() || tagId.isEmpty()) {
        return;
    }
    m_library->applyTagToMedia(ids, tagId);
    m_mediaModel->clearSelection();
}

void AppController::createTagAndApplyToSelection(const QString &name)
{
    if (name.trimmed().isEmpty()) {
        return;
    }
    const QString tagId = m_library->createTag(name);
    applyTagToSelection(tagId);
}

void AppController::revealInFileManager(const QString &filePath)
{
    m_library->showInFolder(filePath);
}

void AppController::requestCaptureDate(const QString &mediaId) const
{
    m_library->requestCaptureDate(mediaId);
}

QString AppController::formatDateTime(qint64 timestampMs) const
{
    if (timestampMs <= 0) {
        return {};
    }
    return QLocale().toString(QDateTime::fromMSecsSinceEpoch(timestampMs),
                              QLocale::ShortFormat);
}

QString AppController::formatFileSize(qint64 bytes) const
{
    return QLocale().formattedDataSize(bytes);
}

bool AppController::transferData(bool exportMode, const QString &filePath)
{
    return m_library->transferData(exportMode, filePath);
}

void AppController::notifyExportDirectory(const QString &filePath)
{
    const QString directory = QFileInfo(filePath).absolutePath();
    if (directory == m_lastExportDirectory) {
        return;
    }
    m_lastExportDirectory = directory;
    emit lastExportDirectoryChanged();
}

} // namespace mc
