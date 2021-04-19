# If the configure script is run, it will define the package version by
# prepending a definition of AC_VERSION to this spec.
# If rpkg preprocesses this file, as in a COPR build, then it will generate the
# package version from the "version" file and saved in the macro RPKG_VERSION. 
# This does not require running the configure script, which does not happen on
# COPR.

# It's also possible to define the macro 'version' on the command line.  The
# version used will be the first defined from 'version' macro, AC_VERSION, and
# then RPKG_VERSION.

# rpkg replaces with command output, otherwise rpmbuild replaces 3 {'s with 2 {'s.
%define RPKG_VERSION {{{ sed s/-/_/ version }}}
%define RPKG_SNAPINFO {{{ echo .$(git log -1 --date=format:"%Y%m%d" --format="%ad")git$(git rev-parse --short HEAD) }}}
%define have_rpkg %([ "%{RPKG_VERSION}" = "{{ sed s/-/_/ version }}" ]; echo $?)
%if 0%{!?version:1}
  %if %{have_rpkg}
    %define version %{RPKG_VERSION}
    %define snapinfo %{RPKG_SNAPINFO}
  %endif
  %{?AC_VERSION: %define version %{AC_VERSION}}
  %{!?version: %{error:Need to define version, e.g. --define "version x.y.z", or preprocess this file with configure or rpkg}}
%endif

# Define pkgrel when building to set release.  Otherwise git rev is used for snapshot tag.
%if 0%{!?pkgrel:1} && 0%{!?snapinfo:1}
%{warn: pkgrel not defined, attempting to build snapshot from current git checkout}
%{warn: Define pkgrel, e.g. --define "pkgrel 1", if not building git snapshot}
%define snapinfo .%(git log -1 --date=format:"%Y%m%d" --format="%ad")git%(git rev-parse --short HEAD)
%define needgit 1
%endif

Name: tg-timer
Version: %{version}
Release: %{?pkgrel}%{!?pkgrel:1}%{?snapinfo}%{?dist}
Summary: Mechanical watch movement timegrapher
License: GPL2
Group: Misc
URL: https://github.com/vacaboja/tg
Source: %name-%version.tar.gz
Packager: Trent Piepho <tpiepho@gmail.com>
BuildRequires: gcc, gtk3-devel, portaudio-devel, fftw-devel
BuildRequires: desktop-file-utils, autoconf, automake
%{?needgit:BuildRequires: git}

%description
Tg (tg-timer) is a program to evaluate the performance of mechanical watch
movements. Tg works with the noise produced by a watch mechanism, and it
produces real-time readings of the rate (or accuracy) and various other
operational parameters.

%prep
%setup -q

%build
autoreconf -fi
%configure
%make_build

%install
%make_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/*.desktop

%files
%license LICENSE
%doc README.md
%_bindir/%{name}
%_mandir/man1/%{name}.1*
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_datadir}/icons/hicolor/scalable/apps/%{name}.svg
%{_datadir}/icons/hicolor/*/mimetypes/application-x-%{name}-data.png
%{_datadir}/icons/hicolor/scalable/mimetypes/application-x-%{name}-data.svg
%{_datadir}/mime/packages/%{name}.xml

%changelog
* Sat Apr 03 2021 Trent Piepho <tpiepho at gmail.com> 1:0.5.2
- Initial version
