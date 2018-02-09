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
URL:			https://mediaarea.net/RAWcooked
Packager:		Jerome Martinez <info@mediaarea.net>
Source0:		rawcooked_%{version}-1.tar.gz
BuildRoot:		%{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: 	gcc-c++
BuildRequires:	pkgconfig
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:	dos2unix

%description
rawcooked CLI (Command Line Interface)

RAWcooked provides this service:

(To be filled)

%prep
%setup -q -n rawcooked
dos2unix     *.txt
%__chmod 644 *.html *.txt

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

%install
pushd Project/GNU/CLI
	%__make install DESTDIR=%{buildroot}
popd

%clean
[ -d "%{buildroot}" -a "%{buildroot}" != "" ] && %__rm -rf "%{buildroot}"

%files
%defattr(-,root,root,-)
%doc License.html
%doc History_CLI.txt
%{_bindir}/rawcooked
%{_mandir}/man1/rawcooked.*

%changelog
* Mon Jan 01 2018 Jerome Martinez <info@mediaarea.net> - 18.01-0
- See History.txt for more info and real dates
