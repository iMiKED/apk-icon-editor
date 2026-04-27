# APK Icon Editor

This repository is a fork of the original [APK Icon Editor](https://github.com/kefir500/apk-icon-editor) maintained by [iMiKED from 4PDA](https://4pda.to/forum/index.php?showuser=1017942).

[![Latest Release](https://img.shields.io/github/v/release/iMiKED/apk-icon-editor?include_prereleases&label=release)](https://github.com/iMiKED/apk-icon-editor/releases)
[![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](https://github.com/iMiKED/apk-icon-editor/blob/master/LICENSE)

## Description
**APK Icon Editor** is a cross-platform APK editor designed to edit and replace APK resources such as icons, strings, images, application name and version. This fork modernizes the original Qt application for current build toolchains and adds beta support for adaptive XML launcher icons decoded by Apktool.

## Features
- Edit, replace and extract APK icons, including classic bitmap launcher icons and adaptive XML launcher icons;
- Preview adaptive icons composed from XML foreground and background resources;
- Render common Android VectorDrawable foreground layers used by adaptive icons;
- Edit application name, version and resources;
- Sign and optimize APK;
- Supported image formats: PNG, ICO, GIF, JPG, BMP, WebP;
- Parse application `roundIcon`, launcher activity aliases, and activity icon fallbacks;
- Size presets for devices: *Android*, *BlackBerry*, *Amazon Kindle Fire*;
- Cloud storage upload: *Dropbox*, *Google Drive*, *Microsoft OneDrive*;
- Multilingual interface.

## 3.0.0 Beta Status
The 3.0 beta branch moves the project to Qt 6.8 LTS, Apktool 3.0.2, and current platform packaging. Version `3.0.0-beta2` focuses on adaptive XML icon handling without requiring `aapt2`: resources are resolved from Apktool-decoded files, color backgrounds are supported, WebP launcher layers are loaded through Qt image plugins, and vector foreground previews are rendered in-app.

Adaptive/vector previews are currently intended for display and safe inspection. Unchanged rendered previews are not written back over XML resources.

## Release Artifacts
GitHub Actions are prepared to build the following packages:

- Windows x64 portable ZIP built with MSVC 2022 and Qt 6.8.
- Linux x86_64 portable `tar.gz`.
- Debian/Ubuntu `amd64` DEB package.
- Fedora/RHEL-compatible RPM package.
- macOS Intel x86_64 app ZIP and DMG.
- macOS Apple Silicon arm64 app ZIP and DMG.

## Requirements
#### Recommended:
- [JDK 64-bit 17](https://adoptium.net/temurin/releases/) (or later) or another current JDK available in `PATH`.
- [Qt 6.8.3 LTS](https://download.qt.io/archive/qt/6.8/6.8.3/) for building the 3.0 beta branch.
- [Apktool 3.0.2](https://bitbucket.org/iBotPeaches/apktool/downloads/apktool_3.0.2.jar) is bundled as `deploy/general/apktool.jar`.
#### Note for macOS users:
- Install a full JDK instead of a JRE so `java` and `javac` are available in `PATH`.

## Build
GitHub Actions can build release artifacts for Windows, Linux, Fedora/RHEL RPM, macOS Intel, and macOS Apple Silicon from `.github/workflows/build.yml`. Pushes and pull requests publish workflow artifacts; tag pushes matching `v*` also upload those artifacts to the matching GitHub Release.

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
- Install Android SDK Build Tools so the package includes a current 64-bit `zipalign`.
- The build script uses `~/Qt/6.8.3/macos/bin` by default; set `QT_BIN=/path/to/Qt/bin` to override it.
- Set `MACOS_ZIPALIGN=/path/to/zipalign` if `zipalign` is not available in `PATH`.
- Run `setup/macosx/BUILD.command` to build and package the app bundle.

## Notice
- You may not use **APK Icon Editor** for any illegal purposes;
- The repacked APKs should not violate the original licenses.
