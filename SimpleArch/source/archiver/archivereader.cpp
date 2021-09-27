#include "archivereader.h"
#include <QScopeGuard>
#include <QByteArray>
#include <QMutexLocker>

ArchiveReader::FileInfo::FileInfo(const ArchEntry& e, const QString& fileName) :
    m_archEntry(e),
    m_fileName(fileName)
{

}

ArchiveReader::FileInfo::FileInfo(ArchEntry&& e, QString&& fileName) :
    m_archEntry(std::move(e)),
    m_fileName(std::move(fileName))
{

}

const ArchiveReader::ArchEntry& ArchiveReader::FileInfo::getArchEntry() const {
    return m_archEntry;
}

const QString& ArchiveReader::FileInfo::getFileName() const {
    return m_fileName;
}

ArchiveReader::FileInfo& ArchiveReader::FileInfo::operator= (const FileInfo& other) {
    m_archEntry = other.m_archEntry;
    m_fileName = other.m_fileName;
    return *this;
}

ArchiveReader::ArchiveReader(std::atomic_bool& processing, QObject* parent) :
    ArchiveBase(parent),
    m_processingOperation(processing)
{

}

void ArchiveReader::cancel() {
    m_processingOperation = false;
}

QString ArchiveReader::getDirName(const QString& fileName) const {
    const auto pos { fileName.lastIndexOf('/') };
    return pos > 0 ? fileName.left(pos) : "./";
}

void ArchiveReader::readArchive(const QString& fileName) {
    if (m_currentFile == fileName || !m_processingOperation) {
        return;
    }

    QHash<QString, QVector<FileInfo>> archFilesystem;

    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    auto guard = qScopeGuard([&f]() { f.close(); });

    if (!m_processingOperation || !isSignatureValid(f.read(SIGNATURE_SIZE))) {
        return;
    }
    RootArchEntry root;
    auto res = readData(f, root);
    if (!m_processingOperation || !res) {
        return;
    }

    auto buf = f.read(root.entries_size);
    if (!m_processingOperation || buf.size() != static_cast<int64_t>(root.entries_size)) {
        return;
    }
    int64_t pos = 0;
    while (m_processingOperation && pos < buf.size()) {
        ArchEntry& entry = *(reinterpret_cast<ArchEntry*>(&pos[buf.data()]));
        pos += sizeof (ArchEntry);
        QByteArray name(&pos[buf.data()], entry.filename_length);
        QString fname { name };
        const auto dir { getDirName(fname) };
        auto it = archFilesystem.find(dir);
        if (it == archFilesystem.end()) {
            //Insert ".." entry first
            archFilesystem.insert(dir, { FileInfo( {0, ET_DIR, 0, 0, 0, 0, 0, 0, 0}, ".." ), FileInfo(entry, fname) });
        } else {
            it.value().append(FileInfo(entry, fname));
        }
        pos += entry.filename_length;
    }

    QMutexLocker lock(&m_mutex);
    m_archFilesystem = archFilesystem;
}

const QVector<ArchiveReader::FileInfo>& ArchiveReader::getFileInfoList(const QString& archPath) const {
    static const QVector<FileInfo> empty { FileInfo( {0, ET_DIR, 0, 0, 0, 0, 0, 0, 0}, ".." ) };
    QMutexLocker lock(&m_mutex);
    const auto it = m_archFilesystem.find(archPath);
    return it == m_archFilesystem.end() ? empty : *it;
}
