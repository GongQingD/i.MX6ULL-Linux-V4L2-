QT       += core gui widgets

TARGET = Camera_Project
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

# PXP硬件加速：添加内核头文件路径
INCLUDEPATH += /home/ladykaka/Linux/linux-imx-4.1.15-2.1.0-e48931b1-v2.8/include/uapi \
             /home/ladykaka/Linux/linux-imx-4.1.15-2.1.0-e48931b1-v2.8/include/


SOURCES += \
    main.cpp \
    mainwindow.cpp \
    v4l2_device.cpp \
    pxp_processor.cpp

HEADERS += \
    mainwindow.h \
    v4l2_device.h \
    pxp_processor.h \
    pxp_types.h
