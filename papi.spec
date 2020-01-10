Summary: Performance Application Programming Interface
Name: papi
Version: 4.1.0
Release: 5%{?dist}
License: BSD
Group: Development/System
URL: http://icl.cs.utk.edu/papi/
Source0: http://icl.cs.utk.edu/projects/papi/downloads/%{name}-%{version}.tar.gz
Patch1: papi-westmere.patch
Patch21: papi-amd1.patch
Patch22: papi-amd2.patch
Patch23: papi-amd3.patch
Patch24: papi-amd4.patch
Patch25: papi-amd5.patch
Patch26: papi-amd6.patch
Patch27: papi-bz692668.patch
Patch28: papi-bz635667.patch
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: ncurses-devel
BuildRequires: gcc-gfortran
BuildRequires: kernel-headers >= 2.6.31
BuildRequires: chrpath
#Right now libpfm does not know anything about s390 and will fail
ExcludeArch: s390 s390x

%description
PAPI provides a programmer interface to monitor the performance of
running programs.

%package devel
Summary: Header files for the compiling programs with PAPI
Group: Development/System
Requires: papi = %{version}-%{release}
%description devel
PAPI-devel includes the C header files that specify the PAPI userspace
libraries and interfaces. This is required for rebuilding any program
that uses PAPI.

%prep
%setup -q
%patch1 -p0 -b .westmere
%patch21 -p1
%patch22 -p1
%patch23 -p1
%patch24 -p1
%patch25 -p1
%patch26 -p1
%patch27 -p1
%patch28 -p1

%build
cd src
%configure --with-static-lib=no --with-shared-lib=yes --with-shlib
#DBG workaround to make sure libpfm just uses the normal CFLAGS
DBG="" make

#%check
#cd src
#make fulltest

%install
rm -rf $RPM_BUILD_ROOT
cd src
make DESTDIR=$RPM_BUILD_ROOT install

chrpath --delete $RPM_BUILD_ROOT%{_libdir}/*.so*

# Remove the static libraries. Static libraries are undesirable:
# https://fedoraproject.org/wiki/Packaging/Guidelines#Packaging_Static_Libraries
rm -rf $RPM_BUILD_ROOT%{_libdir}/*.a

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_libdir}/*.so.*
/usr/share/papi
%doc INSTALL.txt README LICENSE.txt RELEASENOTES.txt

%files devel
%defattr(-,root,root,-)
%{_includedir}/*.h
%{_includedir}/perfmon
%{_libdir}/*.so
%doc %{_mandir}/man3/*
%doc %{_mandir}/man1/*

%changelog
* Thu Apr 20 2011 William Cohen <wcohen@redhat.com> - 4.1.0-5
- Correct AMD family 15H events. [635667]

* Wed Apr 13 2011 William Cohen <wcohen@redhat.com> - 4.1.0-4
- Correct number of counters for AMD family 15H. [692668]

* Thu Feb 11 2011 William Cohen <wcohen@redhat.com> - 4.1.0-3
- Add support for AMD Family 10H and 15H. [635667]

* Tue Jun 29 2010 William Cohen <wcohen@redhat.com> - 4.1.0-2
- Enable Intel westmere support. [608901]

* Tue Jun 29 2010 William Cohen <wcohen@redhat.com> - 4.1.0-1
- Rebase to papi-4.1.0 [608901]

* Fri May 21 2010 William Cohen <wcohen@redhat.com> - 4.0.0-6
- Resolves: rhbz594299 Preserve the CFLAGS from environment.

* Wed Feb 10 2010 William Cohen <wcohen@redhat.com> - 4.0.0-4
- Resolves: rhbz562935 Rebase to papi-4.0.0 (correct ExcludeArch).

* Wed Feb 10 2010 William Cohen <wcohen@redhat.com> - 4.0.0-3
- Resolves: rhbz562935 Rebase to papi-4.0.0 (bump nvr).

* Wed Feb 10 2010 William Cohen <wcohen@redhat.com> - 4.0.0-2
- correct the ctests/shlib test
- have PAPI_set_multiplex() return proper value
- properly handle event unit masks
- correct PAPI_name_to_code() to match events
- Resolves: rhbz562935 Rebase to papi-4.0.0 

* Wed Jan 13 2010 William Cohen <wcohen@redhat.com> - 4.0.0-1
- Generate papi.spec file for papi-4.0.0.
