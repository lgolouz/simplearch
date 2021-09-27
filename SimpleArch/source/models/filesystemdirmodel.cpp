#include "filesystemdirmodel.h"
#include "source/enummetainfo/enummetainfo.h"
#include <QDir>
#include <QDirIterator>
#include <QSemaphore>

IFileInfoWrapper::IFileInfoWrapper(WrapperEntryType t) :
    m_type(t)
{

}

IFileInfoWrapper::WrapperEntryType IFileInfoWrapper::type() const {
    return m_type;
}

QFileInfoWrapper::QFileInfoWrapper(QFileInfo&& fileInfo) :
    IFileInfoWrapper(FilesystemEntry),
    m_fileInfo(std::move(fileInfo))
{

}

bool QFileInfoWrapper::isDir() const {
    return m_fileInfo.isDir();
}

QString QFileInfoWrapper::filename() const {
    return m_fileInfo.fileName();
}

QString QFileInfoWrapper::canonicalFilePath() const {
    return m_fileInfo.canonicalFilePath();
}

uint64_t QFileInfoWrapper::size() const {
    return m_fileInfo.size();
}

const QFileInfo& QFileInfoWrapper::getQFileInfo() const {
    return m_fileInfo;
}

ArchiveFileInfoWrapper::ArchiveFileInfoWrapper(ArchiveReader::FileInfo&& fileInfo) :
    IFileInfoWrapper(ArchiveEntry),
    m_fileInfo(std::move(fileInfo))
{

}

bool ArchiveFileInfoWrapper::isDir() const {
    return m_fileInfo.getArchEntry().entry_type == ArchiveBase::ET_DIR;
}

QString ArchiveFileInfoWrapper::filename() const {
    const auto& fname { m_fileInfo.getFileName() };
    return fname.mid(fname.lastIndexOf('/') + 1);
}

QString ArchiveFileInfoWrapper::canonicalFilePath() const {
    return QString();
}

uint64_t ArchiveFileInfoWrapper::size() const {
    return m_fileInfo.getArchEntry().uncompressed_size;
}

const ArchiveReader::FileInfo& ArchiveFileInfoWrapper::getFileInfo() const {
    return m_fileInfo;
}

QFileInfoEx::QFileInfoEx(QFileInfo&& fileInfo, bool selected) :
    m_fileInfo(new QFileInfoWrapper(std::move(fileInfo))),
    m_selected(selected)
{

}

QFileInfoEx::QFileInfoEx(ArchiveReader::FileInfo&& fileInfo, bool selected) :
    m_fileInfo(new ArchiveFileInfoWrapper(std::move(fileInfo))),
    m_selected(selected)
{

}

QFileInfoEx& QFileInfoEx::operator= (const QFileInfoEx& fileInfo) {
    m_fileInfo = fileInfo.m_fileInfo;
    m_selected = fileInfo.getSelected();
    return *this;
}

const QFileInfo* QFileInfoEx::getQFileInfoPtr() const {
    return m_fileInfo->type() == IFileInfoWrapper::ArchiveEntry ? nullptr : &m_fileInfo.staticCast<QFileInfoWrapper>()->getQFileInfo();
}

const ArchiveReader::FileInfo* QFileInfoEx::getArchiveFileInfoPtr() const {
    return m_fileInfo->type() == IFileInfoWrapper::ArchiveEntry ? &m_fileInfo.staticCast<ArchiveFileInfoWrapper>()->getFileInfo() : nullptr;
}

void QFileInfoEx::setSelected(bool selected) {
    m_selected = selected;
}

bool QFileInfoEx::getSelected() const {
    return m_selected;
}

bool QFileInfoEx::isDir() const {
    return m_fileInfo->isDir();
}

QString QFileInfoEx::fileName() const {
    return m_fileInfo->filename();
}

QString QFileInfoEx::canonicalFilePath() const {
    return m_fileInfo->canonicalFilePath();
}

uint64_t QFileInfoEx::size() const {
    return m_fileInfo->size();
}

ReadDirThread::ReadDirThread(QObject* parent) :
    QObject(parent),
    m_reading(false),
    m_readArchive(QSharedPointer<ArchiveReader>::create(m_reading))
{
    //Registering metatype to be able to pass it via signal/slot mechanism
    qRegisterMetaType<QList<QFileInfoEx>>("QList<QFileInfoEx>");
}

