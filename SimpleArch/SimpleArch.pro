QT += quick testlib
QT += widgets # prerequesite to access to default system icons

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
        source/archiver/archivebase.cpp \
        source/archiver/archivereader.cpp \
        source/archiver/depacker.cpp \
        source/archiver/packer.cpp \
        source/imageprovider/imageprovider.cpp \
        source/models/archivermodel.cpp \
        source/models/filesystemdirmodel.cpp \
        source/enummetainfo/enummetainfo.cpp

RESOURCES += qml/qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

LIBS += $${OUT_PWD}/libs/libzlib.a

HEADERS += \
    source/archiver/archivebase.h \
    source/archiver/archivereader.h \
    source/archiver/depacker.h \
    source/archiver/packer.h \
    source/imageprovider/imageprovider.h \
    source/models/archivermodel.h \
    source/models/filesystemdirmodel.h \
    source/enummetainfo/enummetainfo.h
