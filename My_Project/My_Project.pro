QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/../common

SOURCES += \
    camera_launch_plan.cpp \
    main.cpp \
    linkage_logic.cpp \
    mainwindow.cpp \
    sr501_async.cpp

HEADERS += \
    camera_launch_plan.h \
    linkage_logic.h \
    mainwindow.h \
    sr501_async.h

FORMS += \
    mainwindow.ui

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
