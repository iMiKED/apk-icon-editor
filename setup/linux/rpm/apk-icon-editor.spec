%global app_version 3.0.0-beta5
%global _missing_build_ids_terminate_build 0
%global debug_package %{nil}

Name:           apk-icon-editor-reborn
Version:        3.0.0
Release:        0.beta5%{?dist}
Summary:        Simple APK resource editor

License:        GPL-3.0-or-later
URL:            https://github.com/iMiKED/apk-icon-editor
Source0:        %{name}-%{app_version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  zlib-devel
Requires:       java-17-openjdk-devel
Requires:       hicolor-icon-theme

%description
APK Icon Editor Reborn is a cross-platform APK editor designed to edit and replace APK
resources such as icons, strings, images, application name, and version.

%prep
%autosetup -n %{name}-%{app_version}

%build
qmake6 PREFIX=%{_prefix} "DEFINES+=CI"
%make_build

%install
%make_install INSTALL_ROOT=%{buildroot}

%files
%license LICENSE
%doc README.md
%{_bindir}/apk-icon-editor-reborn
%{_bindir}/zipalign
%{_datadir}/apk-icon-editor-reborn/
%{_datadir}/applications/apk-icon-editor.desktop
%{_datadir}/icons/hicolor/*/apps/apk-icon-editor.png
%{_datadir}/icons/hicolor/*/mimetypes/application-vnd.android.package-archive.png

%changelog
* Wed Jun 10 2026 iMiKED <M-I-B@yandex.ru> - 3.0.0-0.beta5
- Add preview and export support for standalone XML launcher icons.

* Sat Jun 06 2026 iMiKED <M-I-B@yandex.ru> - 3.0.0-0.beta4.1
- Restore native macOS/Linux window chrome and fix Windows light theme restore.

* Fri May 22 2026 iMiKED <M-I-B@yandex.ru> - 3.0.0-0.beta4
- Continue beta work with configurable device presets and localization updates.

* Thu May 14 2026 iMiKED <M-I-B@yandex.ru> - 3.0.0-0.beta3
- Renamed application and binary package to APK Icon Editor Reborn.
- Added resource table alias support for adaptive XML launcher icons.

* Sun Apr 26 2026 iMiKED <M-I-B@yandex.ru> - 3.0.0-0.beta1
- Added RPM packaging for Fedora and Red Hat compatible distributions.
