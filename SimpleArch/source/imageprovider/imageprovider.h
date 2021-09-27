#ifndef IMAGEPROVIDER_H
#define IMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <QStyle>

class ImageProvider : public QQuickImageProvider
{
    QStyle* m_style;

public:
    ImageProvider(QStyle* style);
    virtual ~ImageProvider() = default;

    virtual QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override;
};

#endif // IMAGEPROVIDER_H
