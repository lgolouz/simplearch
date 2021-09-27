#include "imageprovider.h"
#include <QIcon>
#include <QMetaEnum>
#include <QStyle>

ImageProvider::ImageProvider(QStyle* style) :
    QQuickImageProvider(QQuickImageProvider::Pixmap, QQmlImageProviderBase::ForceAsynchronousImageLoading),
    m_style(style)
{

}

QPixmap ImageProvider::requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) {
    Q_UNUSED(size)

    static const auto metaobject { QMetaEnum::fromType<QStyle::StandardPixmap>() };
    const auto val { static_cast<QStyle::StandardPixmap>(metaobject.keyToValue(id.toStdString().c_str())) };
    auto icon { m_style->standardIcon(val) };

    return icon.pixmap(requestedSize);
}
