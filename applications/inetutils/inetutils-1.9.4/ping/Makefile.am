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

AM_CPPFLAGS = \
	$(iu_INCLUDES) \
	-I$(top_srcdir)/libicmp \
	$(INCIDN)

LDADD = \
	$(iu_LIBRARIES) \
	$(LIBIDN)

bin_PROGRAMS = $(ping_BUILD) $(ping6_BUILD)

EXTRA_PROGRAMS = ping ping6

ping_LDADD = $(top_builddir)/libicmp/libicmp.a $(LDADD)

ping_SOURCES = ping.c ping_common.c ping_echo.c ping_address.c \
  ping_router.c ping_timestamp.c ping_common.h  ping_impl.h ping.h libping.c
ping6_SOURCES = ping6.c ping_common.c ping_common.h ping6.h

SUIDMODE = -o root -m 4755

install-ping-hook:
	-@for x in $(bin_PROGRAMS); do \
	$(INSTALL_PROGRAM) $$x $(SUIDMODE) $(AM_INSTALL_PROGRAM_FLAGS) $(DESTDIR)$(bindir)/`echo $$x|sed '$(transform)'` ; \
	if test $$? -ne 0; then \
	  echo "WARNING: insufficient access; not installing $$x"; \
	  echo "NOTE: To install $$x run 'make install' as root"; \
	fi \
	done

install-exec-hook: $(ping_INSTALL_HOOK)
