#ifndef PACKER_H
#define PACKER_H

#include <QFileInfoList>
#include <QList>
#include "archivebase.h"

class Packer : public ArchiveBase
{
    Q_OBJECT

    std::atomic_bool& m_cancelOperation;

    struct Entry {
        QString entryName;
        EntryTypes entryType;
        QFileInfo info;
    };

    struct RelativePathEntry {
        QString filesystemPath;
        QList<Entry> entries;
    };

    struct FileResult {
        uint32_t checksum;
        uint32_t fileSize;
        uint32_t compressedSize;
    };

    uint32_t prepareEntries(const QString& dirPath, const QFileInfoList& entries, QList<Packer::Entry>& result);

    template<typename T>
    void appendToBuf(QByteArray& buf, const T& t, uint32_t size = sizeof (T)) {
        buf.append(reinterpret_cast<const char *>(&t), size);
    }

    FileResult compressFile(/*QByteArray& buf*/QFile& outFile, const QFileInfo& entry, CompressionLevels level);

public:
    explicit Packer(std::atomic_bool& cancel, QObject* parent = nullptr);
    virtual ~Packer() = default;

public slots:
    void pack(QString archiveName, CompressionLevels level, QFileInfoList entries);

signals:
    void packerStateChanged(ArchiveBase::ArchiverStates state);
    void overallProgress(quint32 current, quint32 whole);
    void fileProgress(QString fileName, quint32 current, quint32 whole);
    void scanningFilesystem(QString fileName);
};

#endif // PACKER_H
