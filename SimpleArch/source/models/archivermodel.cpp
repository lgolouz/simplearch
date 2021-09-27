#include "archivermodel.h"
#include "source/archiver/packer.h"
#include "source/models/filesystemdirmodel.h"
#include <QSemaphore>
#include <QUrl>

ArchiverModel::ArchiverModel(QObject* parent) :
    QObject(parent),
    m_cancelOperation(false),
    m_packerThreadObj(new Packer(m_cancelOperation)),
    m_depackerThreadObj(new Depacker(m_cancelOperation)),
    m_archiverState(ArchiveBase::ArchiverStates::PS_IDLE)
{
    qRegisterMetaType<QList<ArchiveReader::FileInfo>>("QList<ArchiveReader::FileInfo>");

    m_packerThread.setObjectName("Packer thread");
    m_packerThreadObj->moveToThread(&m_packerThread);
    m_depackerThread.setObjectName("Depacker thread");
    m_depackerThreadObj->moveToThread(&m_depackerThread);

    connect(this, &ArchiverModel::compressEntries, m_packerThreadObj.data(), &Packer::pack);
    connect(this, &ArchiverModel::decompressEntries, m_depackerThreadObj.data(), &Depacker::depack);
    connect(this, &ArchiverModel::decompressFile, m_depackerThreadObj.data(), &Depacker::depackFile);
    connect(m_packerThreadObj.data(), &Packer::packerStateChanged, this, &ArchiverModel::setArchiverState);
    connect(m_packerThreadObj.data(), &Packer::scanningFilesystem, this, &ArchiverModel::scanningFilesystem);
    connect(m_packerThreadObj.data(), &Packer::fileProgress, this, &ArchiverModel::fileProgress);
    connect(m_packerThreadObj.data(), &Packer::overallProgress, this, &ArchiverModel::overallProgress);
    connect(m_depackerThreadObj.data(), &Depacker::depackerStateChanged, this, &ArchiverModel::setArchiverState);
    connect(m_depackerThreadObj.data(), &Depacker::scanningFilesystem, this, &ArchiverModel::scanningFilesystem);
    connect(m_depackerThreadObj.data(), &Depacker::fileProgress, this, &ArchiverModel::fileProgress);
    connect(m_depackerThreadObj.data(), &Depacker::overallProgress, this, &ArchiverModel::overallProgress);

    QSemaphore initSem { 0 };
    connect(&m_packerThread, &QThread::started, m_packerThreadObj.data(), [&initSem]() { initSem.release(); });
    connect(&m_depackerThread, &QThread::started, m_depackerThreadObj.data(), [&initSem]() { initSem.release(); } );
    m_packerThread.start();
    m_depackerThread.start();

    initSem.acquire(2);
}

void ArchiverModel::cancelOperation() {
    m_cancelOperation = true;
}

void ArchiverModel::decompressSelected(int row, QString archUrl, bool wholeArchive) {
    QString archName { QUrl(archUrl).toLocalFile() };
    if (archName.isEmpty()) {
        return;
    }

    m_cancelOperation = false;
    const auto& inst { *FilesystemDirModel::instance() };
    const auto& list { inst.getFileInfoExList() };
    QString name;
    if (!inst.getBrowsingFilesystem()) {
        name = inst.getCurrentDir();
        const auto idx = name.indexOf('\\');
        name = idx < 1 ? name : name.left(idx);
    } else if (row >=0 && row < list.size()) {
        const auto* entryPtr = list.at(row).getQFileInfoPtr();
        if (entryPtr) {
            name = entryPtr->canonicalFilePath();
        }
    }
    if (wholeArchive) {
        if (ArchiveBase::isArchive(name)) {
            emit decompressFile(archName, name);
        }
    } else {
        QList<ArchiveReader::FileInfo> selectedEntries;
        for (const auto& e: list) {
            const auto* entryPtr = e.getArchiveFileInfoPtr();
            if (entryPtr && e.getSelected()) {
                selectedEntries.append(*entryPtr);
            }
        }
        if (selectedEntries.isEmpty() && row >=0 && row < list.size()) {
            const auto* entryPtr = list.at(row).getArchiveFileInfoPtr();
            if (entryPtr) {
                selectedEntries.append(*entryPtr);
            }
        }
        if (!selectedEntries.isEmpty() && !name.isEmpty()) {
            emit decompressEntries(archName, name, selectedEntries);
        }
    }
}

void ArchiverModel::compressSelected(int row, QString archUrl, int level) {
    QString archName { QUrl(archUrl).toLocalFile() };
    if (archName.isEmpty() || level < 0 || level > 9) {
        return;
    }

    m_cancelOperation = false;
    const auto& list { FilesystemDirModel::instance()->getFileInfoExList() };
    QFileInfoList selectedEntries;
    for (const auto& e: list) {
        const auto* entryPtr = e.getQFileInfoPtr();
        if (entryPtr && e.getSelected()) {
            selectedEntries.append(*entryPtr);
        }
    }
    if (selectedEntries.isEmpty() && row >=0 && row < list.size()) {
        const auto* entryPtr = list.at(row).getQFileInfoPtr();
        if (entryPtr) {
            selectedEntries.append(*entryPtr);
        }
    }
    if (!selectedEntries.isEmpty()) {
        ArchiveBase::CompressionLevels clevel = static_cast<ArchiveBase::CompressionLevels>(level);
        emit compressEntries(archName, clevel, selectedEntries);
    }
}

void ArchiverModel::setArchiverState(ArchiveBase::ArchiverStates state) {
    if (state != m_archiverState) {
        m_archiverState = state;
        emit archiverStateChanged();
    }
}

QVariantList ArchiverModel::getCompressionLevels() const {
    return QVariantList({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
}

ArchiveBase::ArchiverStates ArchiverModel::getArchiverState() const {
    return m_archiverState;
}

ArchiverModel* ArchiverModel::instance() {
    static ArchiverModel i;
    return &i;
}

ArchiverModel::~ArchiverModel() {
    m_packerThread.quit();
    m_packerThread.wait();
    m_depackerThread.quit();
    m_depackerThread.wait();
}
