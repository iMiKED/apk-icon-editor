QT -= gui

TARGET = simplecrypt
TEMPLATE = lib
CONFIG += staticlib

SOURCES += simplecrypt.cpp
HEADERS += simplecrypt.h

# APK Icon Editor:

DESTDIR = $$PWD/../../bin

macx: QMAKE_MACOSX_DEPLOYMENT_TARGET = 12.0
