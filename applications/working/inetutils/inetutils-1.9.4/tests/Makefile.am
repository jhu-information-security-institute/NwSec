#
# Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015
# Free Software Foundation, Inc.
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

noinst_PROGRAMS = identify
identify_LDADD =

check_PROGRAMS = localhost readutmp waitdaemon

dist_check_SCRIPTS = utmp.sh

if ENABLE_inetd
check_PROGRAMS += addrpeek tcpget
endif

if ENABLE_libls
noinst_PROGRAMS += ls
ls_LDADD = $(LIBLS) $(iu_LIBRARIES)
dist_check_SCRIPTS += libls.sh
endif

if ENABLE_ping
dist_check_SCRIPTS += ping-localhost.sh
endif
if ENABLE_traceroute
dist_check_SCRIPTS += traceroute-localhost.sh
endif
if ENABLE_inetd
if ENABLE_tftpd
if ENABLE_tftp
dist_check_SCRIPTS += tftp.sh
endif
endif
endif
if ENABLE_logger
if ENABLE_syslogd
dist_check_SCRIPTS += syslogd.sh
endif
endif

if ENABLE_ftp
dist_check_SCRIPTS += ftp-parser.sh
endif

if ENABLE_inetd
if ENABLE_ftp
if ENABLE_ftpd
dist_check_SCRIPTS += ftp-localhost.sh
endif
endif
endif

if ENABLE_inetd
if ENABLE_telnet
dist_check_SCRIPTS += inetd.sh telnet-localhost.sh
endif
endif

if ENABLE_hostname
dist_check_SCRIPTS += hostname.sh
endif

if ENABLE_dnsdomainname
dist_check_SCRIPTS += dnsdomainname.sh
endif

if ENABLE_ifconfig
dist_check_SCRIPTS += ifconfig.sh
endif

TESTS = localhost waitdaemon $(dist_check_SCRIPTS)

TESTS_ENVIRONMENT = EXEEXT=$(EXEEXT)

EXTRA_DIST = tools.sh.in ifconfig_modes.sh

BUILT_SOURCES = tools.sh

CLEANFILES = tools.sh

tools_subst = sed -e 's,[@]GREP[@],$(GREP),g' \
	      -e 's,[@]SED[@],$(SED),g' \
	      -e 's,[@]DD[@],$(DD),g' \
	      -e 's,[@]MKTEMP[@],$(MKTEMP),g' \
	      -e 's,[@]NETSTAT[@],$(NETSTAT),g'\
	      -e 's,[@]TARGET[@],$(TARGET),g'\
	      -e 's,[@]TARGET6[@],$(TARGET6),g'\
	      -e 's,[@]TEST_IPV6[@],$(TEST_IPV6),g'

tools.sh: tools.sh.in Makefile
	$(tools_subst) < $(srcdir)/tools.sh.in > $@
