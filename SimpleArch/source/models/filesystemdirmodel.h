#ifndef FILESYSTEMDIRMODEL_H
#define FILESYSTEMDIRMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include <QThread>
#include <QSharedPointer>
#include <QFileInfo>
#include "source/archiver/archivereader.h"

struct IFileInfoWrapper
{
    enum WrapperEntryType {
        FilesystemEntry,
        ArchiveEntry
    };

    IFileInfoWrapper(WrapperEntryType t);
    virtual ~IFileInfoWrapper() = default;

    virtual bool isDir() const = 0;
    virtual QString filename() const = 0;
    virtual QString canonicalFilePath() const = 0;
    virtual uint64_t size() const = 0;
    WrapperEntryType type() const;

private:
    WrapperEntryType m_type;
};

class QFileInfoWrapper : public IFileInfoWrapper
{
    QFileInfo m_fileInfo;

public:
    QFileInfoWrapper(QFileInfo&& fileInfo);
    virtual ~QFileInfoWrapper() = default;
    //QFileInfoWrapper& operator= (const QFileInfoWrapper& w) { m_fileInfo = w.m_fileInfo; return *this; }

    virtual bool isDir() const override;
    virtual QString filename() const override;
    virtual QString canonicalFilePath() const override;
    virtual uint64_t size() const override;
    const QFileInfo& getQFileInfo() const;
};

class ArchiveFileInfoWrapper : public IFileInfoWrapper
{
    ArchiveReader::FileInfo m_fileInfo;

public:
    ArchiveFileInfoWrapper(ArchiveReader::FileInfo&& fileInfo);
    virtual ~ArchiveFileInfoWrapper() = default;
    //ArchiveFileInfoWrapper& operator= (const ArchiveFileInfoWrapper& w) { m_fileInfo = w.m_fileInfo; return *this; }

    virtual bool isDir() const override;
    virtual QString filename() const override;
    virtual QString canonicalFilePath() const override;
    virtual uint64_t size() const override;
    const ArchiveReader::FileInfo& getFileInfo() const;
};

class QFileInfoEx
{
    QSharedPointer<IFileInfoWrapper> m_fileInfo;
    bool m_selected;

public:
    QFileInfoEx(QFileInfo&& fileInfo, bool selected = false);
    QFileInfoEx(ArchiveReader::FileInfo&& fileInfo, bool selected = false);
    virtual ~QFileInfoEx() = default;

    QFileInfoEx& operator= (const QFileInfoEx& fileInfo);
    const QFileInfo* getQFileInfoPtr() const;
    const ArchiveReader::FileInfo* getArchiveFileInfoPtr() const;

    void setSelected(bool selected);
    bool getSelected() const;

    bool isDir() const;
    QString fileName() const;
    QString canonicalFilePath() const;
    uint64_t size() const;
};

class ReadDirThread : public QObject
{
    Q_OBJECT

    std::atomic_bool m_reading;
    QSharedPointer<ArchiveReader> m_readArchive;
    QString m_archiveName;

    void readFilesystemDir(const QString& dirName);
    void readArchiveFilesystem(const QString& archive);
    bool isValidArchive(const QString& name);
    QString getFilePath(const QString& name);
    QString getArchivePath(const QString& name);

public:
    explicit ReadDirThread(QObject* parent = nullptr);
    virtual ~ReadDirThread();

    void readDir(const QString& dirName);
    QSharedPointer<ArchiveReader> getArchiveReader() const;

private slots:
    void readDirSlot(QString dirName);

signals:
    void readDirInProgress();
    void readDirCancelled();
    void readDirFinished(QList<QFileInfoEx> entries);
};

class FilesystemDirModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool browsingFilesystem READ getBrowsingFilesystem NOTIFY browsingFilesystemChanged)
    Q_PROPERTY(bool readingDirectory READ getReadingDirectory NOTIFY readingDirectoryChanged)
    Q_PROPERTY(QString currentDir READ getCurrentDir WRITE setCurrentDir NOTIFY currentDirChanged)
    Q_PROPERTY(QStringList drives READ getDrivesList CONSTANT)

    QThread m_readDirTaskThread;
    QScopedPointer<ReadDirThread> m_readDirThreadObj;
    QList<QFileInfoEx> m_dirEntries;
    bool m_readingDirectory;
    QString m_currentDir;
    bool m_browsingFilesystem;

    void setReadingDirectory(bool state);
    void setBrowsingFilesystem(bool state);

private slots:
    void onReadDirInProgress();
    void onReadDirCancelled();
    void onReadDirFinished(QList<QFileInfoEx> entries);

public:
    enum FilesystemDirRoles {
        DirEntryRole = Qt::UserRole + 1,
        EntryTypeRole,
        EntrySelectedRole,
        EntrySizeRole
    };
    Q_ENUM(FilesystemDirRoles)

    explicit FilesystemDirModel(QObject* parent = nullptr);
    virtual ~FilesystemDirModel();
    static FilesystemDirModel* instance();

    //Sets the dir to be present in model
    void setCurrentDir(const QString& dir);

    Q_INVOKABLE virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    Q_INVOKABLE virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE void toggleSelection(int row);
    Q_INVOKABLE void enterDirectory(int row);

    const QList<QFileInfoEx>& getFileInfoExList() const;

    virtual QHash<int,QByteArray> roleNames() const override;

    //getters
    bool getBrowsingFilesystem() const;
    bool getReadingDirectory() const;
    QString getCurrentDir() const;
    QStringList getDrivesList() const;
    QSharedPointer<ArchiveReader> getArchiveReader() const;

signals:
    void browsingFilesystemChanged();
    void readingDirectoryChanged();
    void currentDirChanged();

protected:
    virtual int columnCount(const QModelIndex &parent) const override;

    std::pair<int, QByteArray> getRolePair(FilesystemDirRoles r) const;
};

#endif // FILESYSTEMDIRMODEL_H
