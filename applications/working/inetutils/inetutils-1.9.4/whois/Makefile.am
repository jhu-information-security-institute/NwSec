#
# Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
# 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

AM_CPPFLAGS = $(iu_INCLUDES)

LDADD = $(iu_LIBRARIES)

bin_PROGRAMS = $(whois_BUILD)

EXTRA_PROGRAMS = whois

whois_SOURCES = whois.c

EXTRA_DIST = as_del_list ip_del_list tld_serv_list \
  make_as_del.pl  make_ip_del.pl make_tld_serv.pl \
  as_del.h ip_del.h tld_serv.h

noinst_HEADERS = data.h as_del.h ip_del.h tld_serv.h whois.h

BUILT_SOURCES = as_del.h ip_del.h tld_serv.h

as_del.h: $(srcdir)/as_del_list $(srcdir)/make_as_del.pl
	LANG=C perl $(srcdir)/make_as_del.pl < $(srcdir)/as_del_list > as_del.h

ip_del.h: $(srcdir)/ip_del_list $(srcdir)/make_ip_del.pl
	LANG=C perl $(srcdir)/make_ip_del.pl < $(srcdir)/ip_del_list > ip_del.h

tld_serv.h: $(srcdir)/tld_serv_list $(srcdir)/make_tld_serv.pl
	LANG=C perl $(srcdir)/make_tld_serv.pl < $(srcdir)/tld_serv_list > tld_serv.h
