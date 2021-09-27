#include "depacker.h"
#include "source/models/filesystemdirmodel.h"
#include "zlib.h"
#include <QScopeGuard>
#include <QDateTime>
#include <QDir>
#include <QDebug>

Depacker::Depacker(std::atomic_bool& cancel, QObject* parent) :
    ArchiveBase(parent),
    m_cancelOperation(cancel)
{

}

void Depacker::cancel() {
    m_cancelOperation = true;
}

//Result is an overall entries count for root entry
uint32_t Depacker::prepareEntries(const QSharedPointer<ArchiveReader>& archiveReader, const QVector<ArchiveReader::FileInfo>& entries, QVector<ArchiveReader::FileInfo>& result) {
    uint32_t overall_count = 0;
    for (const auto& entry: entries) {
        if (m_cancelOperation) {
            break;
        }
        const QString entryFileName { entry.getFileName().mid(entry.getFileName().lastIndexOf('/') + 1) };
        result.append(entry);
        ++overall_count;
        emit scanningFilesystem(entryFileName);
        if (entry.getArchEntry().entry_type == ET_DIR) {
            overall_count += prepareEntries(archiveReader, archiveReader->getFileInfoList(entry.getFileName()), result);
        }
    }
    return overall_count;
}

bool Depacker::decompressFile(QFile& f, const ArchiveReader::FileInfo& entry, const QString& outPath) {
    const auto& name = entry.getFileName();
    emit fileProgress(name, 0, 0);
    QString dirName = name;
    bool exit = true;
    if (entry.getArchEntry().entry_type != ET_DIR) {
        const auto idx = name.lastIndexOf('/');
        dirName = idx < 1 ? QString() : name.left(idx);
        exit = false;
    }
    if (!dirName.isEmpty()) {
        QDir d;
        d.mkpath(outPath + dirName);
    }

    if (exit) {
        return true;
    }

    QFile o(outPath + entry.getFileName());
    if (!o.open(QIODevice::WriteOnly)) {
        return false;
    }

    const auto& archEntry = entry.getArchEntry();
    auto guard = qScopeGuard([&o, &archEntry]() {
        o.setPermissions(static_cast<QFileDevice::Permissions>(archEntry.file_permissions));
        o.setFileTime(QDateTime::fromSecsSinceEpoch(archEntry.file_time), QFileDevice::FileBirthTime);
        o.close();
    });
    if (archEntry.uncompressed_size == 0) {
        return true;
    }

    QByteArray fileBuf;
    QByteArray depackBuf(BYTES_TO_READ, Qt::Initialization::Uninitialized);

    f.seek(archEntry.payload_offset);
    fileBuf = f.read(qMin((uint32_t) BYTES_TO_READ, archEntry.compressed_size));
    uLong inBufLen = fileBuf.size();
    uLong depackBufLen = depackBuf.size();

    z_stream zlibstream;
    zlibstream.zalloc = Z_NULL;
    zlibstream.zfree = Z_NULL;
    zlibstream.opaque = Z_NULL;

    zlibstream.next_in = reinterpret_cast<z_const Bytef*>(fileBuf.data());
    zlibstream.avail_in = inBufLen;
    zlibstream.next_out = reinterpret_cast<Bytef*>(depackBuf.data());
    zlibstream.avail_out = depackBufLen;
    auto err = inflateInit(&zlibstream);
    if (err != Z_OK) {
        qDebug() << QString("deflateInit failed: %1").arg(err);
        return false;
    }

    auto inflateGuard = qScopeGuard([&zlibstream]() { inflateEnd(&zlibstream); });
    uint64_t bytesRead = inBufLen;
    do {
        if (m_cancelOperation) {
            return false;
        }
        //We use Z_NO_FLUSH always, as if we will use Z_FINISH, we have to ensure, that output buffer will be large enough to fit all the decompressed data left
        err = inflate(&zlibstream, Z_NO_FLUSH);
        if (err == Z_OK || err == Z_STREAM_END) {
            //One of the buffers run out, storing decompressed data
            auto decompressedSize = depackBufLen - zlibstream.avail_out;
            o.write(depackBuf.data(), decompressedSize);

            bool needRead = err == Z_OK;
            zlibstream.next_out = reinterpret_cast<Bytef*>(depackBuf.data());
            zlibstream.avail_out = depackBufLen;
            if (needRead && zlibstream.avail_in == 0 && bytesRead < archEntry.compressed_size) {
                fileBuf = f.read(qMin((uint64_t) BYTES_TO_READ, archEntry.compressed_size - bytesRead));
                inBufLen = fileBuf.size();
                zlibstream.next_in = reinterpret_cast<z_const Bytef*>(fileBuf.data());
                zlibstream.avail_in = inBufLen;
                bytesRead += inBufLen;
            }
            emit fileProgress(entry.getFileName(), bytesRead, archEntry.uncompressed_size);
        }
    } while (err == Z_OK);

    return archEntry.checksum == zlibstream.adler;
}

