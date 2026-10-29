QT += testlib network widgets

TARGET = test_version
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += test_version.cpp \
           ../../core/updater.cpp

HEADERS += ../../core/updater.h

DEFINES += VER=\\\"0\\\"
