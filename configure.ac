# Process this file with autoconf to produce a configure script.
dnl We use the new format of AC_INIT
dnl info --file=autoconf.info --node="Initializing configure" for more info
AC_INIT([Ayttm], [0.5.0], [ayttm-users@lists.sourceforge.net])
AC_PREREQ(2.53)
AC_CONFIG_SRCDIR(src/main.c)
AM_INIT_AUTOMAKE([1.6 dist-bzip2 dist-zip])
RELEASE=115

# This is to check modules/core binary compat
# Bump it up when necessary only (query_callbacks change, struct change etc)
CORE_VERSION=21

AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

AC_SUBST(REQUIRES)
REQUIRES=

# Checks for programs.
AC_PROG_CC
AC_ISC_POSIX

AM_PROG_LEX
# Got this from http://www.winehq.org/pipermail/wine-devel/2001-June/001254.html
AC_CHECK_PROGS(XLEX, $LEX lex flex,none )
if test "$XLEX" = "none"; then
	AC_MSG_ERROR([No suitable scanner found. Please install 'lex' or 'flex' package])
fi

AC_PROG_YACC
AC_CHECK_PROGS(XYACC, $YACC bison byacc yacc, none)
if test "$XYACC" = "none"; then
	AC_MSG_ERROR([No suitable parser found. Please install 'bison', 'yacc' or 'byacc' package])
fi

AC_PROG_CPP

AC_PROG_CXX
AC_CHECK_PROGS(XCXX, g++ c++ gpp aCC CC cxx cc++ cl.exe FCC KCC RCC xlC_r xlC, none)
if test "$XCXX" = "none"; then
	AC_MSG_ERROR([No suitable c++ compiler found. Please install an appropriate c++ compiler for your system])
fi

AC_PROG_RANLIB
AC_PROG_INSTALL
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

# Dynamic Library Loading
# - To choose if we use POSIX dlopen or cross-platform libtool

AC_ARG_ENABLE(posix-dlopen, AC_HELP_STRING([--enable-posix-dlopen],
		[Use POSIX dlopen to open shared libraries instead of libtool. Note that we still need libtool to build the application]), 
		[use_posix_dlopen=yes], [use_posix_dlopen=no])

if test "$use_posix_dlopen" = "no"; then
	echo "Using Libtool for dynamic library loading"
else
	echo "Using POSIX dlopen for dynamic library loading"
	AC_DEFINE([USE_POSIX_DLOPEN], [1], [Define if we use POSIX dlopen])
fi

AC_CHECK_TOOL(RC, windres, no)

# add some platform-specific flags
use_pthread=yes
use_mingw=no
case "$host" in
*-darwin*)
        CFLAGS="$CFLAGS -no-cpp-precomp"
        CXXFLAGS="$CXXFLAGS -no-cpp-precomp"
        ;;
*-*-mingw*)
        use_mingw=yes
	use_pthread=no
        ;;
esac
case "$host_os" in
aix*)
	use_pthread=no
	;;
netbsd*)
	CFLAGS="$CFLAGS -I/usr/pkg/include"
	CXXFLAGS="$CXXFLAGS -I/usr/pkg/include"
	LDFLAGS="$LDFLAGS -L/usr/pkg/lib -lltdl"
	use_pthread=no
	;;
esac

LIBS="$LIBS -L/usr/local/lib"

# Checks for libraries.
AC_CHECK_LIB([audiofile], [_af_ulaw2linear])
AC_CHECK_LIB([ltdl], [lt_dlopen],,AC_MSG_ERROR([Could not find Libtool Libraries]))

# Checks for header files.
AC_PATH_XTRA
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_CHECK_HEADERS([errno.h fcntl.h langinfo.h libintl.h limits.h locale.h stdlib.h string.h strings.h unistd.h iconv.h])
AC_CHECK_HEADERS([alloca.h argz.h arpa/inet.h malloc.h netdb.h netinet/in.h paths.h sgtty.h stddef.h sys/file.h sys/ioctl.h sys/param.h sys/poll.h sys/socket.h sys/time.h syslog.h termios.h utmp.h getopt.h])
AC_CHECK_HEADERS([sys/utsname.h])

#Checking for pspell
AC_CHECK_HEADER([pspell/pspell.h], [have_pspell_header=yes], [have_pspell_header=no])
AC_CHECK_LIB([pspell], [pspell_manager_check], [have_pspell=yes], [have_pspell=no])
AC_CHECK_LIB([pspell], [aspell_speller_check], [have_aspell=yes], [have_aspell=no])

