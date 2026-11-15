QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
TARGET = ttly-serial

# Fallback for executables built outside the project tree.
TTLY_SOURCE_ROOT_PATH = $$quote($$clean_path($$PWD/../..))
DEFINES += $$shell_quote(TTLY_SOURCE_ROOT=\"$$TTLY_SOURCE_ROOT_PATH\")
HEADERS += ../common/runtimepaths.h
QT += core gui network
QT += core gui serialport gamepad
QT += core gui serialport
QT += multimedia
QT += concurrent
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    videotcpmainwindow.cpp \
    voicetcpmainwindow.cpp

HEADERS += \
    mainwindow.h \
    videotcpmainwindow.h \
    voicetcpmainwindow.h

FORMS += \
    mainwindow.ui \
    videotcpmainwindow.ui \
    voicetcpmainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=

RESOURCES += \
    res.qrc