void ReadDirThread::readFilesystemDir(const QString& dirName) {
    m_reading = true;

    //emit start-of-work signal
    emit readDirInProgress();

    bool reading = true;
    //Using QSharedPointer as QDirIterator has no assgnment operator
    QVector<QSharedPointer<QDirIterator>> dirIterators {
        { QSharedPointer<QDirIterator>::create(dirName, QStringList { ".." }) },
        { QSharedPointer<QDirIterator>::create(dirName, QDir::AllDirs | QDir::NoDotAndDotDot) },
        { QSharedPointer<QDirIterator>::create(dirName, QDir::Files) }
    };
    auto dirIt = QDir(dirName).absolutePath() == "/" ? std::next(dirIterators.begin()) : dirIterators.begin();
    QList<QFileInfoEx> entriesList;
    while (reading) {
        reading = m_reading;
        if ((*dirIt)->hasNext()) {
            (*dirIt)->next();
            entriesList.append(QFileInfoEx((*dirIt)->fileInfo()));
        } else {
            ++dirIt;
            if (dirIt == dirIterators.end()) {
                reading = false;
            }
        }
    }

    if (m_reading) {
        m_reading = false;
        //emit ready signal
        emit readDirFinished(entriesList);
    } else {
        //emit cancelled signal
        emit readDirCancelled();
    }
}

QString ReadDirThread::getFilePath(const QString& name) {
    const auto pos { name.indexOf('\\') };
    return pos > 0 ? name.left(pos) : name;
}

QString ReadDirThread::getArchivePath(const QString& name) {
    const auto pos { name.indexOf('\\') };
    return pos > 0 ? name.mid(pos + 1).replace('\\', '/') : "./";
}

void ReadDirThread::readArchiveFilesystem(const QString& archive) {
    m_reading = true;

    //emit start-of-work signal
    emit readDirInProgress();

    m_readArchive->readArchive(getFilePath(archive));
    const auto archPath { getArchivePath(archive) };

    bool reading = m_reading;
    auto list { m_readArchive->getFileInfoList(archPath) };
    QList<QFileInfoEx> entriesList;
    for (auto it = list.begin(); it != list.end() && reading; ++it) {
        entriesList.append(QFileInfoEx(std::move(*it)));
        reading = m_reading;
    }

    if (m_reading) {
        m_reading = false;
        //emit ready signal
        emit readDirFinished(entriesList);
    } else {
        //emit cancelled signal
        emit readDirCancelled();
    }
}

void ReadDirThread::readDirSlot(QString dirName) {
    QFileInfo f(dirName);
    if (f.isDir()) {
        readFilesystemDir(dirName);
    } else {
        readArchiveFilesystem(dirName);
    }
}

void ReadDirThread::readDir(const QString& dirName) {
    m_reading = false;
    QMetaObject::invokeMethod(this, [dirName, this]() { readDirSlot(dirName); }, Qt::QueuedConnection);
}

QSharedPointer<ArchiveReader> ReadDirThread::getArchiveReader() const {
    return m_readArchive;
}

ReadDirThread::~ReadDirThread() {

}

FilesystemDirModel::FilesystemDirModel(QObject* parent) :
    QAbstractListModel(parent),
    m_readDirThreadObj(new ReadDirThread()),
    m_readingDirectory(false),
    m_currentDir(QString()),
    m_browsingFilesystem(true)
{
    qRegisterMetaType<QFileInfoList>("QFileInfoList");

    connect(m_readDirThreadObj.data(), &ReadDirThread::readDirInProgress, this, &FilesystemDirModel::onReadDirInProgress);
    connect(m_readDirThreadObj.data(), &ReadDirThread::readDirCancelled, this, &FilesystemDirModel::onReadDirCancelled);
    connect(m_readDirThreadObj.data(), &ReadDirThread::readDirFinished, this, &FilesystemDirModel::onReadDirFinished);

    //Use the QSemaphore to block the executing thread until readDirTask thread will be started
    QSemaphore readDirThreadSem { 0 };

    m_readDirTaskThread.setObjectName("ReadDirThread");
    m_readDirThreadObj->moveToThread(&m_readDirTaskThread);
    //We have to execute lambda in m_readDirThreadObj thread to avoid deadlock on semaphore acquire call
    connect(&m_readDirTaskThread, &QThread::started, m_readDirThreadObj.get(), [&readDirThreadSem]() { readDirThreadSem.release(); });
    m_readDirTaskThread.start();

    //Wait for ReadDirThread started
    readDirThreadSem.acquire();
}

bool FilesystemDirModel::getBrowsingFilesystem() const {
    return m_browsingFilesystem;
}

bool FilesystemDirModel::getReadingDirectory() const {
    return m_readingDirectory;
}

void FilesystemDirModel::setReadingDirectory(bool state) {
    if (m_readingDirectory != state) {
        m_readingDirectory = state;
        emit readingDirectoryChanged();
    }
}

void FilesystemDirModel::setBrowsingFilesystem(bool state) {
    if (m_browsingFilesystem != state) {
        m_browsingFilesystem = state;
        emit browsingFilesystemChanged();
    }
}

