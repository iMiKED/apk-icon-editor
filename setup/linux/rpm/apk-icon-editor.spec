%global app_version 3.0.0-beta2
%global _missing_build_ids_terminate_build 0
%global debug_package %{nil}

Name:           apk-icon-editor
Version:        3.0.0
Release:        0.beta1%{?dist}
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
APK Icon Editor is a cross-platform APK editor designed to edit and replace APK
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
%{_bindir}/apk-icon-editor
%{_bindir}/zipalign
%{_datadir}/apk-icon-editor/
%{_datadir}/applications/apk-icon-editor.desktop
%{_datadir}/icons/hicolor/*/apps/apk-icon-editor.png
%{_datadir}/icons/hicolor/*/mimetypes/application-vnd.android.package-archive.png

%changelog
* Sun Apr 26 2026 iMiKED <M-I-B@yandex.ru> - 3.0.0-0.beta1
- Added RPM packaging for Fedora and Red Hat compatible distributions.
