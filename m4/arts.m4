# Configure paths for aRts
# Sebastian Held <sebastian.held@gmx.de> 28.1.2001
# script derived from original esd.m4 (see below)

  # Configure paths for ESD
  # Manish Singh    98-9-30
  # stolen back from Frank Belew
  # stolen from Manish Singh
  # Shamelessly stolen from Owen Taylor


dnl AM_PATH_GARTS([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for ARTS, and define ARTS_CFLAGS and ARTS_LIBS
dnl

AC_DEFUN(AM_PATH_GARTS,
[
  # We use the audiofile library: don't know how to probe...
  ARTS_LIBS="-laudiofile"

  dnl 
  dnl Get the cflags and libraries from the artsc-config script
  dnl

  AC_ARG_WITH(arts-prefix,[  --with-arts-prefix=PFX   Prefix where ARTS is installed (optional)],
              arts_prefix="$withval", arts_prefix="")
  AC_ARG_ENABLE(arts,[  --disable-arts          Do not include support for ARTS])

  # try to automatically find artsc-config
  AC_PATH_PROG(ARTSC_CONFIG, artsc-config, no, $PATH:/opt/kde2/bin)

  # user may override path to artsc-config
  if test x$arts_prefix != x ; then
     arts_args="$arts_args --prefix=$arts_prefix"
     if test x${ARTSC_CONFIG+set} != xset ; then
        ARTSC_CONFIG=$arts_prefix/bin/artsc-config
     fi
  fi

  AC_MSG_CHECKING(for ARTS)

  if test "$ARTSC_CONFIG" != "no" ; then
    ARTS_CFLAGS="$ARTS_CFLAGS `$ARTSC_CONFIG $artsconf_args --cflags`"
    ARTS_LIBS="$ARTS_LIBS `$ARTSC_CONFIG $artsconf_args --libs`"
  else
    echo "cannot find artsc_config; you must provide --with-arts-prefix"
	enable_arts=no
  fi

  if test x$enable_arts = xyes ; then
    AC_MSG_RESULT(yes  - user override)
    arts=yes
  elif test x$enable_arts = xno ; then
    AC_MSG_RESULT(no  - user override)
    ARTS_CFLAGS=""
    ARTS_LIBS=""
    arts=no
  else
    echo -n "automatic: "

    # save CFLAGS & LIBS
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $ARTS_CFLAGS"
    LIBS="$LIBS $ARTS_LIBS"
    
    dnl
    dnl Sanity checks the results of artsc-config to some extent
    dnl
    rm -f conf.artstest
    AC_TRY_RUN([
#include <artsc.h>

int main ()
{
  int err;
  
  system ("touch conf.artstest");

  err = arts_init();
  arts_free();
  if (err < 0)
  {
    printf("ERROR executing arts_init(): %s\n", arts_error_text( err ) );
    return 1;
  }
  return 0;
}

], arts=yes, arts=no,[echo $ac_n "cross compiling; assumed OK... $ac_c"])

    # restore CFLAGS & LIBS
    CFLAGS="$ac_save_CFLAGS"
    LIBS="$ac_save_LIBS"

    if test "x$arts" = xyes ; then
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
      echo "*** Could not run ARTS test program, checking why..."
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $ARTS_CFLAGS"
      LIBS="$LIBS $ARTS_LIBS"
      AC_TRY_LINK([
#include <stdio.h>
#include <artsc.h>
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding ARTS or finding the wrong"
          echo "*** version of ARTS. If it is not finding ARTS, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means ARTS was incorrectly installed"
          echo "*** or that you have moved ARTS since it was installed. In the latter case, you"
          echo "*** may want to edit the artsc-config script: $ARTSC_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
          ARTS_CFLAGS=""
          ARTS_LIBS=""
    fi
    
  fi
  
  AC_SUBST(ARTS_CFLAGS)
  AC_SUBST(ARTS_LIBS)
  rm -f conf.artstest
])
