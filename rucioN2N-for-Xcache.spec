Name:      xrootd-rucioN2N-for-Xcache
Version:   1.0
Release:   0%{?dist}
Summary:   Xrootd Name-to-Name plugin for Disk Caching Proxy (Xcache) to utilize RUCIO metalink
Group:     System Environment/Libraries
License:   BSD 
Vendor:    Stanford University/SLAC
URL:       https://github.com/wyang007/rucioN2N-for-Xcache

%define _rpmfilename %%{ARCH}/%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm

#Source0:   %{name}-src.%{version}.tar.gz
Source0:   rucioN2N-for-Xcache-%{version}.tar.gz
BuildArch: x86_64
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
#BuildRequires: xrootd-server-devel json-c-devel xrootd-client-devel
BuildRequires: xrootd-devel xrootd-server-devel xrootd-client-devel libcurl-devel

Requires: xrootd >= 4.7.0 xrootd-client libcurl

%description
rucioN2N-for-Xcache is a xrootd plugin module that will identify multiple copies of a distributed file based on ATLAS RUCIO data management system. Please refers to project's twiki in github (https://github.com/wyang007/rucioN2N-for-Xcache) for more info.

%prep
#%setup -q -c -D -T %{name} 
%setup -q -c %{name} 
%build
rm -rf %{buildroot}
pwd
ls
cd rucioN2N-for-Xcache-%{version}
# Caution !!!
# if you build along with the xrootd rpms, uncommand the folloing line and comment out the 2 lines after it
make
#make XRD_INC=/afs/slac/package/xrootd/xrootd-4.6.0/xrootd/src XRD_LIB=/afs/slac/package/xrootd/xrootd-4.6.0/@sys/src
#mv XrdName2NameDCP4RUCIO.so XrdName2NameDCP4RUCIO-4.so

%install
mkdir -p %{buildroot}/usr/lib64
# if you build along with the xrootd rpms, uncommand the folloing line and comment out the 1 lines after it
#cp xrootd-server-atlas-n2n-plugin/N2N/cPlusPlus/src/XrdOucName2NameLFC.so   %{buildroot}/usr/lib64/XrdOucName2NameLFC.so
cp rucioN2N-for-Xcache-%{version}/XrdName2NameDCP4RUCIO.so %{buildroot}/usr/lib64/XrdName2NameDCP4RUCIO.so

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
# if you build along with the xrootd rpms, uncommand the folloing line and comment out the 1 lines after it
#/usr/lib64/XrdOucName2NameLFC.so
/usr/lib64/XrdName2NameDCP4RUCIO.so

%post -p /sbin/ldconfig

%changelog 
* Tue Oct 3 2017 Wei Yang <yangw.slac.stanford.edu> v1.0, require xrootd 4.7, generate -4.so
- Tue Oct 3 2017 initial version