if test "$have_pspell_header" = "no" || test "$have_aspell" = "no" && test "$have_pspell" = "no"; then
	AC_MSG_WARN([PSpell not found. Ayttm will be built without spell check.])
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
AC_CHECK_FUNCS([atexit bzero dcgettext dup2 getcwd gethostbyname gethostname gettimeofday getwd hstrerror inet_ntoa isascii memchr memmove mempcpy memset mkdir modf munmap nl_langinfo pow select setlocale socket stpcpy strcasecmp strchr strcspn strdup strerror strncasecmp strpbrk strrchr strspn strstr strtol strtoul uname snprintf])
AC_CHECK_FUNCS([__argz_count __argz_next __argz_stringify getopt_long getopt])

ALL_LINGUAS="fr en_GB pt_BR de es nl sv hi"
AM_GNU_GETTEXT([external])

###############################################################################
#  Check for pthreads
###############################################################################

if test "$use_pthread" = yes ; then
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
fi

###############################################################################
#  Check for iconv
###############################################################################

AC_SEARCH_LIBS(libiconv_open, iconv)

###############################################################################
#  Check for Crash Dialog
###############################################################################

if test "$use_mingw" = no ; then
  AC_ARG_ENABLE(crash-dialog, AC_HELP_STRING([--disable-crash-dialog],
  		[Disable crash dialog]),
  	[enable_crash_dialog=$enableval], [enable_crash_dialog=yes])
  if test "$enable_crash_dialog" != "no"; then
  dnl check if GDB is somewhere
  	AC_CHECK_PROG(enable_crash_dialog, gdb, yes, no)
  	AC_MSG_CHECKING([whether to use crash dialog])
  	if test "$enable_crash_dialog" != "no"; then
  		AC_DEFINE(CRASH_DIALOG, 1, Pop up crash dialog)
  		REQUIRES="Requires: gdb"
  	fi
  	AC_MSG_RESULT($enable_crash_dialog)
  fi
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

AM_PATH_GARTS(1.1.3, arts=yes, arts=old, arts=no)

if test "$arts" = old ; then
  # Older than 1.1.3 - check for older arts
  AM_PATH_GARTS(,arts=yes)
  if test "$arts" = yes ; then
    AC_DEFINE(ARTS_FREE_IS_BROKEN, 1, [Define if arts_init(); arts_free(); arts_init() breaks])
  fi
fi

if test "$arts" = yes ; then
    AC_DEFINE(ARTS_SOUND, , Define if we use arts)
    EB_CFLAGS="$EB_CFLAGS $ARTS_CFLAGS"
    EB_LIBS="$EB_LIBS $ARTS_LIBS"
fi

AM_PATH_GLIB_2_0( , [AC_DEFINE(HAVE_GLIB, 1, [Define if you have glib])],
	AC_MSG_ERROR([Unable to find glib-devel version 2.0.0 or higher]))

AM_PATH_GTK_2_0( 2.14.0, [AC_DEFINE(HAVE_GTK_2_14, 1, [Define if you have gtk])],
	[AM_PATH_GTK_2_0( 2.12.0, [AC_DEFINE(HAVE_GTK, 1, [Define if you have gtk])],
		AC_MSG_ERROR([Unable to find gtk-devel version 2.12.0 or higher]))])

AC_ARG_ENABLE(webcam, AC_HELP_STRING([--disable-webcam],
		[Do not build webcam support]))

AC_ARG_WITH(jasper-prefix, AC_HELP_STRING([--with-jasper-prefix],
		[Path where jasper include and lib directories can be found]))
AC_ARG_ENABLE(jasper-filter, AC_HELP_STRING([--disable-jasper-filter],
		[Do not build a jasper based jpeg2000 filter]))

