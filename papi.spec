%bcond_without bundled_libpfm
%global papi4_version 4.1.3
%define papi_dirname papi-5.1.1
%define papidir %{_libdir}/%{papi_dirname}
Summary: Performance Application Programming Interface
Name: papi
Version: 5.1.1
Release: 5%{?dist}
License: BSD
Group: Development/System
URL: http://icl.cs.utk.edu/papi/
Source0: http://icl.cs.utk.edu/projects/papi/downloads/%{name}-%{version}.tar.gz
Source1: http://icl.cs.utk.edu/projects/papi/downloads/%{name}-%{papi4_version}.tar.gz
Patch11: papi-soname.patch
Patch100: papi4-cflags.patch
Patch101: papi4-cov1.patch
Patch102: papi4-cov2.patch
Patch103: papi4-make-parallel.patch
Patch200: papi-testsuite1.patch
Patch300: papi-shlib.patch
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: ncurses-devel
BuildRequires: gcc-gfortran
BuildRequires: kernel-headers >= 2.6.32
BuildRequires: chrpath
BuildRequires: lm_sensors-devel
%if %{without bundled_libpfm}
BuildRequires: libpfm-devel >= 4.3.0
BuildRequires: libpfm-static >= 4.3.0
%endif
# Following required for net component
BuildRequires: net-tools
# Following required for inifiband component
BuildRequires: libibmad-devel
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
PAPI-devel includes the C header files that specify the PAPI user-space
libraries and interfaces. This is required for rebuilding any program
that uses PAPI.

%package testsuite
Summary: Set of tests for checking PAPI functionality
Group: Development/System
Requires: papi = %{version}-%{release}
%description testsuite
PAPI-testuiste includes compiled versions of papi tests to ensure
that PAPI functions on particular hardware.

%package static
Summary: Static libraries for the compiling programs with PAPI
Group: Development/System
Requires: papi = %{version}-%{release}
%description static
PAPI-static includes the static versions of the library files for
the PAPI user-space libraries and interfaces.

%prep
%setup -q
%setup -q -D -T -b 1

%patch11 -p1 -b .soname

pushd ../papi-%{papi4_version}
%patch100 -p1 -b .cflags
%patch101 -p1 -b .cov1
%patch102 -p1 -b .cov2
%patch103 -p1 -b .par
popd

%patch200 -p1
%patch300 -p1

%build
%if %{without bundled_libpfm}
# Build our own copy of libpfm.
%global libpfm_config --with-pfm-incdir=%{_includedir} --with-pfm-libdir=%{_libdir}
%endif

cd src
%configure --with-perf-events \
%{?libpfm_config} \
--with-static-lib=yes --with-shared-lib=yes --with-shlib \
--with-components="appio coretemp example lmsensors lustre mx net rapl stealtime"
#components currently left out because of build configure/build issues
#--with-components="appio cuda vmware"

pushd components
#pushd cuda; ./configure; popd
pushd infiniband; %configure; popd
pushd lmsensors; \
 %configure --with-sensors_incdir=/usr/include/sensors \
 --with-sensors_libdir=%{_libdir}; \
 popd
#pushd vmware; ./configure; popd
popd

#DBG workaround to make sure libpfm just uses the normal CFLAGS
DBG="" make %{?_smp_mflags}

# configure/build papi4 also
pushd ../../papi-%{papi4_version}/src
%configure --with-static-lib=no --with-shared-lib=yes --with-shlib
#DBG workaround to make sure libpfm just uses the normal CFLAGS
DBG="" make
popd

%install
rm -rf $RPM_BUILD_ROOT
cd src
#install the newer version files in papidir to avoid conflict
make DESTDIR=$RPM_BUILD_ROOT/%{papidir} LDCONFIG=/bin/true install-all

#install the older papi-4.1.3 version for capatibility
pushd ../../papi-%{papi4_version}/src
make DESTDIR=$RPM_BUILD_ROOT LDCONFIG=/bin/true install
popd

# make sure that unversioned  libpapi.so found on shared lib search
mkdir $RPM_BUILD_ROOT%{_libdir}/papi-%{papi4_version}
ln -s ../libpapi.so.%{papi4_version}.0 $RPM_BUILD_ROOT%{_libdir}/papi-%{papi4_version}/libpapi.so

