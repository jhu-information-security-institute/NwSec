# readline.m4 serial 9
dnl Copyright (C) 2005, 2006, 2009, 2010, 2011, 2012, 2013, 2014, 2015
dnl Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Written by Simon Josefsson, with help from Bruno Haible and Oskar
dnl Liljeblad.

AC_DEFUN([gl_FUNC_READLINE],
[
  dnl Prerequisites of AC_LIB_LINKFLAGS_BODY.
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  AC_REQUIRE([AC_LIB_RPATH])

  dnl Allow disabling the use of libreadline.
  AC_ARG_ENABLE([readline],
    AS_HELP_STRING([--disable-readline],
		   [do not build against libreadline or libedit]), ,
    [enable_readline=yes])

  dnl Readline libraries come in a handful flavours.
  dnl Detect the ones we do offer support for.
  AC_CHECK_HEADERS([readline/readline.h readline/history.h \
		    editline/readline.h editline/history.h])

  dnl Search for libreadline and define LIBREADLINE, LTLIBREADLINE and
  dnl INCREADLINE accordingly.
  AC_LIB_LINKFLAGS_BODY([readline])
  AC_LIB_LINKFLAGS_BODY([edit])

  dnl Add $INCREADLINE to CPPFLAGS before performing the following checks,
  dnl because if the user has installed libreadline and not disabled its use
  dnl via --without-libreadline-prefix, he wants to use it. The AC_LINK_IFELSE
  dnl will then succeed.
  am_save_CPPFLAGS="$CPPFLAGS"
  AC_LIB_APPENDTOVAR([CPPFLAGS], [$INCREADLINE])

  AC_CACHE_CHECK([for libreadline], [gl_cv_lib_readline], [
    gl_cv_lib_readline=no
    am_save_LIBS="$LIBS"
    dnl On some systems, -lreadline doesn't link without an additional
    dnl -lncurses or -ltermcap.
    dnl Try -lncurses before -ltermcap, because libtermcap is unsecure
    dnl by design and obsolete since 1994. Try -lcurses last, because
    dnl libcurses is unusable on some old Unices.
    for extra_lib in "" ncurses termcap curses; do
      LIBS="$am_save_LIBS $LIBREADLINE"
      if test -n "$extra_lib"; then
        LIBS="$LIBS -l$extra_lib"
      fi
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>
#ifdef HAVE_READLINE_READLINE_H
# include <readline/readline.h>
#endif]],
          [[readline((char*)0);]])],
        [if test -n "$extra_lib"; then
           gl_cv_lib_readline="yes, requires -l$extra_lib"
         else
           gl_cv_lib_readline="yes"
         fi
        ])
      if test "$gl_cv_lib_readline" != no; then
        break
      fi
    done
    LIBS="$am_save_LIBS"
  ])

  dnl In case of failure, examine whether libedit can act
  dnl as replacement. Small NetBSD systems use editline
  dnl as wrapper for readline.
  CPPFLAGS="$am_save_CPPFLAGS"
  AC_LIB_APPENDTOVAR([CPPFLAGS], [$INCEDIT])

  AC_CACHE_CHECK([for libedit], [gl_cv_lib_edit], [
    gl_cv_lib_edit=no
    am_save_LIBS="$LIBS"
    LIBS="$am_save_LIBS -ledit"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>
#ifdef HAVE_EDITLINE_READLINE_H
# include <editline/readline.h>
#elif defined HAVE_READLINE_READLINE_H
# include <readline/readline.h>
#endif]],
        [[readline((char*)0);]])],
      [gl_cv_lib_edit="yes"])
    LIBS="$am_save_LIBS"
  ])

  dnl Is the function readline() available?
  if ( test "$gl_cv_lib_readline" != no || test "$gl_cv_lib_edit" != no ) \
     && test "$enable_readline" = yes; then
    AC_DEFINE([HAVE_READLINE], [1], [Define if you have a readline function.])
  fi

  dnl Identify the available implementation.
  if test "$enable_readline" = "yes" \
     && test "$gl_cv_lib_readline" != no; then
    AC_DEFINE([HAVE_LIBREADLINE], [1], [Define if you have the readline library.])
    extra_lib=`echo "$gl_cv_lib_readline" | sed -n -e 's/yes, requires //p'`
    if test -n "$extra_lib"; then
      LIBREADLINE="$LIBREADLINE $extra_lib"
      LTLIBREADLINE="$LTLIBREADLINE $extra_lib"
    fi
    AC_MSG_CHECKING([how to link with libreadline])
    AC_MSG_RESULT([$LIBREADLINE])
  elif test "$enable_readline" = "yes" \
     && test "$gl_cv_lib_edit" != no; then
    AC_DEFINE([HAVE_LIBEDIT], [1], [Define if you have the edit library.])

    dnl We have a working replacement for libreadline, so use it.
    LIBREADLINE="$LIBEDIT"
    LTLIBREADLINE="$LTLIBEDIT"
    AC_MSG_CHECKING([how to link with libedit])
    AC_MSG_RESULT([$LIBEDIT])
  else
    dnl If $LIBREADLINE didn't lead to a usable library, we don't
    dnl need $INCREADLINE either.
    CPPFLAGS="$am_save_CPPFLAGS"
    LIBREADLINE=
    LTLIBREADLINE=
    LIBEDIT=
    LTLIBEDIT=
  fi
  AC_SUBST([LIBREADLINE])
  AC_SUBST([LTLIBREADLINE])
])

# Prerequisites of lib/readline.c.
AC_DEFUN([gl_PREREQ_READLINE], [
  :
])
