# Process this file with autoconf to produce a configure script.
dnl We use the new format of AC_INIT
dnl info --file=autoconf.info --node="Initializing configure" for more info
AC_INIT(Ayttm, 0.3.1, ayttm-users@lists.sourceforge.net)
AC_PREREQ(2.53)
AC_CONFIG_SRCDIR(src/main.c)
AM_INIT_AUTOMAKE([1.6 dist-bzip2 dist-zip])
RELEASE=38

# This is to check modules/core binary compat
# Bump it up when necessary only (query_callbacks change, struct change etc)
CORE_VERSION=14

AM_CONFIG_HEADER(config.h)

if test "x$host_alias" = "xmingw32"; then
    case `$CC --version | sed -e 's,\..*,.,' -e q` in
      2.)
        if $CC -v --help 2>/dev/null | grep fnative-struct >/dev/null; then
          msnative_struct='-fnative-struct'
        fi
        ;;
      *)
        if $CC -v --help 2>/dev/null | grep ms-bitfields >/dev/null; then
          msnative_struct='-mms-bitfields'
        fi
        ;;
    esac

	EB_CFLAGS="-mno-cygwin $msnative_struct -I/target/include -I/usr/include/mingw -I/usr/include/w32api -I/target/lib/glib-2.0/include -I/target/lib/gtk+/include -I/target/include/glib-2.0 -I/usr/local/include -I/opt/include -I../ -I/usr/include/w32api -I/usr/include -I../mingw32 -D_REENTRANT -Wall -g"
	EB_LDADD="$LDADD"
	EB_LIBS="-Wl,-subsystem,windows -Wl,--export-dynamic -L../libproxy -lproxy /target/lib/intl.lib /target/lib/iconv.lib ../libltdl/libltdl.a /target/lib/dirent.lib /target/lib/libgtk.dll.a /target/lib/libgdk.dll.a /target/lib/libglib-2.0.dll.a -lwinmm -lws2_32 -lwst -lmsvcrt"
fi

# Checks for programs.
AC_PROG_CC
AC_ISC_POSIX
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL

# add some platform-specific flags
case "$host" in
*-darwin*)
        CFLAGS="$CFLAGS -no-cpp-precomp"
        CXXFLAGS="$CXXFLAGS -no-cpp-precomp"
        ;;
esac

LIBS="$LIBS -L/usr/local/lib"

# Checks for libraries.
AC_CHECK_LIB([audiofile], [_af_ulaw2linear])
AC_CHECK_LIB([ltdl], [lt_dlopen])
AC_CHECK_LIB([pspell], [pspell_manager_check])

# Checks for header files.
AC_PATH_XTRA
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_CHECK_HEADERS([errno.h fcntl.h langinfo.h libintl.h limits.h locale.h stdlib.h string.h strings.h unistd.h iconv.h pspell/pspell.h])
if test "x$host_alias" != "xmingw32"; then
AC_CHECK_HEADERS([alloca.h argz.h arpa/inet.h malloc.h netdb.h netinet/in.h paths.h sgtty.h stddef.h sys/file.h sys/ioctl.h sys/param.h sys/poll.h sys/socket.h sys/time.h syslog.h termios.h utmp.h getopt.h])
else
AC_CHECK_HEADERS([sys/utsname.h])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_STRUCT_TM
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_ALLOCA
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([atexit bzero dcgettext dup2 getcwd gethostbyname gethostname gettimeofday getwd inet_ntoa isascii memchr memmove mempcpy memset mkdir modf munmap nl_langinfo pow select setlocale socket stpcpy strcasecmp strchr strcspn strdup strerror strncasecmp strpbrk strrchr strspn strstr strtol strtoul uname snprintf])
if test "x$host_alias" != "xmingw32"; then
AC_CHECK_FUNCS([__argz_count __argz_next __argz_stringify getopt_long getopt])
fi

ALL_LINGUAS="fr en_GB pt_BR"
AM_GNU_GETTEXT

###############################################################################
#  Check for pthreads
###############################################################################

AC_MSG_CHECKING(how to include pthread)
AC_MSG_RESULT([])
AC_MSG_CHECKING(for -lpthread compiler option)
TMP_LIBS="$LIBS"
LIBS="$LIBS -lpthread"
AC_TRY_LINK_FUNC(pthread_create, [AC_MSG_RESULT(yes)
	have_l_pthread=yes], [AC_MSG_RESULT(no)
	have_l_pthread=no] )

