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

dnl IU_FLUSHLEFT -- remove all whitespace at the beginning of lines
dnl This is useful for c-code which may include cpp statements
dnl
AC_DEFUN([IU_FLUSHLEFT],
 [m4_changequote(`,')dnl
m4_bpatsubst(`$1', `^[	 ]+')
m4_changequote([,])])dnl
