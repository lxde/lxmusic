dnl Process this file with autoconf to produce a configure script.

AC_INIT(lxmusic, 0.4.7, http://lxde.org/)
AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz])
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE([enable])

# Required Packages
m4_define([gtk_minimum_version], [2.20.0])
m4_define([gmodule_export_minimum_version], [2.0])
m4_define([xmms2_minimum_version], [0.7])
m4_define([xmms2_client_glib_minimum_version], [0.7])

# Optional Packages
m4_define([libnotify_minimum_version], [0.4.1])

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

AC_PROG_INTLTOOL(, [no-xml])

dnl check for mandatory modules
AC_ARG_ENABLE(gtk3,
AC_HELP_STRING([--enable-gtk3],[enable to use gtk-3.0 instead of gtk-2.0]),
[case "${enableval}" in
  yes)  enable_gtk3=yes ;;
  no)   enable_gtk3=no ;;
  *) AC_MSG_ERROR([bad value "${enableval}" for --enable-gtk3, use "yes" (default) or "no".]) ;;
esac],[])

# Checks for libraries.
if test "x$enable_gtk3" = "xyes" ; then
  CFLAGS="$CFLAGS -DENABLE_GTK3"
  gtk_modules="gtk+-3.0 >= 3.0.0"
else
  gtk_modules="gtk+-2.0 >= gtk_minimum_version"
fi
PKG_CHECK_MODULES(LXMUSIC,
        [$gtk_modules
	gmodule-export-2.0 >= gmodule_export_minimum_version
	xmms2-client >= xmms2_minimum_version
	xmms2-client-glib >= xmms2_client_glib_minimum_version])

dnl check for optional modules
LIBNOTIFY_CFLAGS=
LIBNOTIFY_LIBS=
PKG_CHECK_MODULES(LIBNOTIFY, libnotify >= libnotify_minimum_version, HAVE_LIBNOTIFY="yes", HAVE_LIBNOTIFY="no")
if test "x$HAVE_LIBNOTIFY" = "xyes"; then
	AC_DEFINE(HAVE_LIBNOTIFY, 1, [libnotify available])
	libnotify="libnotify"
else
	libnotify=""
fi

AC_SUBST(LIBNOTIFY_CFLAGS)
AC_SUBST(LIBNOTIFY_LIBS)

AM_CONDITIONAL(HAVE_LIBNOTIFY, test "x$HAVE_LIBNOTIFY" = "xyes")



GETTEXT_PACKAGE=lxmusic

AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

# Generate po/LINGUAS on the fly rather than relying on translators
# to maintain it manually. This also overcome the problem that Transifex
# cannot add a language to po/LINGUAS if a new po file is submitted.
rm -f $srcdir/po/LINGUAS
for po_file in `ls $srcdir/po/*.po | sort`;
do
    lang=`echo "$po_file" | sed "s|.*/po/\(.*\)\.po|\1|g"`
    echo $lang >> $srcdir/po/LINGUAS
done

AM_GLIB_GNU_GETTEXT

AC_OUTPUT([
Makefile
data/Makefile
src/Makefile
po/Makefile.in
])

