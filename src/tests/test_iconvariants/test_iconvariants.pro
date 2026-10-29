QT += widgets xml testlib
CONFIG += testcase console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_iconvariants
INCLUDEPATH += ../../apk ../../core ../../dialogs ../../widgets
SOURCES += test_iconvariants.cpp \
    ../../apk/apkfile.cpp ../../apk/adaptiveicon.cpp ../../apk/androidvectorrenderer.cpp \
    ../../apk/icon.cpp ../../apk/iconsmodel.cpp ../../apk/iconsproxy.cpp ../../apk/device.cpp \
    ../../apk/resourceref.cpp ../../apk/resourcearsc.cpp ../../apk/resourceresolver.cpp \
    ../../apk/manifest.cpp ../../apk/manifestmodel.cpp ../../apk/titlesmodel.cpp ../../apk/titlenode.cpp \
    ../../core/globals.cpp ../../dialogs/adaptivereplacementdialog.cpp ../../widgets/decorationdelegate.cpp
HEADERS += ../../apk/icon.h ../../apk/iconsmodel.h ../../apk/iconsproxy.h \
    ../../apk/device.h ../../apk/manifestmodel.h ../../apk/titlesmodel.h \
    ../../dialogs/adaptivereplacementdialog.h
