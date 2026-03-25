QT       += core gui widgets

TARGET = Camera_Project
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/../common

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    v4l2_device.cpp

HEADERS += \
    mainwindow.h \
    v4l2_device.h
