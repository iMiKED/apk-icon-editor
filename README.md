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
- [Qt 6.8.3 LTS](https://download.qt.io/archive/qt/6.8/6.8.3/) for building the 3.0 beta branch.
- [Apktool 3.0.2](https://bitbucket.org/iBotPeaches/apktool/downloads/apktool_3.0.2.jar) is bundled as `deploy/general/apktool.jar`.
#### Note for macOS users:
- Install a full JDK instead of a JRE so `java` and `javac` are available in `PATH`.

## Build
GitHub Actions can build release artifacts for Windows, Linux, Fedora/RHEL RPM, and macOS from `.github/workflows/build.yml`. Pushes and pull requests publish workflow artifacts; tag pushes matching `v*` also upload those artifacts to the matching GitHub Release.

#### Windows
- Install Qt 6.8.3 with the `msvc2022_64` component.
- Install Visual Studio 2022 Build Tools with the C++ workload.
- Run `qmake "DEFINES+=CI"` and then build with `nmake`.
- Optional installer packaging uses NSIS; see `setup/win32/README.txt`.

#### Linux
- Install Qt 6 development packages, `make`, `g++`, and zlib development headers.
- Run `qmake6 && make`.
- Debian packaging is driven by `setup/linux/deb/build.sh`.
- Fedora/RHEL RPM packaging is driven by `setup/linux/rpm/build.sh` and requires `rpm-build`, `qt6-qtbase-devel`, `qt6-qttools-devel`, `gcc-c++`, `make`, `zlib-devel`, and `java-17-openjdk-devel`.

#### macOS
- Install Qt 6.8.3 for macOS and Xcode 15 or newer.
- The build script uses `~/Qt/6.8.3/macos/bin` by default; set `QT_BIN=/path/to/Qt/bin` to override it.
- Run `setup/macosx/BUILD.command` to build and package the app bundle.

## Notice
- You may not use **APK Icon Editor** for any illegal purposes;
- The repacked APKs should not violate the original licenses.
