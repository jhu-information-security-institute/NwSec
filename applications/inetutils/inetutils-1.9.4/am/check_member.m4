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

dnl IU_CHECK_MEMBER(AGGREGATE.MEMBER,
dnl                [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
dnl                [INCLUDES])
dnl AGGREGATE.MEMBER is for instance `struct passwd.pw_gecos'.
dnl The member itself can be of an aggregate type
dnl Shell variables are not a valid argument.
AC_DEFUN([IU_CHECK_MEMBER],
[AS_LITERAL_IF([$1], [],
               [AC_FATAL([$0: requires literal arguments])])dnl
m4_bmatch([$1], [\.], ,
         [m4_fatal([$0: Did not see any dot in `$1'])])dnl
AS_VAR_PUSHDEF([ac_Member], [ac_cv_member_$1])dnl
dnl Extract the aggregate name, and the member name
AC_CACHE_CHECK([for $1], ac_Member,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT(IU_FLUSHLEFT([$4]))],
[dnl AGGREGATE ac_aggr;
static m4_bpatsubst([$1], [\..*]) ac_aggr;
dnl ac_aggr.MEMBER;
if (sizeof(ac_aggr.m4_bpatsubst([$1], [^[^.]*\.])))
return 0;])],
                [AS_VAR_SET(ac_Member, yes)],
                [AS_VAR_SET(ac_Member, no)])])
AS_IF([test AS_VAR_GET(ac_Member) = yes], [$2], [$3])
AS_VAR_POPDEF([ac_Member])dnl
])dnl IU_CHECK_MEMBER


dnl IU_CHECK_MEMBERS([AGGREGATE.MEMBER, ...],
dnl                  [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND]
dnl                  [INCLUDES])
AC_DEFUN([IU_CHECK_MEMBERS],
[m4_foreach([AC_Member], [$1],
  [IU_CHECK_MEMBER(AC_Member,
         [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_[]AC_Member), 1,
                            [Define to 1 if `]m4_bpatsubst(AC_Member,
                                                     [^[^.]*\.])[' is
                             member of `]m4_bpatsubst(AC_Member, [\..*])['.])
$2],
                 [$3],
                 [$4])])])
