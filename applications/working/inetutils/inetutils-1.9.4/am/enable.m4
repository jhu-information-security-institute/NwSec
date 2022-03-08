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

dnl Written Joel N. Weber II.

AC_DEFUN([IU_ENABLE_FOO],
 [AC_ARG_ENABLE($1, AS_HELP_STRING([--disable-$1], [don't compile $1]), ,
                [enable_]$1[=$enable_]$2)
[if test "$enable_$1" = yes; then
   $1_BUILD=$1
   $1_INSTALL_HOOK="install-$1-hook"
else
   $1_BUILD=''
   $1_INSTALL_HOOK=''
fi;]
  AC_SUBST([$1_BUILD])
  AC_SUBST([$1_INSTALL_HOOK])
  AC_SUBST([$1_PROPS])
  AC_SUBST([enable_$1])
  AM_CONDITIONAL([ENABLE_$1], test "$enable_$1" = yes)
])

AC_DEFUN([IU_ENABLE_CLIENT], [IU_ENABLE_FOO($1, clients)])
AC_DEFUN([IU_ENABLE_SERVER], [IU_ENABLE_FOO($1, servers)])

AC_DEFUN([IU_DISABLE_TARGET],
  [enable_$1=no $1_BUILD='' $1_INSTALL_HOOK=''
  AM_CONDITIONAL([ENABLE_$1], test "$enable_$1" != no)
])
