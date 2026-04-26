# APK Icon Editor

This repository is a fork of the original [APK Icon Editor](https://github.com/kefir500/apk-icon-editor) maintained by [iMiKED from 4PDA](https://4pda.to/forum/index.php?showuser=1017942).

[![Latest Release](https://img.shields.io/github/v/release/iMiKED/apk-icon-editor?include_prereleases&label=release)](https://github.com/iMiKED/apk-icon-editor/releases)
[![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](https://github.com/iMiKED/apk-icon-editor/blob/master/LICENSE)

## Description
**APK Icon Editor** is a cross-platform APK editor designed to easily edit and replace APK resources such as icons, strings, images, application name and version, etc. Written in C++/Qt.

## Features
- Edit, replace and extract APK icons;
- Edit application name, version and resources;
- Sign and optimize APK;
- Supported image formats: PNG, ICO, GIF, JPG, BMP;
- Size presets for devices: *Android*, *BlackBerry*, *Amazon Kindle Fire*;
- Cloud storage upload: *Dropbox*, *Google Drive*, *Microsoft OneDrive*;
- Multilingual interface.

## Requirements
#### Recommended:
- [JDK 64-bit 17](https://adoptium.net/temurin/releases/) (or later) or another current JDK available in `PATH`.
- [Qt 5.15.2](https://download.qt.io/archive/qt/5.15/5.15.2/) for building the 3.0 beta branch.
- [Apktool 3.0.2](https://bitbucket.org/iBotPeaches/apktool/downloads/apktool_3.0.2.jar) is bundled as `deploy/general/apktool.jar`.
#### Note for macOS users:
- Install a full JDK instead of a JRE so `java` and `javac` are available in `PATH`.

## Build
#### Windows
- Install Qt 5.15.2 with the MSVC 2019 component.
- Install Visual Studio 2019 Build Tools with the C++ workload.
- Run `qmake "DEFINES+=CI"` and then build with `nmake`.
- Optional installer packaging uses NSIS; see `setup/win32/README.txt`.

#### Linux
- Install Qt 5 development packages, `make`, `g++`, and zlib development headers.
- Run `qmake && make`.
- Debian packaging is driven by `setup/linux/deb/build.sh`.

#### macOS
- Install Qt 5.15.2 for clang and make sure `~/Qt/5.15.2/clang_64/bin` exists.
- Run `setup/macosx/BUILD.command` to build and package the app bundle.

## Notice
- You may not use **APK Icon Editor** for any illegal purposes;
- The repacked APKs should not violate the original licenses.
