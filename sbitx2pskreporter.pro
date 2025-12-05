QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0



CONFIG += debug
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    pskrworker.cpp \
    pskrworker_sender.cpp\
    config.cpp \
    logger.cpp

HEADERS += \
    mainwindow.h \
    pskrworker.h \
    config.h \
    logger.h


FORMS += \
    mainwindow.ui

# LIBS += /usr/lib/x86_64-linux-gnu/libtelnet.so
#LIBS += /usr/lib/aarch64-linux-gnu/libtelnet.so
#LIBS += /Users/fabrizio/Syncthing/fab/Park/sbitx2pskreporter-cpp/libtelnet/libtelnet.so
#LIBS += /Users/fabrizio/Syncthing/fab/Park/sbitx2pskreporter-cpp/libtelnet/liblibtelnet.a
#INCLUDEPATH += /Users/fabrizio/Syncthing/fab/Park/sbitx2pskreporter-cpp/libtelnet/

PKGCONFIG += telnet

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

unix:!macx: LIBS += -ltelnet
macx: LIBS += -L$$PWD/../libtelnet/ -llibtelnet

macx: INCLUDEPATH += $$PWD/../libtelnet
macx: DEPENDPATH += $$PWD/../libtelnet

# Install the config file
cfg.path = /etc
cfg.files = etc/*