void FilesystemDirModel::onReadDirInProgress() {
    setReadingDirectory(true);
}

void FilesystemDirModel::onReadDirCancelled() {
    setReadingDirectory(false);
}

void FilesystemDirModel::onReadDirFinished(QList<QFileInfoEx> entries) {
    setReadingDirectory(false);
    beginResetModel();
    m_dirEntries = entries;
    endResetModel();
}

void FilesystemDirModel::setCurrentDir(const QString& dir) {
    QFileInfo f(dir);
    const auto cpath { getBrowsingFilesystem() ? f.canonicalFilePath() : dir };
    if (m_currentDir != cpath) {
        m_currentDir = cpath;
        m_readDirThreadObj->readDir(dir);
        emit currentDirChanged();
    }
}

QString FilesystemDirModel::getCurrentDir() const {
    return m_currentDir;
}

QStringList FilesystemDirModel::getDrivesList() const {
    QStringList result;
    const auto drives { QDir::drives() };
    for (const auto& d: drives) {
        result.append(d.absolutePath());
    }
    return result;
}

QSharedPointer<ArchiveReader> FilesystemDirModel::getArchiveReader() const {
    return m_readDirThreadObj ? m_readDirThreadObj->getArchiveReader() : QSharedPointer<ArchiveReader>(nullptr);
}

std::pair<int, QByteArray> FilesystemDirModel::getRolePair(FilesystemDirRoles r) const {
    return { r, EnumMetaInfo::getEnumName<FilesystemDirRoles, QByteArray>(r) };
}

void FilesystemDirModel::toggleSelection(int row) {
    if (row >= 0 && row < rowCount()) {
        auto& item = m_dirEntries[row];
        if (row == 0 && item.isDir() && item.fileName() == "..") {
            return;
        }
        item.setSelected(!item.getSelected());
        emit dataChanged(createIndex(row, 0), createIndex(row, 0));
    }
}

void FilesystemDirModel::enterDirectory(int row) {
    if (row >= 0 && row < rowCount()) {
        auto& item = m_dirEntries[row];
        if (getBrowsingFilesystem()) {
            if (item.isDir()) {
                setCurrentDir(QString("%1/%2").arg(m_currentDir, item.fileName()));
            } else {
                const QString cfp { item.canonicalFilePath() };
                if (ArchiveBase::isArchive(cfp)) {
                    setBrowsingFilesystem(false);
                    setCurrentDir(cfp);
                }
            }
        } else {
            if (item.isDir()) {
                const auto fileName { item.fileName() };
                if (fileName == "..") {
                    const auto idx = m_currentDir.lastIndexOf('\\');
                    if (idx < 0) {
                        setBrowsingFilesystem(true);
                        setCurrentDir(QString("%1/%2").arg(m_currentDir, item.fileName()));
                    } else {
                        setCurrentDir(idx < 1 ? m_currentDir : m_currentDir.left(idx));
                    }
                } else {
                    setCurrentDir(QString("%1\\%2").arg(m_currentDir, item.fileName()));
                }
            }
        }
    }
}

const QList<QFileInfoEx>& FilesystemDirModel::getFileInfoExList() const {
    return m_dirEntries;
}

QHash<int, QByteArray> FilesystemDirModel::roleNames() const {
    return {
        { getRolePair(DirEntryRole) },
        { getRolePair(EntryTypeRole) },
        { getRolePair(EntrySelectedRole) },
        { getRolePair(EntrySizeRole) }
    };
}

int FilesystemDirModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    return m_dirEntries.size();
}

QVariant FilesystemDirModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rowCount()) {
        return { };
    }

    const auto& entry = m_dirEntries.at(index.row());
    QVariantMap entryMap { {"selected", entry.getSelected()} };
    switch (role) {
        case DirEntryRole:
            entryMap.insert(QVariantMap { { "fileName", entry.fileName() }, { "iconUri", entry.isDir() ? "image://icons/SP_DirIcon" : "image://icons/SP_FileIcon"} });
            break;

        case EntryTypeRole:
            entryMap.insert(QVariantMap { {"isDir", entry.isDir()} });
            break;

        case EntrySizeRole:
            entryMap.insert(QVariantMap { {"size", entry.isDir() ? QVariant(QString()) : QVariant(quint64(entry.size()))} });
            break;

        default:
            break;
    }

    return entryMap;
}

int FilesystemDirModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return 1;
}

FilesystemDirModel* FilesystemDirModel::instance() {
    static FilesystemDirModel m;
    return &m;
}

FilesystemDirModel::~FilesystemDirModel() {
    //We have to stop the thread and wait when it ends
    m_readDirTaskThread.quit();
    m_readDirTaskThread.wait();
}
