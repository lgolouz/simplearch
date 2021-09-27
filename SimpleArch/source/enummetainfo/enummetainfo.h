#ifndef ENUMMETAINFO_H
#define ENUMMETAINFO_H

#include <QString>
#include <QMetaEnum>

class EnumMetaInfo final
{
protected:
    EnumMetaInfo() = default;
    ~EnumMetaInfo() = default;

public:
    template <typename T, typename O = QString> static O getEnumName(T state) {
        auto s = QMetaEnum::fromType<T>().valueToKey(static_cast<int>(state));
        return s ? s : "Undefined";
    }
};

#endif // ENUMMETAINFO_H
