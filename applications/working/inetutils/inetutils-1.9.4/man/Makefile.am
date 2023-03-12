#
# Copyright (C) 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

all = hostname.1 dnsdomainname.1 ifconfig.1 inetd.8 ftp.1 ftpd.8	\
      logger.1 ping.1 ping6.1 rcp.1 rexec.1 rexecd.8 rlogin.1		\
      rlogind.8 rsh.1 rshd.8 syslogd.8 talk.1 talkd.8 telnet.1		\
      telnetd.8 tftp.1 tftpd.8 traceroute.1 uucpd.8 whois.1

dist_man_MANS =

if ENABLE_hostname
dist_man_MANS += hostname.1
endif

if ENABLE_dnsdomainname
dist_man_MANS += dnsdomainname.1
endif

if ENABLE_ifconfig
dist_man_MANS += ifconfig.1
endif

if ENABLE_inetd
dist_man_MANS += inetd.8
endif

if ENABLE_ftp
dist_man_MANS += ftp.1
endif

if ENABLE_ftpd
dist_man_MANS += ftpd.8
endif

if ENABLE_logger
dist_man_MANS += logger.1
endif

if ENABLE_ping
dist_man_MANS += ping.1
endif

if ENABLE_ping6
dist_man_MANS += ping6.1
endif

if ENABLE_rcp
dist_man_MANS += rcp.1
endif

if ENABLE_rexec
dist_man_MANS += rexec.1
endif

if ENABLE_rexecd
dist_man_MANS += rexecd.8
endif

if ENABLE_rlogin
dist_man_MANS += rlogin.1
endif

if ENABLE_rlogind
dist_man_MANS += rlogind.8
endif

if ENABLE_rsh
dist_man_MANS += rsh.1
endif

if ENABLE_rshd
dist_man_MANS += rshd.8
endif

if ENABLE_syslogd
dist_man_MANS += syslogd.8
endif

if ENABLE_talk
dist_man_MANS += talk.1
endif

if ENABLE_talkd
dist_man_MANS += talkd.8
endif

if ENABLE_telnet
dist_man_MANS += telnet.1
endif

if ENABLE_telnetd
dist_man_MANS += telnetd.8
endif

if ENABLE_tftp
dist_man_MANS += tftp.1
endif

if ENABLE_tftpd
dist_man_MANS += tftpd.8
endif

if ENABLE_traceroute
dist_man_MANS += traceroute.1
endif

if ENABLE_uucpd
dist_man_MANS += uucpd.8
endif

if ENABLE_whois
dist_man_MANS += whois.1
endif

EXTRA_DIST = $(all) $(all:.1=.h2m) $(all:.8=.h2m)

MAINTAINERCLEANFILES = $(all)

ftp.1: ftp.h2m $(top_srcdir)/ftp/main.c $(top_srcdir)/configure.ac

ftpd.8: ftpd.h2m $(top_srcdir)/ftpd/ftpd.c $(top_srcdir)/configure.ac

hostname.1: hostname.h2m $(top_srcdir)/src/hostname.c $(top_srcdir)/configure.ac

dnsdomainname.1: dnsdomainname.h2m $(top_srcdir)/src/dnsdomainname.c $(top_srcdir)/configure.ac

ifconfig.1: ifconfig.h2m $(top_srcdir)/ifconfig/options.c $(top_srcdir)/configure.ac

inetd.8: inetd.h2m $(top_srcdir)/src/inetd.c $(top_srcdir)/configure.ac

logger.1: logger.h2m $(top_srcdir)/src/logger.c $(top_srcdir)/configure.ac

ping.1: ping.h2m $(top_srcdir)/ping/ping.c $(top_srcdir)/configure.ac

ping6.1: ping6.h2m $(top_srcdir)/ping/ping6.c $(top_srcdir)/configure.ac

rcp.1: rcp.h2m $(top_srcdir)/src/rcp.c $(top_srcdir)/configure.ac

rexec.1: rexec.h2m $(top_srcdir)/src/rexec.c $(top_srcdir)/configure.ac

rexecd.8: rexecd.h2m $(top_srcdir)/src/rexecd.c $(top_srcdir)/configure.ac

rlogin.1: rlogin.h2m $(top_srcdir)/src/rlogin.c $(top_srcdir)/configure.ac

rlogind.8: rlogind.h2m $(top_srcdir)/src/rlogind.c $(top_srcdir)/configure.ac

rsh.1: rsh.h2m $(top_srcdir)/src/rsh.c $(top_srcdir)/configure.ac

rshd.8: rshd.h2m $(top_srcdir)/src/rshd.c $(top_srcdir)/configure.ac

syslogd.8: syslogd.h2m $(top_srcdir)/src/syslogd.c $(top_srcdir)/configure.ac

talk.1: talk.h2m $(top_srcdir)/talk/talk.c $(top_srcdir)/configure.ac

talkd.8: talkd.h2m $(top_srcdir)/talkd/talkd.c $(top_srcdir)/configure.ac

telnet.1: telnet.h2m $(top_srcdir)/telnet/main.c $(top_srcdir)/configure.ac

telnetd.8: telnetd.h2m $(top_srcdir)/telnetd/telnetd.c $(top_srcdir)/configure.ac

tftp.1: tftp.h2m $(top_srcdir)/src/tftp.c $(top_srcdir)/configure.ac

tftpd.8: tftpd.h2m $(top_srcdir)/src/tftpd.c $(top_srcdir)/configure.ac

traceroute.1: traceroute.h2m $(top_srcdir)/src/traceroute.c $(top_srcdir)/configure.ac

uucpd.8: uucpd.h2m $(top_srcdir)/src/uucpd.c $(top_srcdir)/configure.ac

whois.1: whois.h2m $(top_srcdir)/whois/whois.c $(top_srcdir)/configure.ac

.h2m.1:
	$(MAKE) man TOOL=$* SECTION=1

.h2m.8:
	$(MAKE) man TOOL=$* SECTION=8

mapped_name = `echo ../$(TOOL)/$(TOOL) \
| sed s,../ping6/ping6,../ping/ping6,\
| sed s,../hostname/hostname,../src/hostname,\
| sed s,../dnsdomainname/dnsdomainname,../src/dnsdomainname,\
| sed s,../inetd/inetd,../src/inetd,\
| sed s,../logger/logger,../src/logger,\
| sed s,../rcp/rcp,../src/rcp,\
| sed s,../rexec/rexec,../src/rexec,\
| sed s,../rexecd/rexecd,../src/rexecd,\
| sed s,../rlogin/rlogin,../src/rlogin,\
| sed s,../rlogind/rlogind,../src/rlogind,\
| sed s,../rsh/rsh,../src/rsh,\
| sed s,../rshd/rshd,../src/rshd,\
| sed s,../syslogd/syslogd,../src/syslogd,\
| sed s,../tftp/tftp,../src/tftp,\
| sed s,../tftpd/tftpd,../src/tftpd,\
| sed s,../traceroute/traceroute,../src/traceroute,\
| sed s,../uucpd/uucpd,../src/uucpd,`

.PHONY: man
man:
	$(HELP2MAN)					\
		--source='$(PACKAGE_STRING)'		\
		--include=$(srcdir)/$(TOOL).h2m		\
		--output=$(TOOL).$(SECTION)		\
		--section $(SECTION)			\
		$(mapped_name)
