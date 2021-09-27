#include <QApplication>
#include <QQmlApplicationEngine>
#include "source/models/filesystemdirmodel.h"
#include "source/models/archivermodel.h"
#include "source/imageprovider/imageprovider.h"

void registerTypes() {
    qmlRegisterSingletonInstance("com.example.models", 1, 0, "FilesystemDirModel", FilesystemDirModel::instance());
    qmlRegisterSingletonInstance("com.example.models", 1, 0, "ArchiverModel", ArchiverModel::instance());
    qmlRegisterType<ArchiveBase>("com.example.enums", 1, 0, "ArchiverStates");
}

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication app(argc, argv);
    registerTypes();

    QQmlApplicationEngine engine;
    engine.addImageProvider("icons", new ImageProvider(app.style()));

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);
    engine.load(url);

    FilesystemDirModel::instance()->setCurrentDir(".");

    return app.exec();
}
