dnl required version of autoconf
AC_PREREQ([2.53])

AC_INIT([gst-plugin-usb],[0.10.0])

dnl required versions of gstreamer and plugins-base
GST_REQUIRED=0.10.16
GSTPB_REQUIRED=0.10.16

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

dnl required version of automake
AM_INIT_AUTOMAKE([1.10])

dnl enable mainainer mode by default
AM_MAINTAINER_MODE([enable])

dnl check for tools (compiler etc.)
AC_PROG_CC

dnl required version of libtool
LT_PREREQ([2.2.6])
LT_INIT


dnl give error and exit if we don\'t have pkgconfig
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
  AC_MSG_ERROR([You need to have pkg-config installed!])
])

dnl Check for the required version of GStreamer core (and gst-plugins-base)
dnl This will export GST_CFLAGS and GST_LIBS variables for use in Makefile.am
dnl
PKG_CHECK_MODULES(GST, [
  gstreamer-0.10 >= $GST_REQUIRED
  gstreamer-base-0.10 >= $GST_REQUIRED
  gstreamer-dataprotocol-0.10 >= $GST_REQUIRED
], [
  AC_SUBST(GST_CFLAGS)
  AC_SUBST(GST_LIBS)
], [
  AC_MSG_ERROR([
      You need to install or upgrade the GStreamer development
      packages on your system. On debian-based systems these are
      libgstreamer0.10-dev and libgstreamer-plugins-base0.10-dev.
      on RPM-based systems gstreamer0.10-devel, libgstreamer0.10-devel
      or similar. The minimum version required is $GST_REQUIRED.
  ])
])



PKG_CHECK_MODULES(LIBUSB, [
  libusb-1.0
], [
  AC_SUBST(LIBUSB_CFLAGS)
  AC_SUBST(LIBUSB_LIBS)
], [
AC_MSG_ERROR([
      Check for libusb to be installed.
  ])
])

dnl check if compiler understands -Wall (if yes, add -Wall to GST_CFLAGS)
AC_MSG_CHECKING([to see if compiler understands -Wall])
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wall"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([ ], [ ])], [
  GST_CFLAGS="$GST_CFLAGS -Wall"
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])
])

dnl set the plugindir where plugins should be installed (for src/Makefile.am)
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-0.10/plugins"
else
  plugindir="\$(libdir)/gstreamer-0.10"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

dnl Erase this section if you are not interested in 
dnl allocating memory with CMEM

AC_ARG_WITH(dvsdk,
  [AS_HELP_STRING([--with-dvsdk=<path to dvsdk with Rules.make file>],
	[Path to a DVSDK with a Rules.make file (dvsdk 4 and up)])],
	[dvsdk=$withval]
)

if test "x$dvsdk" = "x" || ! test -f $dvsdk/Rules.make ; then
    AC_MSG_ERROR([Unable to find a valid Rules.make (DVSDK no provided or path is invalid)]);
else
    AC_MSG_NOTICE([Rules.make file found at: $dvsdk/Rules.make]);
fi

AC_SUBST([RULES_MAKE],["$dvsdk/Rules.make"])
AC_SUBST([PLATFORMDEFS],[platformdefs.inc])

dnl End of CMEM section

AC_OUTPUT

