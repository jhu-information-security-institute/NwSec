dnl
dnl Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
dnl 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015
dnl Free Software Foundation, Inc.
dnl
dnl This file is part of GNU Inetutils.
dnl
dnl GNU Inetutils is free software: you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation, either version 3 of the License, or (at
dnl your option) any later version.
dnl
dnl GNU Inetutils is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see `http://www.gnu.org/licenses/'.

dnl Written by Miles Bader.

dnl IU_LIB_NCURSES -- check for, and configure, ncurses
dnl
dnl If libncurses is found to exist on this system and the --disable-ncurses
dnl flag wasn't specified, defines LIBNCURSES with the appropriate linker
dnl specification, and possibly defines NCURSES_INCLUDE with the appropriate
dnl -I flag to get access to ncurses include files.
dnl
AC_DEFUN([IU_LIB_NCURSES], [
  AC_ARG_ENABLE([ncurses],
                AS_HELP_STRING([--disable-ncurses],
                               [don't prefer -lncurses over -lcurses]),
              , [enable_ncurses=yes])
  if test "$enable_ncurses" = yes; then
    AC_CHECK_LIB(ncurses, initscr, LIBNCURSES="-lncurses")
    if test "$LIBNCURSES"; then
      # Use ncurses header files instead of the ordinary ones, if possible;
      # is there a better way of doing this, that avoids looking in specific
      # directories?
      AC_ARG_WITH([ncurses-include-dir],
                  AS_HELP_STRING([--with-ncurses-include-dir=DIR],
                                 [Set directory containing the include files for
                          use with -lncurses, when it isn't installed as
                          the default curses library.  If DIR is "none",
                          then no special ncurses include files are used.]))
      if test "${with_ncurses_include_dir+set}" = set; then
        AC_MSG_CHECKING(for ncurses include dir)
	case "$with_ncurses_include_dir" in
	  no|none)
	    inetutils_cv_includedir_ncurses=none;;
	  *)
	    inetutils_cv_includedir_ncurses="$with_ncurses_include_dir";;
	esac
        AC_MSG_RESULT($inetutils_cv_includedir_ncurses)
      else
	AC_CACHE_CHECK(for ncurses include dir,
		       inetutils_cv_includedir_ncurses,
	  for D in $includedir $prefix/include /local/include /usr/local/include /include /usr/include; do
	    if test -d $D/ncurses; then
	      inetutils_cv_includedir_ncurses="$D/ncurses"
	      break
	    fi
	    test "$inetutils_cv_includedir_ncurses" \
	      || inetutils_cv_includedir_ncurses=none
	  done)
      fi
      if test "$inetutils_cv_includedir_ncurses" = none; then
        NCURSES_INCLUDE=""
	LIBNCURSES=''
      else
        NCURSES_INCLUDE="-I$inetutils_cv_includedir_ncurses"
      fi
    fi
  fi
  AC_SUBST(NCURSES_INCLUDE)
  AC_SUBST(LIBNCURSES)])dnl

dnl IU_LIB_TERMCAP -- check for various termcap libraries
dnl
dnl Checks for various common libraries implementing the termcap interface,
dnl including ncurses (unless --disable ncurses is specified), curses (which
dnl does so on some systems), termcap, and termlib.  If termcap is found, then
dnl LIBTERMCAP is defined with the appropriate linker specification.
dnl
dnl Solaris is known to use libtermcap for tgetent, but to declare tgetent
dnl in <term.h>!
dnl
AC_DEFUN([IU_LIB_TERMCAP], [
  AC_REQUIRE([IU_LIB_NCURSES])
  if test "$LIBNCURSES"; then
    LIBTERMCAP="$LIBNCURSES"
  else
    dnl Must check declaration in different settings,
    dnl so caching in AC_CHECK_DECL is too distructive.
    dnl
    _IU_SAVE_LIBS=$LIBS
    AC_CHECK_LIB(termcap, tgetent, LIBTERMCAP=-ltermcap)
    AC_MSG_CHECKING([whether tgetent needs support])
    location_tgetent=no
    LIBS="$LIBS $LIBTERMCAP"
    AC_LINK_IFELSE(
      [AC_LANG_PROGRAM([[#include <termcap.h>]],
	[[(void) tgetent((char *) 0, (char *) 0);]])],
      [AC_DEFINE([HAVE_TERMCAP_TGETENT], 1,
	[Define to 1 if tgetent() exists in <termcap.h>.])
       ac_cv_have_decl_tgetent=yes
       location_tgetent=termcap.h],
      [AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[#include <curses.h>
#include <term.h>]],
	  [[(void) tgetent((char *) 0, (char *) 0);]])],
	[AC_DEFINE([HAVE_CURSES_TGETENT], 1,
	  [Define to 1 if tgetent() exists in <term.h>.])
	 ac_cv_have_decl_tgetent=yes
	 location_tgetent=term.h])
      ])
    LIBS=$_IU_SAVE_LIBS
    AC_MSG_RESULT($location_tgetent)

    if test "$ac_cv_lib_termcap_tgetent" = yes \
	&& test "$ac_cv_have_decl_tgetent" = yes; then
      :
    else
      AC_CHECK_LIB(curses, tgetent, LIBTERMCAP=-lcurses)
      AC_CHECK_DECLS([tgetent], , , [[#include <curses.h>
#include <term.h>]])
      if test "$ac_cv_lib_curses_tgetent" = yes \
	  && test "$ac_cv_have_decl_tgetent" = yes; then
	AC_DEFINE([HAVE_CURSES_TGETENT], 1)
      fi
    fi
    if test "$ac_cv_lib_curses_tgetent" = no \
	&& test "$ac_cv_lib_termcap_tgetent" = no; then
      AC_CHECK_LIB(termlib, tgetent, LIBTERMCAP=-ltermlib)
      if test "$ac_cv_lib_termlib_tgetent" = yes; then
	AC_DEFINE([HAVE_TERMLIB_TGETENT], 1,
		  [Define to 1 if tgetent() exists in libtermlib.])
      else
	LIBTERMCAP=
      fi
    fi
    if test -n "$LIBTERMCAP"; then
      AC_DEFINE([HAVE_TGETENT], 1, [Define to 1 if tgetent() exists.])
    fi
  fi
  AC_SUBST(LIBTERMCAP)])dnl

dnl IU_LIB_CURSES -- check for curses, and associated libraries
dnl
dnl Checks for various libraries implementing the curses interface, and if
dnl found, defines LIBCURSES to be the appropriate linker specification,
dnl *including* any termcap libraries if needed (some versions of curses
dnl don't need termcap).
dnl
AC_DEFUN([IU_LIB_CURSES], [
  AC_REQUIRE([IU_LIB_TERMCAP])
  AC_REQUIRE([IU_LIB_NCURSES])
  if test "$LIBNCURSES"; then
    LIBCURSES="$LIBNCURSES"	# ncurses doesn't require termcap
  else
    _IU_SAVE_LIBS="$LIBS"
    LIBS="$LIBTERMCAP"
    AC_CHECK_LIB(curses, initscr, LIBCURSES="-lcurses")
    if test "$LIBCURSES" \
       && test "$LIBTERMCAP" \
       && test "$LIBCURSES" != "$LIBTERMCAP"; then
      AC_CACHE_CHECK(whether curses needs $LIBTERMCAP,
		     inetutils_cv_curses_needs_termcap,
	LIBS="$LIBCURSES"
	AC_TRY_LINK([#include <curses.h>], [initscr ();],
		    [inetutils_cv_curses_needs_termcap=no],
		    [inetutils_cv_curses_needs_termcap=yes]))
      if test $inetutils_cv_curses_needs_termcap = yes; then
	  LIBCURSES="$LIBCURSES $LIBTERMCAP"
      fi
    fi
    LIBS="$_IU_SAVE_LIBS"
  fi
  AC_SUBST(LIBCURSES)])dnl
