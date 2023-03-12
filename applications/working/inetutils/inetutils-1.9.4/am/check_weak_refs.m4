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

dnl IU_CHECK_WEAK_REFS -- See if any of a variety of `weak reference'
dnl mechanisms works.  If so, this defines HAVE_WEAK_REFS, and one of
dnl HAVE_ATTR_WEAK_REFS, HAVE_PRAGMA_WEAK_REFS, or HAVE_ASM_WEAK_REFS to
dnl indicate which sort.
dnl
dnl This can't just be a compile-check, as gcc somtimes accepts the syntax even
dnl feature isn't actually supported.
dnl
AC_DEFUN([IU_CHECK_WEAK_REFS], [
  AH_TEMPLATE(HAVE_WEAK_REFS, 1, [Define if you have weak references])
  AC_CACHE_CHECK(whether gcc weak references work,
		 inetutils_cv_attr_weak_refs,
    AC_TRY_LINK([],
      [extern char *not_defined (char *, char *) __attribute__ ((weak));
	if (not_defined) puts ("yes"); ],
      [inetutils_cv_attr_weak_refs=yes],
      [inetutils_cv_attr_weak_refs=no]))
  if test "$inetutils_cv_weak_refs" = yes; then
    AC_DEFINE([HAVE_WEAK_REFS])
    AC_DEFINE([HAVE_ATTR_WEAK_REFS], 1,
              [Define if you have weak "attribute" references])
  else
    AC_CACHE_CHECK(whether pragma weak references work,
		   inetutils_cv_pragma_weak_refs,
      AC_TRY_LINK([],
	[extern char *not_defined (char *, char *);
#pragma weak not_defined
	 if (not_defined) puts ("yes"); ],
	[inetutils_cv_pragma_weak_refs=yes],
	[inetutils_cv_pragma_weak_refs=no]))
    if test "$inetutils_cv_pragma_weak_refs" = yes; then
      AC_DEFINE([HAVE_WEAK_REFS])
      AC_DEFINE([HAVE_PRAGMA_WEAK_REFS], 1,
                [Define if you have weak "pragma" references])
    else
      AC_CACHE_CHECK(whether asm weak references work,
		     inetutils_cv_asm_weak_refs,
	AC_TRY_LINK([],
	  [extern char *not_defined (char *, char *);
	   asm (".weak not_defined");
	   if (not_defined) puts ("yes"); ],
	  [inetutils_cv_asm_weak_refs=yes],
	  [inetutils_cv_asm_weak_refs=no]))
      if test "$inetutils_cv_asm_weak_refs" = yes; then
	AC_DEFINE([HAVE_WEAK_REFS])
	AC_DEFINE([HAVE_ASM_WEAK_REFS], 1,
	          [Define if you have weak "assembler" references])
      fi
    fi
  fi])dnl
