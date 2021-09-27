#ifndef ARCHIVEBASE_H
#define ARCHIVEBASE_H

#include <QObject>
#include <QString>
#include <cstdint>

#define SIGNATURE_SIZE 10
#define BYTES_TO_READ  1048576

class ArchiveBase : public QObject
{
    Q_OBJECT

public:
    enum class ArchiverStates {
        PS_IDLE = 0,
        PS_SCANNING_FILESYSTEM,
        PS_COMPRESSING,
        PS_DECOMPRESSING,
        PS_DECOMPRESSION_ERROR
    };
    Q_ENUMS(ArchiverStates)

    enum EntryTypes : uint8_t {
        ET_DIR,
        ET_FILE
    };

#pragma pack(push, 1)
    struct ArchEntry {
        uint8_t compression;
        uint8_t entry_type;
        uint64_t file_time;
        uint16_t file_permissions;
        uint32_t checksum;
        uint32_t compressed_size;
        uint32_t uncompressed_size;
        uint64_t payload_offset;
        uint16_t filename_length;
    };

protected:
    std::atomic_bool& getFakeAtomicBool();

    struct RootArchEntry {
        uint32_t total_entries;
        uint32_t entries_size;
    };
#pragma pack(pop)

    static const uint8_t SIGNATURE[SIGNATURE_SIZE];

    static bool isSignatureValid(const QByteArray& ba);

public:
    explicit ArchiveBase(QObject* parent = nullptr);
    virtual ~ArchiveBase() = default;

    enum CompressionLevels : uint8_t {
        C_NO_COMPRESSION      = 0,
        C_LEVEL_1,
        C_LEVEL_2,
        C_LEVEL_3,
        C_LEVEL_4,
        C_LEVEL_5,
        C_LEVEL_6,
        C_LEVEL_7,
        C_LEVEL_8,
        C_LEVEL_9,
        C_BEST_SPEED          = C_LEVEL_1,
        C_BEST_COMPRESSION    = C_LEVEL_9
    };

    static bool isArchive(const QString& filename);
};

#endif // ARCHIVEBASE_H
