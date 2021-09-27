#include "archivebase.h"
#include <cstring>
#include <QFile>

const uint8_t ArchiveBase::SIGNATURE[SIGNATURE_SIZE] = { 'S', 'i', 'm', 'p', 'l', 'e', 'A', 'r', 'c', 'h' };

ArchiveBase::ArchiveBase(QObject* parent) :
    QObject(parent)
{
    qRegisterMetaType<ArchiveBase::CompressionLevels>("ArchiveBase::CompressionLevels");
    qRegisterMetaType<ArchiveBase::ArchiverStates>("ArchiveBase::ArchiverStates");
}

bool ArchiveBase::isSignatureValid(const QByteArray &ba) {
    return ba.size() >= SIGNATURE_SIZE && memcmp(ba.data(), SIGNATURE, SIGNATURE_SIZE) == 0;
}

bool ArchiveBase::isArchive(const QString& filename) {
    QFile f(filename);
    if (!f.exists() || f.size() < SIGNATURE_SIZE || !f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const auto b = f.read(SIGNATURE_SIZE);
    f.close();
    return isSignatureValid(b);
}
