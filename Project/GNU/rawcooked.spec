# Copyright (c) 2018 info@mediaarea.net
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.

# norootforbuild

%define rawcooked_version		18.01

Name:			rawcooked
Version:		%rawcooked_version
Release:		1
Summary:		(To be filled) (CLI)
Group:			Productivity/Multimedia/Other
License:		MIT
URL:			https://mediaarea.net/RAWCooked
Packager:		Jerome Martinez <info@mediaarea.net>
Source0:		rawcooked_%{version}-1.tar.gz
BuildRoot:		%{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: 	gcc-c++
BuildRequires:	pkgconfig
BuildRequires:  automake
BuildRequires:  autoconf
%if 0%{?suse_version}
BuildRequires:	update-desktop-files
%endif
%if 0%{?fedora_version}
BuildRequires:	desktop-file-utils
%if 0%{?fedora_version} == 99
BuildRequires:  gnu-free-sans-fonts
%endif
%endif
%if 0%{?mageia}
BuildRequires:  sane-backends-iscan
%if 0%{?mageia} >= 6
BuildRequires:  libproxy-pacrunner
%endif
BuildRequires:  libuuid-devel
%endif

%description
rawcooked CLI (Command Line Interface)

RAWCooked provides this service:

(To be filled)

%package gui
Summary:	(To be filled) (GUI)
Group:		Productivity/Multimedia/Other

%if 0%{?fedora_version} || 0%{?centos} >= 7
BuildRequires:  pkgconfig(Qt5)
%else
%if 0%{?mageia}
%ifarch x86_64
BuildRequires:  lib64qt5base5-devel
%else
BuildRequires:  libqt5base5-devel
%endif
%else
%if 0%{?suse_version} >= 1200
BuildRequires: libqt5-qtbase-devel
%else
BuildRequires: libqt4-devel
%endif
%endif
%endif

%if 0%{?rhel} >= 7
BuildRequires:  gnu-free-sans-fonts
%endif

%description gui
rawcooked GUI (Graphical User Interface)

RAWCooked provides this service:

(To be filled)

This package contains the graphical user interface

%prep
%setup -q -n rawcooked

%build
export CFLAGS="$RPM_OPT_FLAGS"
export CXXFLAGS="$RPM_OPT_FLAGS"

# build CLI
pushd Project/GNU/CLI
	%__chmod +x autogen
	./autogen
	%if 0%{?mageia} >= 6
		%configure --disable-dependency-tracking
	%else
		%configure
	 %endif

	%__make %{?jobs:-j%{jobs}}
popd

# now build GUI
pushd Project/Qt
	./prepare
	%__make %{?jobs:-j%{jobs}}
popd

%install
pushd Project/GNU/CLI
	%__make install DESTDIR=%{buildroot}
popd

pushd Project/Qt
	%__install -m 755 rawcooked-gui %{buildroot}%{_bindir}/rawcooked-gui
popd

# icon
%__install -dm 755 %{buildroot}%{_datadir}/icons/hicolor/128x128/apps
%__install -m 644 Source/Resource/Image/Icon.png \
	%{buildroot}%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
%__install -dm 755 %{buildroot}%{_datadir}/pixmaps
%__install -m 644 Source/Resource/Image/Icon.png \
	%{buildroot}%{_datadir}/pixmaps/%{name}.png

# menu-entry
%__install -dm 755 %{buildroot}/%{_datadir}/applications
%__install -m 644 Project/Qt/rawcooked-gui.desktop %{buildroot}/%{_datadir}/applications
%if 0%{?suse_version}
  %suse_update_desktop_file -n rawcooked-gui AudioVideo AudioVideoEditing
%endif
%__install -dm 755 %{buildroot}/%{_datadir}/apps/konqueror/servicemenus
%__install -m 644 Project/Qt/rawcooked-gui.kde3.desktop %{buildroot}/%{_datadir}/apps/konqueror/servicemenus/rawcooked-gui.desktop
%if 0%{?suse_version}
  %suse_update_desktop_file -n %{buildroot}/%{_datadir}/apps/konqueror/servicemenus/rawcooked-gui.desktop AudioVideo AudioVideoEditing
%endif
%__install -dm 755 %{buildroot}/%{_datadir}/kde4/services/ServiceMenus/
%__install -m 644 Project/Qt/rawcooked-gui.kde4.desktop %{buildroot}/%{_datadir}/kde4/services/ServiceMenus/rawcooked-gui.desktop
%__install -dm 755 %{buildroot}/%{_datadir}/kservices5/ServiceMenus/
%__install -m 644 Project/Qt/rawcooked-gui.kde4.desktop %{buildroot}/%{_datadir}/kservices5/ServiceMenus/rawcooked-gui.desktop
%if 0%{?suse_version}
  %suse_update_desktop_file -n %{buildroot}/%{_datadir}/kde4/services/ServiceMenus/rawcooked-gui.desktop AudioVideo AudioVideoEditing
  %suse_update_desktop_file -n %{buildroot}/%{_datadir}/kservices5/ServiceMenus/rawcooked-gui.desktop AudioVideo AudioVideoEditing
%endif

%clean
[ -d "%{buildroot}" -a "%{buildroot}" != "" ] && %__rm -rf "%{buildroot}"

%files
%defattr(-,root,root,-)
%doc License.html
%doc History_CLI.txt
%{_bindir}/rawcooked

%files gui
%defattr(-,root,root,-)
%doc License.html
%doc History_GUI.txt
%{_bindir}/rawcooked-gui
%{_datadir}/applications/*.desktop
%{_datadir}/pixmaps/*.png
%dir %{_datadir}/icons/hicolor
%dir %{_datadir}/icons/hicolor/128x128
%dir %{_datadir}/icons/hicolor/128x128/apps
%{_datadir}/icons/hicolor/128x128/apps/*.png
%dir %{_datadir}/apps
%dir %{_datadir}/apps/konqueror
%dir %{_datadir}/apps/konqueror/servicemenus
%{_datadir}/apps/konqueror/servicemenus/*.desktop
%dir %{_datadir}/kde4
%dir %{_datadir}/kde4/services
%dir %{_datadir}/kde4/services/ServiceMenus
%{_datadir}/kde4/services/ServiceMenus/*.desktop
%dir %{_datadir}/kservices5
%dir %{_datadir}/kservices5/ServiceMenus
%{_datadir}/kservices5/ServiceMenus/*.desktop

%changelog
* Mon Jan 01 2018 Jerome Martinez <info@mediaarea.net> - 18.01-0
- See History.txt for more info and real dates