# include information about where the newer libpfm/libpapi libraries are
mkdir -p %{buildroot}/etc/ld.so.conf.d
echo "%{_libdir}/papi-%{papi4_version}" > $RPM_BUILD_ROOT/etc/ld.so.conf.d/papi-%{_arch}.conf
echo "%{papidir}/usr/lib" >> $RPM_BUILD_ROOT/etc/ld.so.conf.d/papi-%{_arch}.conf

# Make the papi-5.1.1 bin tools available
mv $RPM_BUILD_ROOT%{papidir}%{_mandir}/man1/* $RPM_BUILD_ROOT%{_mandir}/man1/.
mv $RPM_BUILD_ROOT%{papidir}/usr/share/papi/papi_events.csv $RPM_BUILD_ROOT/usr/share/papi/papi_events.csv
rm -rf $RPM_BUILD_ROOT%{papidir}%{_mandir}/man1
mv $RPM_BUILD_ROOT%{papidir}%{_bindir}/* $RPM_BUILD_ROOT%{_bindir}/.
rm -rf $RPM_BUILD_ROOT%{papidir}%{_bindir}
if [ "%{_lib}" == "lib64" ] ; then
mv $RPM_BUILD_ROOT%{papidir}%{_libdir} $RPM_BUILD_ROOT%{papidir}/usr/lib
fi

chrpath --delete $RPM_BUILD_ROOT%{_libdir}/*.so*
chrpath --delete $RPM_BUILD_ROOT%{papidir}/usr/lib/*.so.*

# Manually compress the papi-5.1.1 man pages
gzip $RPM_BUILD_ROOT%{papidir}%{_mandir}/man3/*


%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_libdir}/*.so.*
%{_libdir}/papi-%{papi4_version}
/usr/share/papi
%doc INSTALL.txt README LICENSE.txt RELEASENOTES.txt
%doc %{_mandir}/man1/*
%dir %{papidir}
%dir %{papidir}/usr
%dir %{papidir}/usr/share
%dir %{papidir}/usr/share/papi
%dir %{papidir}/usr/lib
%{papidir}/usr/lib/*.so.*
%dir %{papidir}/usr/share/man
%dir %{papidir}/usr/share/man/man3
%{_sysconfdir}/ld.so.conf.d/*

%files devel
%defattr(-,root,root,-)
%{_includedir}/*.h
%{_includedir}/perfmon/*.h
%{papidir}/usr/include
%{_libdir}/*.so
%{papidir}/usr/lib/*.so
%doc %{_mandir}/man3/*
%doc %{papidir}%{_mandir}/man3/*

%files testsuite
%defattr(-,root,root,-)
%{papidir}/usr/share/papi/run_tests*
%{papidir}/usr/share/papi/ctests
%{papidir}/usr/share/papi/ftests
%{papidir}/usr/share/papi/components
%{papidir}/usr/share/papi/testlib

%files static
%defattr(-,root,root,-)
%{_libdir}/*.a
%{papidir}/usr/lib/*.a

%changelog
* Fri Sep 13 2013 William Cohen <wcohen@redhat.com> - 5.1.1-5
- Include the papi-4.1.3 lib dir.

* Tue Aug 27 2013 William Cohen <wcohen@redhat.com> - 5.1.1-4
- Ensure that unversioned libpap.so papi-4.1.3 seen before papi-5.1.1 version.

* Fri Jul 12 2013 William Cohen <wcohen@redhat.com> - 5.1.1-3
 - rhbz884628: /ctests/shlib fails with Error: PAPI_get_shared_lib_info

* Wed Jul 10 2013 William Cohen <wcohen@redhat.com> - 5.1.1-2
  - rhbz740909:	rebuild fails using parallel build

* Tue Jun 25 2013 William Cohen <wcohen@redhat.com> - 5.1.1-1
- Rebase to 5.1.1 for:
  - rhbz726798: Support for Sandy Bridge in PAPI
  - rhbz785258: FP event on Sandy Bridge when HT enabled
  - rhbz743648: Support for access to various energy and perf registers via papi 
  - rhbz831751: Ivy Bridge: support PAPI 
  - rhbz740909: Allow parallel builds
  - rhbz785975: Correct use of apostrophes in PAPI documentation

* Thu Sep 08 2011 William Cohen <wcohen@redhat.com> - 4.1.3-3
- Fixes for problems found in static analysis. [736006]

* Thu Aug 25 2011 William Cohen <wcohen@redhat.com> - 4.1.3-2
- Correct CFLAGS use in build.  [705893]

* Thu Aug 25 2011 William Cohen <wcohen@redhat.com> - 4.1.3-1
- Rebase to 4.1.3. [705893]

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
