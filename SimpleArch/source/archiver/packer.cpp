#include "packer.h"
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QThread>
#include <QDirIterator>
#include <cstring>
#include "zlib.h"

Packer::Packer(std::atomic_bool& cancel, QObject* parent) :
    ArchiveBase(parent),
    m_cancelOperation(cancel)
{

}

//Result is an overall entries size for root entry
uint32_t Packer::prepareEntries(const QString& dirPath, const QFileInfoList& entries, QList<Packer::Entry>& result) {
    uint32_t overall_size = 0;
    for (const auto& entry: entries) {
        if (m_cancelOperation) {
            break;
        }
        const QString entryFileName { entry.fileName() };
        const QString relPath { dirPath + entryFileName };
        result.append({ relPath, entry.isDir() ? EntryTypes::ET_DIR : EntryTypes::ET_FILE, entry});
        overall_size += sizeof (ArchEntry) + relPath.size(); //Entry size is a sizeof(ArchEntry) + length of file name
        emit scanningFilesystem(entryFileName);
        if (entry.isDir()) {
            QDir d(entry.canonicalPath() + "/" + entryFileName);
            overall_size += prepareEntries(relPath + "/", d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsFirst), result);
        }
    }
    return overall_size;
}

Packer::FileResult Packer::compressFile(/*QByteArray& buf*/QFile& outFile, const QFileInfo& entry, CompressionLevels level) {
    emit fileProgress(entry.fileName(), 0, 0);
    if (entry.isDir() || entry.size() == 0) {
        return {0, 0, 0};
    }

    QFile f(entry.canonicalFilePath());
    QByteArray fileBuf;
    QByteArray packBuf(BYTES_TO_READ, Qt::Initialization::Uninitialized);
    f.open(QIODevice::ReadOnly);
    fileBuf = f.read(BYTES_TO_READ);
    uLong inBufLen = fileBuf.size();
    uLong packBufLen = packBuf.size();
    uint32_t actualFileSize = f.size();

    z_stream zlibstream;
    zlibstream.zalloc = Z_NULL;
    zlibstream.zfree = Z_NULL;
    zlibstream.opaque = Z_NULL;

    zlibstream.next_in = reinterpret_cast<z_const Bytef*>(fileBuf.data());
    zlibstream.avail_in = inBufLen;
    zlibstream.next_out = reinterpret_cast<Bytef*>(packBuf.data());
    zlibstream.avail_out = packBufLen;
    auto err = deflateInit(&zlibstream, (int) level);
    if (err != Z_OK) {
        qDebug() << QString("deflateInit failed: %1").arg(err);
        return {0, 0, 0};
    }

    uint64_t bytesRead = inBufLen;
    uint32_t actualCompressedSize = 0;
    do {
        if (m_cancelOperation) {
            break;
        }
        int flush = inBufLen == BYTES_TO_READ && !f.atEnd() ? Z_NO_FLUSH : Z_FINISH;
        err = deflate(&zlibstream, flush);
        if (err == Z_OK || err == Z_STREAM_END) {
            //Input buffer runs out, storing compressed data
            auto compressedSize = packBufLen - zlibstream.avail_out;
            outFile.write(packBuf.data(), compressedSize);
            actualCompressedSize += compressedSize;

            bool needRead = err == Z_OK;
            if (needRead && zlibstream.avail_out == 0) {
                unsigned pending = 0;
                int bits = 0;
                err = deflatePending(&zlibstream, &pending, &bits);
                needRead = !pending && !bits;
            }

            zlibstream.next_out = reinterpret_cast<Bytef*>(packBuf.data());
            zlibstream.avail_out = packBufLen;
            if (needRead && zlibstream.avail_in == 0 && !f.atEnd()) {
                fileBuf = f.read(BYTES_TO_READ);
                inBufLen = fileBuf.size();
                zlibstream.next_in = reinterpret_cast<z_const Bytef*>(fileBuf.data());
                zlibstream.avail_in = inBufLen;
                bytesRead += inBufLen;
            }
            emit fileProgress(entry.fileName(), bytesRead, actualFileSize);
        }
    } while (err == Z_OK);

    deflateEnd(&zlibstream);
    f.close();
    qDebug() << "Compressing" << entry.fileName() << (err == Z_STREAM_END);
    return {zlibstream.adler, actualFileSize, actualCompressedSize};
}

void Packer::pack(QString archiveName, CompressionLevels level, QFileInfoList entries) {
        QVector<Packer::RelativePathEntry> result;
        uint32_t numEntries = 0;
        uint32_t entriesSize = 0;

        emit packerStateChanged(ArchiverStates::PS_SCANNING_FILESYSTEM);

        for (const auto& e: entries) {
            result.append({e.canonicalPath(), { }});
            auto& item = result.last();
            entriesSize += prepareEntries(QString(), { e }, item.entries);
            numEntries += item.entries.count();
        }

        emit packerStateChanged(ArchiverStates::PS_COMPRESSING);
        emit overallProgress(0, numEntries);

        QFile archive(archiveName);
        archive.open(QIODevice::WriteOnly);

        //Generate header
        QByteArray buf;
        appendToBuf(buf, SIGNATURE, SIGNATURE_SIZE);
        appendToBuf(buf, RootArchEntry { numEntries, entriesSize });
        uint64_t indexEntryOffset = buf.size();

        //Append entries buffer placeholder
        buf.append(entriesSize, 0);
        archive.write(buf);

        uint64_t lastPos = archive.pos();
        uint64_t currentEntry = 0;
        for (const auto& rootEntry: qAsConst(result)) {
            if (m_cancelOperation) {
                break;
            }

            for (const auto& packedEntry: rootEntry.entries) {
                if (m_cancelOperation) {
                    break;
                }

                uint64_t archEntryOffset = lastPos;
                buf.clear();
                auto compressResult = compressFile(archive, packedEntry.info, level);
                lastPos = archive.pos();
                archive.seek(indexEntryOffset);
                appendToBuf(buf, ArchEntry {
                                static_cast<uint8_t>(level),
                                static_cast<uint8_t>(packedEntry.entryType),
                                static_cast<uint64_t>(packedEntry.info.birthTime().currentSecsSinceEpoch()),
                                static_cast<uint16_t>(packedEntry.info.permissions()),
                                compressResult.checksum,
                                static_cast<uint32_t>(compressResult.compressedSize),
                                compressResult.fileSize,
                                archEntryOffset,
                                static_cast<uint16_t>(packedEntry.entryName.size())
                             } );
                appendToBuf(buf, *packedEntry.entryName.toStdString().c_str(), packedEntry.entryName.size());
                archive.write(buf);
                indexEntryOffset = archive.pos();
                archive.seek(lastPos);
                ++currentEntry;
                emit overallProgress(currentEntry, numEntries);
            }
        }

        archive.close();
        emit packerStateChanged(ArchiverStates::PS_IDLE);
}
