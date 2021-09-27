#ifndef ARCHIVERMODEL_H
#define ARCHIVERMODEL_H

#include <QObject>
#include <QVariant>
#include <QFileInfoList>
#include <QScopedPointer>
#include <QThread>
#include "source/archiver/packer.h"
#include "source/archiver/depacker.h"
#include "source/archiver/archivereader.h"

class ArchiverModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(ArchiveBase::ArchiverStates archiverState READ getArchiverState NOTIFY archiverStateChanged)
    Q_PROPERTY(QVariantList compressionLevels READ getCompressionLevels CONSTANT)

    QThread m_packerThread;
    QThread m_depackerThread;
    std::atomic_bool m_cancelOperation;
    QScopedPointer<Packer> m_packerThreadObj;
    QScopedPointer<Depacker> m_depackerThreadObj;
    ArchiveBase::ArchiverStates m_archiverState;

public:
    explicit ArchiverModel(QObject* parent = nullptr);
    virtual ~ArchiverModel();

    QVariantList getCompressionLevels() const;
    ArchiveBase::ArchiverStates getArchiverState() const;
    void setArchiverState(ArchiveBase::ArchiverStates state);

    static ArchiverModel* instance();

    Q_INVOKABLE void decompressSelected(int row, QString archUrl, bool wholeArchive);
    Q_INVOKABLE void compressSelected(int row, QString archUrl, int level);
    Q_INVOKABLE void cancelOperation();

signals:
    void decompressFile(QString depackDir, QString archiveName);
    void decompressEntries(QString depackDir, QString file, QList<ArchiveReader::FileInfo> entries);
    void compressEntries(QString archName, ArchiveBase::CompressionLevels level, QFileInfoList entries);
    void archiverStateChanged();
    void overallProgress(quint32 current, quint32 whole);
    void fileProgress(QString fileName, quint32 current, quint32 whole);
    void scanningFilesystem(QString fileName);
};

#endif // ARCHIVERMODEL_H
