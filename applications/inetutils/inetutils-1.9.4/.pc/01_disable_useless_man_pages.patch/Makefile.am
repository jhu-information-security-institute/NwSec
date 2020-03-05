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

ACLOCAL_AMFLAGS = -I am -I m4

EXTRA_DIST = paths ChangeLog.0 summary.sh.in CHECKLIST

SUBDIRS = lib \
	libinetutils libtelnet libicmp libls \
	src telnet telnetd ftp ftpd talk talkd whois ping ifconfig \
	doc man \
	tests

DISTCLEANFILES = pathdefs.make paths.defs \
	$(top_builddir)/.version \
	$(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION).tar.xz

BUILT_SOURCES = $(top_builddir)/.version
$(top_builddir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-`date +"%Y%m%d"`
