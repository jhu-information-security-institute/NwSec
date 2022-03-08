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

dnl IU_CHECK_MACRO -- Check for cpp defines
dnl   $1 - NAME   -- printed in message
dnl   $2 - INCLS  -- C program text to inculde necessary files for testing
dnl   $3 - MACROS -- a space-separated list of macros that all must be defined
dnl		     defaults to NAME
dnl   $4 - TRUE	  -- what to do if all macros are defined; defaults to
dnl		     AC_DEFINE(upcase(HAVE_$1))
dnl   $5 - FALSE  -- what to do if some macros aren't defined
dnl
AC_DEFUN([IU_CHECK_MACRO], [
  define([IU_CVAR], [inetutils_cv_macro_]translit($1, [A-Z ./<>], [a-z___]))dnl
  define([IU_TAG], [IU_CHECK_MACRO_]translit($1, [a-z ./<>], [A-Z___]))dnl
  AC_CACHE_CHECK([for $1], IU_CVAR,
    AC_EGREP_CPP(IU_TAG,
      IU_FLUSHLEFT(
[$2
#if ]dnl
changequote(<<,>>)dnl
patsubst(patsubst(ifelse(<<$3>>, , <<$1>>, <<$3>>),
	          <<\>[ ,]+\<>>, << && >>),
         <<\w+>>, <<defined(\&)>>) dnl
changequote([,])dnl
[
]IU_TAG[
#endif]),
      IU_CVAR[=yes],
      IU_CVAR[=no])) dnl
  IU_RESULT_ACTIONS(IU_CVAR, [$1], [$4], [$5]) dnl
  undefine([IU_CVAR]) undefine([IU_TAG])])dnl
