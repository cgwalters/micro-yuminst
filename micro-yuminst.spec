Name:           micro-yuminst
Summary:        A minimal implementation of `yum install` in C for Docker
Version:        1
Release:        1%{?dist}
License:        LGPLv2+
URL:            https://gitlab.com/cgwalters/micro-yuminst
Source0:        %{name}-%{version}.tar.xz
BuildRequires: autoconf automake libtool git
BuildRequires: pkgconfig(libhif)

%description
%{summary}

Only use this for Docker base images.

%prep
%setup -q

%build
env NOCONFIGURE=1 ./autogen.sh
%configure
%make_build

%install
%make_install
ln -s micro-yuminst $RPM_BUILD_ROOT/%{_bindir}/yum

%files
%doc README.md
%license COPYING
%{_bindir}/micro-yuminst
%{_bindir}/yum
