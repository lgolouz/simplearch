#ifndef ARCHIVEREADER_H
#define ARCHIVEREADER_H

#include "archivebase.h"
#include <QFile>
#include <QHash>
#include <QString>
#include <QMutex>

class ArchiveReader : public ArchiveBase
{
    Q_OBJECT

    QString m_currentFile;

public:
    class FileInfo
    {
        ArchEntry m_archEntry;
        QString m_fileName;

    public:
        FileInfo(const ArchEntry& e, const QString& fileName);
        FileInfo(ArchEntry&& e, QString&& fileName);
        virtual ~FileInfo() = default;
        FileInfo& operator= (const FileInfo& other);

        const ArchEntry& getArchEntry() const;
        const QString& getFileName() const;
    };

protected:
    std::atomic_bool& m_processingOperation;
    mutable QMutex m_mutex;
    QHash<QString, QVector<FileInfo>> m_archFilesystem;

    template<typename T>
    bool readData(QFile& f, T& t, int64_t size = sizeof (T)) {
        auto b = f.read(reinterpret_cast<char *>(&t), size);
        return b == size;
    }
    QString getDirName(const QString& fileName) const;

public:
    ArchiveReader(std::atomic_bool& processing, QObject* parent = nullptr);
    virtual ~ArchiveReader() = default;

    void cancel();

    void readArchive(const QString& fileName);
    const QVector<FileInfo>& getFileInfoList(const QString& archPath) const;
};

#endif // ARCHIVEREADER_H