if test "$have_l_pthread" = no; then
  AC_MSG_CHECKING(for -pthread compiler option)
  LIBS="$TMP_LIBS"
  TMP_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -pthread"

  AC_TRY_LINK_FUNC(pthread_create,
  	[AC_MSG_RESULT(yes)
	have_l_pthread=yes],
	[AC_MSG_RESULT(no)
	AC_MSG_ERROR(No pthread capability found)])
fi

###############################################################################
#  Check for iconv
###############################################################################

AC_SEARCH_LIBS(libiconv_open, iconv)

###############################################################################
#  Check for Crash Dialog
###############################################################################

AC_SUBST(REQUIRES)
REQUIRES=
AC_ARG_ENABLE(crash-dialog, AC_HELP_STRING([--disable-crash-dialog],
		[Disable crash dialog]),
	[enable_crash_dialog=$enableval], [enable_crash_dialog=yes])
if test "$enable_crash_dialog" != "no"; then
dnl check if GDB is somewhere
	AC_CHECK_PROG(enable_crash_dialog, gdb, yes, no)
	AC_MSG_CHECKING([whether to use crash dialog])
	if test "$enable_crash_dialog" != "no"; then
		AC_DEFINE(CRASH_DIALOG, 1, Pop up crash dialog)
		REQUIRES="gdb"
	fi
	AC_MSG_RESULT($enable_crash_dialog)
fi

###############################################################################
#  Check for ESD/ARTS
###############################################################################

AC_ARG_ENABLE(esd, AC_HELP_STRING([--disable-esd],
		[compile without ESD soundserver]))

AC_MSG_CHECKING(whether we want ESD support)
AC_MSG_RESULT($enable_esd)
if test "$enable_esd" != no ; then
    AM_PATH_GESD
    if test "$no_esd" != yes ; then
        AC_DEFINE(ESD_SOUND, , Define if we use esd)
	EB_CFLAGS="$EB_CFLAGS $ESD_CFLAGS"
	EB_LIBS="$EB_LIBS $ESD_LIBS"
    fi
fi

AM_PATH_GARTS
if test "$arts" = yes ; then
    AC_DEFINE(ARTS_SOUND, , Define if we use arts)
    EB_CFLAGS="$EB_CFLAGS $ARTS_CFLAGS"
    EB_LIBS="$EB_LIBS $ARTS_LIBS"
fi

# Hack it out of mingw32
if test "x$host_alias" != "xmingw32"; then
AM_PATH_GLIB(1.2.0, [AC_DEFINE(HAVE_GLIB, 1, [Define if you have glib])],
	AC_MSG_ERROR([Unable to find glib-devel version 1.2.0 or higher]))
AM_PATH_GTK(1.2.0, [AC_DEFINE(HAVE_GTK, 1, [Define if you have gtk])],
	AC_MSG_ERROR([Unable to find gtk-devel version 1.2.0 or higher]))

AC_ARG_ENABLE(webcam, AC_HELP_STRING([--disable-webcam],
		[Do not build webcam support]))