if test "$enable_webcam" != "no"; then

    AC_MSG_CHECKING(for jasper)
    ac_check_lib_save_LIBS=$LIBS
    ac_check_lib_save_CFLAGS=$CFLAGS
    LIBS="-ljasper -lm -ljpeg -L $with_jasper_prefix/lib $LIBS"
    if test -n "$with_jasper_prefix"; then
        CFLAGS="$CFLAGS -I$with_jasper_prefix/include"
    fi
    AC_TRY_LINK([#include <jasper/jasper.h>],
	[jas_init()],
	[JASPER_LIBS="-ljasper -lm -ljpeg"
	AC_MSG_RESULT(yes)], 
	[AC_MSG_RESULT(no)
	enable_jasper_filter="no"
	AC_MSG_WARN([Unable to find jasper, you won't be able to view webcams])])
    LIBS=$ac_check_lib_save_LIBS
else
    # if one is disabled then disable both
    enable_jasper_filter="no"
fi

AC_ARG_WITH(openssl-prefix, AC_HELP_STRING([--with-openssl-prefix=PFX],
		[Prefix where openssl is installed (optional)]))
SSL_INCLUDES=
SSL_LIBDIR=
if test "$with_openssl_prefix" != ""; then
	SSL_INCLUDES="-I${with_openssl_prefix}/include"
	SSL_LIBDIR="-L${with_openssl_prefix}/lib"
fi

AC_MSG_CHECKING(for openssl)
ac_check_lib_save_LIBS=$LIBS
LIBS="$SSL_LIBDIR -lssl -lcrypto $LIBS"
ac_check_lib_save_CFLAGS=$CFLAGS
CFLAGS="$SSL_INCLUDES $CFLAGS"
AC_TRY_LINK([#include <openssl/opensslv.h>],
    [ return OPENSSL_VERSION_NUMBER; ],
    [have_openssl=yes], [have_openssl=no])

if test "$have_openssl" = yes; then
	EB_LIBS="$EB_LIBS $SSL_LIBDIR -lssl -lcrypto"
	EB_CFLAGS="$EB_CFLAGS $SSL_INCLUDES"
	AC_DEFINE(HAVE_OPENSSL, 1, [Define if you have openssl])
	AC_MSG_RESULT([yes])
else
	enable_msn="no"
	AC_MSG_RESULT(no)
	AC_MSG_WARN([Unable to find openssl, you won't have MSN plugin])
fi


LIBS=$ac_check_lib_save_LIBS 
CFLAGS=$ac_check_lib_save_CFLAGS

GTK_CFLAGS="$GTK_CFLAGS" # -DGTK_DISABLE_DEPRECATED"
EB_CFLAGS="$EB_CFLAGS $GLIB_CFLAGS $GTK_CFLAGS"
EB_LIBS="$EB_LIBS $GLIB_LIBS $GTK_LIBS"


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

AC_CHECK_HEADER(X11/xpm.h, [have_xpm=yes], [have_xpm=no], )

if test "$have_xpm" = yes; then
	AC_DEFINE(HAVE_XPM, , Define if we have XPM file support)
else
	AC_MSG_RESULT([Smiley support will not be built. Install the libxpm-dev package to get smiley support])
fi

EB_CFLAGS="$EB_CFLAGS $X_CFLAGS"
EB_LIBS="$EB_LIBS $X_LIBS"

###########################################################################
# Begin Optional Modules
###########################################################################

AC_ARG_ENABLE(oscar, AC_HELP_STRING([--disable-oscar],
		[exclude oscar service]))
AC_ARG_ENABLE(workwizu, AC_HELP_STRING([--enable-workwizu],
		[include workwizu service]))
AC_ARG_ENABLE(broken_icq, AC_HELP_STRING([--enable-broken-icq],
		[include broken icq service instead of working one]))
AC_ARG_ENABLE(smtp, AC_HELP_STRING([--enable-smtp],
		[include smtp service]))
AC_ARG_ENABLE(lj, AC_HELP_STRING([--enable-lj],
		[include livejournal service]))
AC_ARG_ENABLE(aycryption, AC_HELP_STRING([--disable-aycryption],
 		[exclude encryption utility]))
AC_ARG_ENABLE(aim_toc, AC_HELP_STRING([--enable-aim-toc],
		[include aim-toc service]))
AC_ARG_ENABLE(icq_toc, AC_HELP_STRING([--enable-icq-toc],
		[include icq-toc service]))
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

if test "$enable_oscar" != no; then
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
if test "$enable_lj" = yes; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR livejournal"
fi
if test "$enable_aim_toc" = yes; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR aim-toc"
fi
if test "$enable_icq_toc" = yes; then
	OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR icq-toc"
fi
if test "$enable_aycryption" != no; then
	AM_PATH_GPGME(1.0.0, [OPTIONAL_PLUG_DIR="$OPTIONAL_PLUG_DIR aycryption"], 
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
EB_LDFLAGS="$EB_LDFLAGS $LDFLAGS  `pkg-config --libs gthread-2.0`"

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
AC_SUBST(JASPER_LIBS)
AC_SUBST(OPTIONAL_PLUG_DIR)
AC_SUBST(DEBUG_LIBMSN)


dnl ##########################################################################
# Checking for an installed web browser

printf "Looking for a Web browser... "

if [ test -e /usr/bin/x-www-browser ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["x-www-browser"], [x-www-browser] )
	AC_MSG_RESULT([x-www-browser])
elif [ test -e /usr/bin/mozilla ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["mozilla"], [Mozilla])
	AC_MSG_RESULT([mozilla])
elif [ test -e /usr/bin/firefox ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["firefox"], [Firefox])
	AC_MSG_RESULT([firefox])
elif [ test -e /usr/bin/iceweasel ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["iceweasel"], [Iceweasel])
	AC_MSG_RESULT([iceweasel])
elif [ test -e /usr/bin/iceape ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["iceape"], [Iceape])
	AC_MSG_RESULT([iceape])
elif [ test -e /usr/bin/epiphany ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["epiphany"], [Epiphany])
	AC_MSG_RESULT([epiphany])
elif [ test -e /usr/bin/dillo ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["dillo"], [Dillo])
	AC_MSG_RESULT([dillo])
elif [ test -e /usr/bin/konqueror ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["konqueror"], [Konqueror])
	AC_MSG_RESULT([konqueror])
elif [ test -e /usr/bin/galeon ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["galeon"], [Galeon])
	AC_MSG_RESULT([galeon])
elif [ test -e /usr/bin/opera ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["opera"], [Opera])
	AC_MSG_RESULT([opera])
elif [ test -e /usr/bin/lynx ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["lynx"], [Lynx])
	AC_MSG_RESULT([lynx])
elif [ test -e /usr/bin/links ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["links"], [Links])
	AC_MSG_RESULT([links])
elif [ test -e /usr/bin/elinks ]; then
	AC_DEFINE(DEFAULT_WWW_BROWSER, ["elinks"], [ELinks])
	AC_MSG_RESULT([elinks])
else
	AC_MSG_WARN([Could not find any of the following browsers:
		     x-www-browser 
		     mozilla 
		     firefox 
		     iceape 
		     iceweasel 
		     epiphany 
		     dillo 
		     konqueror 
		     galeon 
		     opera 
		     lynx 
		     links 
		     elinks
Defaulting to x-www-browser since I don't know better. You can change it later from the Preferences menu])

	AC_DEFINE(DEFAULT_WWW_BROWSER, ["x-www-browser"], [x-www-browser])
fi

dnl ##########################################################################


HOST=`echo $host | sed -e 's/^[[^-]]*-[[^-]]*-\([[^-]]*\).*/\1/'`
HOST=`echo $ECHO_N "${HOST%${HOST#?}} $ECHO_C" | tr a-z A-Z; echo ${HOST#?} | tr A-Z a-z`
AC_DEFINE_UNQUOTED(HOST, "$HOST", [OS that this system was built for])
AC_SUBST(HOST)
AC_SUBST(RC)

AM_CONDITIONAL(MINGW32, test $use_mingw = yes)
AM_CONDITIONAL(BUILD_JASPER_FILTER, test "$enable_jasper_filter" != "no")

AC_CONFIG_FILES([Makefile ayttm.spec
                 doc/Makefile
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
		 modules/aim-oscar/libfaim/include/Makefile
		 modules/aim-oscar/libfaim/src/Makefile
                 modules/icq/Makefile
                 modules/icq/libicq/Makefile
                 modules/importers/Makefile
                 modules/irc/Makefile
		 modules/irc/libirc/Makefile
		 modules/smtp/Makefile
                 modules/livejournal/Makefile
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
		 modules/smileys/Makefile
		 modules/smileys/console_smileys/Makefile
		 modules/smileys/giles_smiles/Makefile
		 modules/image_filter/Makefile
                 pixmaps/Makefile
                 sounds/Makefile
                 src/Makefile
		 src/gtk/Makefile])
#		 mingw32/Makefile])


AC_OUTPUT

echo "$PACKAGE_STRING-$RELEASE configured with the following plugins:"
echo "	$OPTIONAL_PLUG_DIR"
