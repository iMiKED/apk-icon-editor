# APK Icon Editor

This repository is a fork of the original [APK Icon Editor](https://github.com/kefir500/apk-icon-editor) maintained by [iMiKED from 4PDA](https://4pda.to/forum/index.php?showuser=1017942).

[![Build](https://github.com/iMiKED/apk-icon-editor/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/iMiKED/apk-icon-editor/actions/workflows/build.yml)
[![CodeQL](https://github.com/iMiKED/apk-icon-editor/actions/workflows/github-code-scanning/codeql/badge.svg?branch=master)](https://github.com/iMiKED/apk-icon-editor/actions/workflows/github-code-scanning/codeql)
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

Adaptive/vector previews are intended for display, replacement, export, and safe APK repacking. The editor shows adaptive icon metadata in tooltips and logs, including the launcher entry, XML descriptor, foreground, background, monochrome layer when present, preview source, and write-back target.

`Write-back` means the real resource that will be changed before Apktool rebuilds the APK. Classic bitmap icons are written back to the selected PNG/WebP file. Adaptive XML icons are saved through an explicit foreground-only strategy: existing bitmap foreground layers are replaced in place, while vector/XML foregrounds get a generated custom bitmap foreground resource and the adaptive XML is updated to reference it. Background layers are preserved to avoid breaking the original adaptive icon structure.

Resource candidate diagnostics show how the editor chooses decoded APK resources. Candidates are scored by density and qualifiers, and the resource with the lowest `score` is selected.

## 3.0.0-beta2 Release Notes
Highlights:

- Added adaptive XML launcher icon support based on Apktool-decoded resources, without bundling `aapt2`.
- Added preview composition for adaptive foreground/background layers, including WebP, color, drawable XML shapes, layer-lists, insets, rotate wrappers, and common Android VectorDrawable foregrounds.
- Preferred ready bitmap resources with the same adaptive XML resource id when present, preserving app-provided launcher shape and scale.
- Added AOSP-style adaptive layer inset and oversampled preview composition for better scale and antialiasing.
- Added Save Icon, Replace Icon, Pack APK, and reopen support for complex adaptive/vector launcher icons.
- Added foreground-only adaptive icon write-back with clear tooltip/log metadata and confirmation text.
- Added `roundIcon`, launcher activity, and activity-alias launcher entry parsing with a stable display order.
- Added monochrome layer metadata for Android themed icons.
- Improved APK signing with bundled apksigner and explicit v1/v2/v3 schemes.
- Added build and packaging updates for Windows, Linux, Fedora/RHEL RPM, macOS Intel, and macOS Apple Silicon.

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
