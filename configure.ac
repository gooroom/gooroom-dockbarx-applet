dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_INIT([gooroom-dockbarx-applet], [0.1.0])
AC_PREREQ(2.63)

dnl ***********************
dnl Initialize automake ***
dnl ***********************
AM_INIT_AUTOMAKE([1.8 dist-xz no-dist-gzip foreign])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl *******************************
dnl *** Check for UNIX variants ***
dnl *******************************
AC_AIX()
AC_ISC_POSIX()
AC_MINIX()

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
AM_PROG_CC_C_O()
AC_PROG_LD()
AC_PROG_INSTALL()
IT_PROG_INTLTOOL([0.35.2])

dnl **********************
dnl Initialize libtool ***
dnl **********************
LT_PREREQ([2.4])
LT_INIT([disable-static])

dnl **********************************
dnl *** Check for standard headers ***
dnl **********************************
AC_HEADER_STDC()
AC_CHECK_HEADERS([stdlib.h string.h errno.h unistd.h])

dnl ******************************
dnl *** Check for i18n support ***
dnl ******************************
GETTEXT_PACKAGE=gooroom-dockbarx-applet
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext Package])
AM_GLIB_GNU_GETTEXT

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
GTK_REQUIRED=3.20.0
GLIB_REQUIRED=2.44.0
LIBGNOME_PANEL_REQUIRED=3.38.0
GIO_REQUIRED=2.54.1
GCONF_REQUIRED=3.2.6

PKG_CHECK_MODULES(GLIB, glib-2.0 >= $GLIB_REQUIRED)
PKG_CHECK_MODULES(GTK, gtk+-3.0 >= $GTK_REQUIRED)
PKG_CHECK_MODULES(GIO, gio-2.0 >= $GIO_REQUIRED)
PKG_CHECK_MODULES(GIO_UNIX, gio-unix-2.0)
PKG_CHECK_MODULES(JSON_C, json-c)
PKG_CHECK_MODULES(LIBGNOMEPANEL, libgnome-panel >= $LIBGNOME_PANEL_REQUIRED)

GNOME_PANEL_MODULES_DIR=`$PKG_CONFIG --variable=modulesdir libgnome-panel`
AC_SUBST([GNOME_PANEL_MODULES_DIR], [$GNOME_PANEL_MODULES_DIR])

AC_OUTPUT([
  Makefile
  src/Makefile
  po/Makefile.in
])