void Depacker::depackFile(QString depackDir, QString file) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    if (!depackDir.endsWith('/')) {
        depackDir += '/';
    }

    bool decompressionError = true;
    auto guard = qScopeGuard([&f, &decompressionError, this]() {
        f.close();
        if (decompressionError) {
            emit depackerStateChanged(ArchiverStates::PS_DECOMPRESSION_ERROR);
        } else {
            emit depackerStateChanged(ArchiverStates::PS_IDLE);
        }
    });
    if (!isSignatureValid(f.read(SIGNATURE_SIZE))) {
        decompressionError = false;
        return;
    }
    emit depackerStateChanged(ArchiverStates::PS_DECOMPRESSING);
    RootArchEntry rootEntry;
    if (f.read(reinterpret_cast<char *>(&rootEntry), sizeof (RootArchEntry)) != sizeof (RootArchEntry)) {
        return;
    }
    auto pos = f.pos();
    for (auto count = rootEntry.total_entries; count > 0; --count) {
        if (m_cancelOperation) {
            break;
        }

        f.seek(pos);
        ArchEntry entry;
        if (f.read(reinterpret_cast<char *>(&entry), sizeof (ArchEntry)) != sizeof (ArchEntry)) {
            return;
        }
        const auto fileName { f.read(entry.filename_length) };
        if (fileName.size() != entry.filename_length) {
            return;
        }
        pos = f.pos();
        bool result = decompressFile(f, { entry, fileName }, depackDir);
        qDebug() << "Decompressing" << fileName << result;
        emit overallProgress(rootEntry.total_entries - count + 1, rootEntry.total_entries);
        decompressionError &= result;
    }
}

void Depacker::depack(QString depackDir, QString file, QList<ArchiveReader::FileInfo> entries) {
    QVector<ArchiveReader::FileInfo> result;
    uint32_t numEntries = 0;
    const auto archiveReader { FilesystemDirModel::instance()->getArchiveReader() };
    if (archiveReader.isNull()) {
        return;
    }
    if (!depackDir.endsWith('/')) {
        depackDir += '/';
    }
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }

    emit depackerStateChanged(ArchiverStates::PS_SCANNING_FILESYSTEM);

    for (const auto& e: entries) {
        numEntries += prepareEntries(archiveReader, { e }, result);
    }

    emit depackerStateChanged(ArchiverStates::PS_DECOMPRESSING);
    emit overallProgress(0, numEntries);

    bool decompressionError = true;
    auto guard = qScopeGuard([&f, &decompressionError, this]() {
        f.close();
        if (decompressionError) {
            emit depackerStateChanged(ArchiverStates::PS_DECOMPRESSION_ERROR);
        } else {
            emit depackerStateChanged(ArchiverStates::PS_IDLE);
        }
    });

    uint32_t counter = 0;
    for (const auto& entry: entries) {
        if (m_cancelOperation) {
            break;
        }
        bool result = decompressFile(f, { entry.getArchEntry(), entry.getFileName() }, depackDir);
        qDebug() << "Decompressing" << entry.getFileName() << result;
        emit overallProgress(counter, numEntries);
        decompressionError &= result;
    }
}
