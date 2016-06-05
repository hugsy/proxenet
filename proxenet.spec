#
# SPEC file for RPM-based distro build for proxenet
#
# @_hugsy_
#

%define name proxenet
%define version 0.4
%define release 1
%define author hugsy
%define author_email hugsy@blah.cat

%define target /opt/%{name}

Name: 		%{name}
Version: 	%{version}
Release: 	%{release}
Summary: 	Lightweight, fast, hacker-friendly proxy tool for penetration tests
Source0: 	https://github.com/hugsy/proxenet/archive/v%{version}.tar.gz
License: 	GPLv2
URL:		https://github.com/hugsy/proxenet
Group: 		Applications/Internet
BuildRoot: 	%{_tmppath}/%{name}-%{version}-buildroot
Requires: 	perl, python, ruby, lua, tcl, java-1.8.0-openjdk, mbedtls
BuildRequires:  perl-devel, python-devel, ruby-devel, tcl-devel, java-1.8.0-openjdk-devel, mbedtls-devel

%description
Plugin driven proxy for web application penetration tests, or Man-In-The-Middle attacks.
proxenet is a multi-threaded proxy which allows you to manipulate HTTP requests and responses
using your favorite scripting language (currently supports Python, Ruby, Java, Tcl, Lua, Perl).

%prep
rm -rf $RPM_BUILD_ROOT
%setup -q

%build
cmake . -DDEBUG=ON -DUSE_C_PLUGIN=ON -DUSE_PYTHON_PLUGIN=ON -DUSE_JAVA_PLUGIN=ON -DUSE_PERL_PLUGIN=ON -DUSE_LUA_PLUGIN=ON -DUSE_RUBY_PLUGIN=ON
make %{?_smp_mflags}

%make_install

%clean
rm -rf $RPM_BUILD_ROOT

%post

%files
%{_mandir}/man1/*
%{target}/*

%changelog
* Sun Jun 05 2016 hugsy <hugsy@blah.cat> 0.4-1
- Released version 0.4, first RPM packaged release
