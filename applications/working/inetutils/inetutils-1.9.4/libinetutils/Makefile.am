#
# Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
# 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
# Foundation, Inc.
#
# This file is part of GNU Inetutils.
#
# GNU Inetutils is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or (at
# your option) any later version.
#
# GNU Inetutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see `http://www.gnu.org/licenses/'.

@PATHDEFS_MAKE@

AM_CPPFLAGS = \
	$(iu_INCLUDES) $(INCAUTH) \
	$(PATHDEF_WTMPX) $(PATHDEF_TTY_PFX) $(PATHDEF_DEVNULL) \
	$(PATHDEF_HEQUIV)

noinst_LIBRARIES = libinetutils.a

noinst_HEADERS = argcv.h libinetutils.h tftpsubs.h \
		 kerberos5_def.h shishi_def.h

EXTRA_DIST = logwtmp.c

libinetutils_a_SOURCES = \
 argcv.c\
 cleansess.c\
 daemon.c\
 defauthors.c\
 if_index.c \
 kcmd.c\
 kerberos5.c \
 krcmd.c\
 localhost.c\
 logwtmpko.c\
 setsig.c\
 shishi.c\
 tftpsubs.c\
 ttymsg.c\
 utmp_init.c\
 utmp_logout.c
