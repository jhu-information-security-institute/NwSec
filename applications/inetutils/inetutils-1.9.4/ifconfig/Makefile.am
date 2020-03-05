#
# Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
# 2010, 2011, 2012, 2013, 2014, 2015 Free Software Foundation, Inc.
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

SUBDIRS = system

@PATHDEFS_MAKE@

AM_CPPFLAGS = \
	$(iu_INCLUDES) \
	$(PATHDEF_PROCNET_DEV)

LDADD = \
	$(iu_LIBRARIES)

bin_PROGRAMS = $(ifconfig_BUILD)

EXTRA_PROGRAMS = ifconfig

ifconfig_SOURCES = ifconfig.c flags.c changeif.c options.c printif.c system.c

noinst_HEADERS = ifconfig.h flags.h options.h printif.h system.h
