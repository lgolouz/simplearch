#ifndef DEPACKER_H
#define DEPACKER_H

#include <QObject>
#include "archivereader.h"

class Depacker : public ArchiveBase
{
    Q_OBJECT

    std::atomic_bool& m_cancelOperation;

    uint32_t prepareEntries(const QSharedPointer<ArchiveReader>& archiveReader, const QVector<ArchiveReader::FileInfo>& entries, QVector<ArchiveReader::FileInfo>& result);
    bool decompressFile(QFile& f, const ArchiveReader::FileInfo& entry, const QString& outPath);

public:
    explicit Depacker(QObject* parent = nullptr);
    explicit Depacker(std::atomic_bool& cancel, QObject* parent = nullptr);
    virtual ~Depacker() = default;

    void cancel();

public slots:
    void depack(QString depackDir, QString file, QList<ArchiveReader::FileInfo> entries);
    void depackFile(QString depackDir, QString file);

signals:
    void depackerStateChanged(ArchiveBase::ArchiverStates state);
    void overallProgress(quint32 current, quint32 whole);
    void fileProgress(QString fileName, quint32 current, quint32 whole);
    void scanningFilesystem(QString fileName);
};

#endif // DEPACKER_H