if test "$enable_webcam" != "no"; then
    AM_PATH_GDK_PIXBUF(, [AC_DEFINE(HAVE_GDK_PIXBUF, 1, [Define if you have gdk-pixbuf])],
        [AC_MSG_WARN([Unable to find gdk-pixbuf-devel, you won't be able to view webcams])])

    AC_MSG_CHECKING(for jasper)
    ac_check_lib_save_LIBS=$LIBS
    LIBS="-ljasper -lm -ljpeg  $LIBS"
    AC_TRY_LINK([#include <jasper/jasper.h>],
	[jas_init()],
	[EB_LIBS="$EB_LIBS -ljasper -lm -ljpeg"
	AC_DEFINE(HAVE_LIBJASPER, 1, [Define if you have jasper])
	AC_MSG_RESULT(yes)], 
	[AC_MSG_RESULT(no)
	AC_MSG_WARN([Unable to find jasper, you won't be able to view webcams])])
    LIBS=$ac_check_lib_save_LIBS

fi
else
 	GTK_CFLAGS="-mno-cygwin $msnative_struct -I/target/include -I/usr/include/mingw -I/usr/include/w32api -I/target/include/glib -I/target/include/glib-2.0 -I/usr/local/include -I/opt/include -I../ -I/usr/include/w32api -I/usr/include -D_REENTRANT -Wall -g -I../mingw32"
fi

EB_CFLAGS="$EB_CFLAGS $GLIB_CFLAGS $GTK_CFLAGS $GDK_PIXBUF_CFLAGS"
if test "x$host_alias" != "xmingw32"; then
	EB_LIBS="$EB_LIBS $GLIB_LIBS $GTK_LIBS $GDK_PIXBUF_LIBS"
fi


###############################################################################
#  Check for the X screensaver extension
###############################################################################

AC_CHECK_HEADERS(X11/Xlib.h)
AC_CHECK_HEADER(X11/extensions/scrnsaver.h, [have_xss=yes], [have_xss=no],
	[#if HAVE_X11_XLIB_H
	 #include <X11/Xlib.h>
	 #endif
	])

if test "$have_xss" = yes; then
	# Let's figure out what library it's in...
	
	# First check for it in Xss
	AC_CHECK_LIB(Xss, XScreenSaverRegister,
					[have_xss=yes; EB_LIBS="$EB_LIBS -lXss"],
					[have_xss=no], -L/usr/X11R6/lib -lX11 -lXext)

	if test "$have_xss" = no; then
		# Some versions of XFree86 put this in Xext instead of Xss
		AC_CHECK_LIB(Xext, XScreenSaverRegister,
					[have_xss=yes; EB_LIBS="$EB_LIBS -lXext"],
					[have_xss=no], -L/usr/X11R6/lib -lX11)
	fi

	if test "$have_xss" = no; then
		# And others put it in XExExt instead of in Xext
		AC_CHECK_LIB(XExExt, XScreenSaverRegister,
					[have_xss=yes; EB_LIBS="$EB_LIBS -lXExExt"],
					[have_xss=no], -L/usr/X11R6/lib -lX11 -lXext)
	fi

	if test "$have_xss" = yes; then
		AC_DEFINE(HAVE_XSS, , Define if we have the X screensaver extension)
	fi
fi

###########################################################################
# Check for Xft stuff
###########################################################################
AC_ARG_ENABLE(xft, AC_HELP_STRING([--enable-xft],
		[compile with anti-aliased font support]))
if test "$enable_xft" = yes ; then
	have_xft=no
    XFT_CFLAGS=`xft-config --cflags`
    XFT_LDFLAGS=`xft-config --libs`
    CFLAGS="$CFLAGS $XFT_CFLAGS"
    LDFLAGS="$LDFLAGS $XFT_LDFLAGS"
    AC_CHECK_HEADER(X11/Xft/Xft.h, [
	AC_CHECK_LIB(Xft, XftFontOpen, 
            have_xft=yes;[EB_LIBS="$EB_LIBS $XFT_LDFLAGS"], 
            [AC_CHECK_LIB(Xft, XftFontOpen,
                have_xft=yes;[EB_LIBS="$EB_LIBS $XFT_LDFLAGS"], ,
                -L/usr/X11R6/lib -lX11 -lfontconfig)],
            -L/usr/X11R6/lib -lX11)
    ])
  if test "$have_xft" = yes; then
    AC_DEFINE(HAVE_LIBXFT, , Define if we are to use AA)
  fi
fi

###########################################################################
# Begin Optional Modules
###########################################################################

AC_ARG_ENABLE(oscar, AC_HELP_STRING([--enable-oscar],
		[include oscar service]))
AC_ARG_ENABLE(workwizu, AC_HELP_STRING([--enable-workwizu],
		[include workwizu service]))
AC_ARG_ENABLE(broken_icq, AC_HELP_STRING([--enable-broken-icq],
		[include broken icq service instead of working one]))
AC_ARG_ENABLE(smtp, AC_HELP_STRING([--enable-smtp],
		[include smtp service]))
AC_ARG_ENABLE(aycryption, AC_HELP_STRING([--disable-aycryption],
		[exclude encryption utility]))
AC_ARG_ENABLE(aim_toc, AC_HELP_STRING([--disable-aim-toc],
		[exclude aim-toc service]))
AC_ARG_ENABLE(icq_toc, AC_HELP_STRING([--disable-icq-toc],
		[exclude icq-toc service]))
AC_ARG_ENABLE(icq, AC_HELP_STRING([--disable-icq],
		[exclude icq service]))
AC_ARG_ENABLE(irc, AC_HELP_STRING([--disable-irc],
		[exclude irc service]))
AC_ARG_ENABLE(jabber, AC_HELP_STRING([--disable-jabber],
		[exclude jabber service]))
AC_ARG_ENABLE(msn, AC_HELP_STRING([--disable-msn],
		[exclude msn service]))
AC_ARG_ENABLE(yahoo, AC_HELP_STRING([--disable-yahoo],
		[exclude yahoo service]))

OPTIONAL_PLUG_DIR=""
DEBUG_LIBMSN=""

if test "$enable_oscar" = yes; then
 	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR aim-oscar"
fi
if test "$enable_msn" != no; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR msn2"
fi
if test "$enable_workwizu" = yes; then
	have_gmp=no;
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR workwizu"
	GMP_LDFLAGS="-lgmp"
	LDFLAGS="$LDFLAGS $GMP_LDFLAGS"
	AC_CHECK_HEADER(gmp.h, [
	AC_CHECK_LIB(gmp, __gmpz_init,have_gmp=yes;[EB_LIBS="$EB_LIBS $GMP_LDFLAGS"],, -lgmp)])
	if test "$have_gmp" = no; then
	 echo "You need gmp for workwizu"
	 echo "http://www.swox.com/gmp/"
	 enable_workwizu=no;
	fi;
fi
if test "$enable_yahoo" != no; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR yahoo2"
fi
if test "$enable_irc" != no; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR irc"
fi
if test "$enable_jabber" != no; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR jabber"
fi
if test "$enable_broken_icq" = yes; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR icq"
fi
if test "$enable_smtp" = yes; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR smtp"
fi
if test "$enable_aim_toc" != no; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR aim-toc"
fi
if test "$enable_icq_toc" != no; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR icq-toc"
fi
if test "$enable_aycryption" != no; then
	AM_PATH_GPGME(, [OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR aycryption"], 
		AC_MSG_WARN(You need gpgme to compile the aycryption module))
fi

###########################################################################
# End Optional Modules
###########################################################################

AC_ARG_ENABLE(all-warnings, AC_HELP_STRING([--enable-all-warnings],
		[enable all compiler warnings - for developers]))

if test "$enable_all_warnings" = yes; then
	CFLAGS="$CFLAGS -Wall"
	CXXFLAGS="$CXXFLAGS -Wall"
fi

CFLAGS="$CFLAGS -I\$(top_srcdir)/src"
CXXFLAGS="$CXXFLAGS"


AC_SUBST_FILE(CHANGELOG)
CHANGELOG="ChangeLog"
AC_SUBST(RELEASE)
AC_DEFINE_UNQUOTED(RELEASE, "$RELEASE", [Release number of package])
AC_SUBST(CORE_VERSION)
AC_DEFINE_UNQUOTED(CORE_VERSION, $CORE_VERSION, 
	[Version of the core plugin api])

AC_SUBST(EB_CFLAGS)
AC_SUBST(EB_LIBS)
AC_SUBST(EB_LDFLAGS)
AC_SUBST(OPTIONAL_PLUG_DIR)
AC_SUBST(DEBUG_LIBMSN)

case "$host" in
*-mingw*)
	# lists more subdirectories to build
	MINGW32="mingw32"
	;;
*)
	MINGW32=""
	;;
esac

AC_SUBST(MINGW32)

AC_CONFIG_FILES([Makefile ayttm.spec
                 doc/Makefile
                 intl/Makefile
                 libproxy/Makefile
                 po/Makefile.in
                 m4/Makefile 
		 modules/Makefile
                 modules/aim-toc/Makefile
                 modules/aim-toc/libtoc/Makefile
                 modules/icq-toc/Makefile
                 modules/icq-toc/libtoc/Makefile
                 modules/aim-oscar/Makefile
                 modules/aim-oscar/libfaim/Makefile
                 modules/icq/Makefile
                 modules/icq/libicq/Makefile
                 modules/importers/Makefile
                 modules/irc/Makefile
		 modules/smtp/Makefile
                 modules/jabber/Makefile
                 modules/jabber/libjabber/Makefile
                 modules/jabber/libjabber/include/Makefile
                 modules/jabber/libjabber/include/jabber/Makefile
                 modules/jabber/libxode/Makefile
                 modules/jabber/libxode/include/Makefile
                 modules/msn2/Makefile
                 modules/msn2/libmsn2/Makefile
		 modules/aycryption/Makefile
                 modules/utility/Makefile
                 modules/yahoo2/Makefile
                 modules/yahoo2/libyahoo2/Makefile
                 modules/workwizu/Makefile
		 modules/smileys/Makefile
		 modules/smileys/console_smileys/Makefile
		 modules/smileys/giles_smiles/Makefile
                 pixmaps/Makefile
                 sounds/Makefile
                 src/Makefile
		 src/gtk/Makefile
		 mingw32/Makefile])


AC_OUTPUT

echo "$PACKAGE_STRING-$RELEASE configured with the following plugins:"
echo "	$OPTIONAL_PLUG_DIR"
