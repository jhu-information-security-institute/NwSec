# configure.ac - Configuration for inetutils
# Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
# 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015
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

AC_PREREQ(2.59)

AC_INIT([GNU inetutils],
 m4_esyscmd([build-aux/git-version-gen .tarball-version 's/inetutils-/v/;s/_/./g']),
 [bug-inetutils@gnu.org])

AC_CONFIG_SRCDIR([src/inetd.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h:config.hin])
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([1.11.1 dist-xz -Wall -Werror])

# For now, we always use libexecdir; later we may want to vary where
# the daemons go.
inetdaemondir='$(libexecdir)'
AC_SUBST(inetdaemondir)

AC_ARG_WITH([krb4],
            AS_HELP_STRING([--with-krb4[[=PATH]]],
                           [Compile with Kerberos IV]),
            [KERBEROS_VERSION=krb4
             KERBEROS_DIR=$withval])
AC_ARG_WITH([krb5],
            AS_HELP_STRING([--with-krb5[[=PATH]]],
                           [Compile with Kerberos V]),
            [KERBEROS_VERSION=krb5
             KERBEROS_DIR=$withval])
AC_ARG_WITH([shishi],
            AS_HELP_STRING([--with-shishi[[=PATH]]],
		           [Compile with Shishi (Kerberos 5)]),
            [KERBEROS_VERSION=Shishi
             KERBEROS_DIR=$withval])

AC_ARG_ENABLE([servers],
              AS_HELP_STRING([--disable-servers],
                             [Do not compile servers]),
	      , [enable_servers=yes])
AC_ARG_ENABLE([clients],
              AS_HELP_STRING([--disable-clients],
                             [Do not compile clients]),
	      , [enable_clients=yes])
AC_ARG_ENABLE([libls],
              AS_HELP_STRING([--disable-libls],
                             [Disable support for libls ]),
              , [enable_libls=yes])
AC_ARG_ENABLE([encryption],
              AS_HELP_STRING([--enable-encryption],
                             [Enable encryption]),
              [test -z "$KERBEROS_VERSION" && AC_MSG_ERROR([--enable-encryption is given, but a Kerberos version is not provided])],
              [test -z "$KERBEROS_VERSION" && enable_encryption=no])
AC_ARG_ENABLE([authentication],
              AS_HELP_STRING([--enable-authentication],
                             [Enable connection authentication]),
              [test -z "$KERBEROS_VERSION" && AC_MSG_ERROR([--enable-authentication is given, but a Kerberos version is not provided])],
	      [test -z "$KERBEROS_VERSION" && enable_authentication=no])

if test -n "$KERBEROS_VERSION"; then
  test -n "$enable_authentication" || enable_authentication=yes
  test -n "$enable_encryption" || enable_encryption=yes
fi

AC_ARG_WITH([wrap],
            AS_HELP_STRING([--with-wrap],
                           [Add tcp wrapper support]))
AC_ARG_WITH([pam],
            AS_HELP_STRING([--with-pam],
                           [Add support for PAM]))
AC_ARG_WITH([idn],
  AS_HELP_STRING([--with-idn[[=DIR]]],
		 [Use IDN support, with optional inclusion directory]), ,
		 [with_idn=no])
AC_ARG_WITH([libidn-prefix],
  AS_HELP_STRING([--with-libidn-prefix[[=DIR]]],
		 [Base directory, probes DIR/include and DIR/lib]), ,
		 [with_libidn_prefix=no])

IU_ENABLE_SERVER(ftpd)
IU_ENABLE_SERVER(inetd)
IU_ENABLE_SERVER(rexecd)
IU_ENABLE_SERVER(rlogind)
IU_ENABLE_SERVER(rshd)
IU_ENABLE_SERVER(syslogd)
IU_ENABLE_SERVER(talkd)
IU_ENABLE_SERVER(telnetd)
IU_ENABLE_SERVER(tftpd)
IU_ENABLE_SERVER(uucpd)

IU_ENABLE_CLIENT(ftp)
IU_ENABLE_CLIENT(dnsdomainname)
IU_ENABLE_CLIENT(hostname)
IU_ENABLE_CLIENT(ping)
IU_ENABLE_CLIENT(ping6)
IU_ENABLE_CLIENT(rcp)
IU_ENABLE_CLIENT(rexec)
IU_ENABLE_CLIENT(rlogin)
IU_ENABLE_CLIENT(rsh)
IU_ENABLE_CLIENT(logger)
IU_ENABLE_CLIENT(talk)
IU_ENABLE_CLIENT(telnet)
IU_ENABLE_CLIENT(tftp)
IU_ENABLE_CLIENT(whois)
IU_ENABLE_CLIENT(ifconfig)
IU_ENABLE_CLIENT(traceroute)

AM_SILENT_RULES([yes])

AC_ISC_POSIX

### Checks for programs.
AC_PROG_CC
gl_EARLY
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_CHECK_TOOL(AR, ar)
AC_PATH_PROG(DD, dd, dd)
AC_PATH_PROG(MKTEMP, mktemp, mktemp)
AC_PATH_PROG(NETSTAT, netstat, netstat)
AC_PATH_PROG(RM, rm, rm)
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_YACC
AC_PROG_LN_S
AC_PROG_SED
AM_MISSING_PROG(HELP2MAN, help2man, $missing_dir)
AC_ARG_VAR(GREP, [Location of preferred 'grep' utility.])
AC_ARG_VAR(SED, [Location of preferred 'sed' utility.])
AC_ARG_VAR(DD, [Location of 'dd'.])
AC_ARG_VAR(MKTEMP, [Location of 'mktemp'.])
AC_ARG_VAR(NETSTAT, [Location of 'netstat'.])
AC_ARG_VAR(TARGET, [IP address used while testing. @<:@127.0.0.1@:>@])
AC_ARG_VAR(TARGET6, [IPv6 address used while testing. @<:@::1@:>@])

gl_INIT

### Checks for libraries.
AC_SEARCH_LIBS([inet_ntoa], [nsl])
AC_SEARCH_LIBS([getpeername], [socket])

# See if a termcap library is available (under one of several names)
IU_LIB_TERMCAP

# Find libreadline -- the ftp client will be built without
# command editing and history capability if readline() cannot
# be found, neither in libreadline, nor in libedit.
gl_FUNC_READLINE

# See if there's a separate libcrypt (many systems put crypt there).
AC_CHECK_LIB(crypt, crypt, LIBCRYPT=-lcrypt)
AC_SUBST(LIBCRYPT)

# libutil is a 4.4BSD library that defines several functions handy for
# inetutils daemons, doing such things as utmp frobbing, and pty
# setup.
AC_CHECK_LIB(util, login, LIBUTIL=-lutil)
AC_CHECK_LIB(util, loginx, LIBUTIL=-lutil)
AC_CHECK_LIB(util, logout, LIBUTIL=-lutil)
AC_CHECK_LIB(util, logoutx, LIBUTIL=-lutil)
AC_CHECK_LIB(util, logwtmp, LIBUTIL=-lutil)
AC_CHECK_LIB(util, logwtmpx, LIBUTIL=-lutil)
AC_SUBST(LIBUTIL)

# FreeBSD 9.0 has changed access to wtmp system, abolishing
# logout(), logwtmp(), etcetera.  We need to use LIBUTIL
# for rpl_openpty() and forkpty() in libgnu.a.
if test "$ac_cv_func_login_tty" = yes \
   || test "$ac_cv_have_decl_openpty" = yes; then
  # Make sure that linking contains `-lutil'.
  if test -z "$LIBUTIL"; then
    LIBUTIL=-lutil
  fi
fi

# Check if they want support for PAM.  Certain daemons like ftpd have
# support for it.

# See if we have libpam.a.  Investigate PAM versus Linux-PAM.
if test "$with_pam" = yes ; then
  AC_CHECK_LIB(dl, dlopen, LIBDL=-ldl)
  AC_CHECK_LIB(pam, pam_authenticate, LIBPAM=-lpam)
  if test "$ac_cv_lib_pam_pam_authenticate" = yes ; then
    AC_DEFINE([WITH_PAM], 1, [Define to one if you have -lpam.])
    # ftpd was originally coded for Linux-PAM and was later
    # adapted to go with OpenPAM and with Solaris-PAM.
    FTPD_LIBPAM=$LIBPAM
    AC_CHECK_DECLS(PAM_CONV_AGAIN, , , [#include <security/pam_appl.h>])
    if test "$ac_cv_have_decl_PAM_CONV_AGAIN" = yes ; then
      AC_DEFINE([WITH_LINUX_PAM], 1, [Define to one if you use Linux-PAM.])
    fi
  fi
fi
AC_SUBST(LIBDL)
AC_SUBST(LIBPAM)
AC_SUBST(FTPD_LIBPAM)

# Can we use libls? but we must have fchdir()
if test "$enable_libls" = yes && test "$ac_cv_func_fchdir" = yes ; then
  LIBLS="../libls/libls.a"
  libls_BUILD="libls.a"
  AC_DEFINE([WITH_LIBLS], 1, [Define to one if you have -lls])
else
  LIBLS=''
  libls_BUILD=''
  enable_libls='no'
fi
AC_SUBST(LIBLS)
AC_SUBST(libls_BUILD)
AC_SUBST(enable_libls)
AM_CONDITIONAL([ENABLE_libls], [test "$enable_libls" = yes])

# At least OpenSolaris is missing <protocols/talkd.h>.
AC_CHECK_HEADER(protocols/talkd.h, , ,
  [IU_FLUSHLEFT([#include <sys/types.h>
		 #include <sys/socket.h>])])

if test "$ac_cv_header_protocols_talkd_h" = no; then
  AC_MSG_WARN([protocols/talkd.h is not available, not building talk, nor talkd])
  IU_DISABLE_TARGET(talk)
  IU_DISABLE_TARGET(talkd)
fi

# Only talk uses curses, so only check for it if we want that
if test "$enable_talk" != no; then
  IU_LIB_CURSES
fi
if test "$enable_talk" != no && test "$LIBCURSES"; then
  enable_talk=yes
else
  if test "$enable_talk" = yes; then
    AC_MSG_WARN([curses is not available, so not building talk])
  fi
  IU_DISABLE_TARGET(talk)
fi

# NetBSD offers orcmd() and orcmd_af() for getting
# exactly the functionality that rcmd() and rcmd_af()
# offer on all other targetted systems.
AC_CHECK_FUNC(rcmd_af, , , [#include <netdb.h>])
if test "$ac_cv_func_rcmd_af" = yes; then
  AC_DEFINE([WITH_RCMD_AF], 1, [Define to one if you can use rcmd_af.])
fi
AC_CHECK_FUNC(orcmd, , , [#include <unistd.h>])
if test "$ac_cv_func_orcmd" = yes; then
  AC_DEFINE([WITH_ORCMD], 1, [Define to one if you can use orcmd.])
fi
AC_CHECK_FUNC(orcmd_af, , , [#include <unistd.h>])
if test "$ac_cv_func_orcmd_af" = yes; then
  AC_DEFINE([WITH_ORCMD_AF], 1, [Define to one if you can use orcmd_af.])
fi
AC_CHECK_FUNC(rresvport_af, , , [#include <netdb.h>])
if test "$ac_cv_func_rresvport_af" = yes; then
  AC_DEFINE([WITH_RRESVPORT_AF], 1,
	    [Define to one if you can use rresvport_af.])
fi

# FIXME: Presently rshd and rlogind prefer iruserok(3), which is not
# universally available.  Replacement code?  A fallback is being built
# from ruserok(3) and inet_ntoa(3).  Calls using iruserok/rusersok are
# used for non-PAM configured executables rshd/rlogind.
AC_CHECK_FUNC(iruserok, , , [#include <netdb.h>])
AC_CHECK_FUNC(iruserok_af, , , [#include <netdb.h>])
AC_CHECK_FUNC(iruserok_sa, , , [#include <netdb.h>])
AC_CHECK_FUNC(ruserok, , , [#include <netdb.h>])
AC_CHECK_FUNC(ruserok_af, , , [#include <netdb.h>])
if test "$ac_cv_func_iruserok" = yes; then
  AC_DEFINE([WITH_IRUSEROK], 1, [Define to one if you can use iruserok.])
  if test "$ac_cv_func_iruserok_af" = yes; then
    AC_DEFINE([WITH_IRUSEROK_AF], 1,
	      [Define to one if you can use iruserok_af.])
  fi
  if test "$ac_cv_func_iruserok_sa" = yes; then
    AC_DEFINE([WITH_IRUSEROK_SA], 1,
	      [Define to one if you can use iruserok_sa.])
  fi
else
  if test "$ac_cv_func_ruserok" = yes; then
    AC_DEFINE([WITH_RUSEROK], 1, [Define to one if you can use ruserok.])
    AC_MSG_WARN([iruserok is not available, falling back to ruserok.])
    if test "$ac_cv_func_ruserok_af" = yes; then
      AC_DEFINE([WITH_RUSEROK_AF], 1,
		[Define to on if you can use ruserok_af.])
    fi
  else
    AC_MSG_WARN([Disabling rlogind and rshd, since no iruserok and no ruserok.])
    IU_DISABLE_TARGET(rlogind)
    IU_DISABLE_TARGET(rshd)
  fi
fi


# FIXME: Make this test case a bit more robust.  Remove KRB4
LIBAUTH=
INCAUTH=
if test "$enable_encryption" = yes \
   || test "$enable_authentication" = yes; then
  AH_TEMPLATE(KERBEROS, [Define to one if you have Kerberos])
  AH_TEMPLATE(KRB4, [Define to one if you have Kerberos IV])
  AH_TEMPLATE(KRB5, [Define to one if you have Kerberos V])
  AH_TEMPLATE(SHISHI, [Define to 1 if you have Shishi])
  AH_TEMPLATE(DES_ENCRYPTION, [Define to 1 if you have DES encryption])

  case $KERBEROS_VERSION in
  krb4)
    AC_CHECK_LIB(krb, krb_mk_req,
                 LIBAUTH=-lkrb
                  AC_DEFINE([KRB4]),
                 -L$KERBEROS_DIR)
    if test ".$LIBAUTH" != . ; then
      AC_CHECK_LIB(des, des_key_sched,
                   [LIBAUTH="$LIBAUTH -ldes"],
                   -L$KERBEROS_DIR)
      # FIXME: Fix this to one test case.
      test "$enable_authentication" = yes \
      && test "$ac_cv_lib_krb_krb_mk_req" = yes \
      && AC_DEFINE([KRB4])

      test "$enable_authentication" = yes \
      && test "$ac_cv_lib_krb_krb_mk_req" = yes \
      && AC_DEFINE([KERBEROS])

      test "$enable_encryption" = yes \
      && test "$ac_cv_lib_des_des_key_sched" = yes \
      && AC_DEFINE([DES_ENCRYPTION])

      AC_CHECK_HEADERS([com_err.h des.h kerberosIV/des.h \
			kerberosIV/krb.h krb.h])
    fi
    ;;
  krb5)
    IU_CHECK_KRB5([5],$KERBEROS_DIR)
    LIBAUTH="$KRB5_LIBS" # FIXME
    INCAUTH="$KRB5_CFLAGS"
    if test ".$LIBAUTH" != . ;then
      AC_DEFINE([KERBEROS])
      AC_DEFINE([KRB5])
      save_CPPFLAGS=$CPPFLAGS
      CPPFLAGS="$CPPFLAGS $INCAUTH"
      AC_CHECK_HEADERS([com_err.h kerberosV/krb5.h krb5.h \
			krb5/asn1.h krb5/crc-32.h krb5/ext-proto.h \
			krb5/krb5.h krb5/los-proto.h])
      CPPFLAGS=$save_CPPFLAGS
      # We have limited support for krcmd() with Kerberos5.
      # Encryption must be sorted out as a first step.
      IU_DISABLE_TARGET(rcp)
      IU_DISABLE_TARGET(rlogin)
      IU_DISABLE_TARGET(rsh)
      # Likewise, we need to migrate away from KRB4 and des_*()
      IU_DISABLE_TARGET(rlogind)
      IU_DISABLE_TARGET(rshd)
    fi
    ;;
  *)
    # Properly handle --without-shishi.
    if test x$KERBEROS_DIR = xno; then
      KERBEROS_VERSION=
    else
      if test x$KERBEROS_DIR != x \
	  && test x$KERBEROS_DIR != xyes; then
	LIBAUTH=-L$KERBEROS_DIR/lib
	INCAUTH=-I$KERBEROS_DIR/include
      fi
      LIBAUTH="$LIBAUTH -lshishi"
      save_LIBS=$LIBS
      LIBS="$LIBS $LIBAUTH"
      save_CPPFLAGS=$CPPFLAGS
      CPPFLAGS="$CPPFLAGS $INCAUTH"
      AC_CHECK_HEADER(shishi.h,
	AC_SEARCH_LIBS([shishi_check_version], [shishi],
	  AC_DEFINE([SHISHI]),
	  [INCAUTH= LIBAUTH= KERBEROS_VERSION=]),
	[INCAUTH= LIBAUTH= KERBEROS_VERSION=])
      LIBS=$save_LIBS
      CPPFLAGS=$save_CPPFLAGS
    fi
  esac
fi
AC_SUBST(LIBAUTH)
AC_SUBST(INCAUTH)
AC_SUBST(KERBEROS_VERSION)

# Define CPP macros that enable auth/encrypt code
if test ".$LIBAUTH" != .; then
  test "$enable_authentication" = yes && AC_DEFINE([AUTHENTICATION], 1,
                   [Define to one if you want authentication.])
  test "$enable_encryption" = yes     && AC_DEFINE([ENCRYPTION], 1,
                   [Define to one if you want encryption.])
fi

# Checks for IPv6 support.

AC_ARG_ENABLE([ipv6],
  AS_HELP_STRING([--disable-ipv6], [disable IPv6 support]),
  [case "${enable_ipv6}" in
  no)
    AC_MSG_NOTICE([Disabling IPv6 at user request])
    ipv6=no
    ;;
  *)
    ipv6=yes
    ;;
  esac],
  [ipv6=auto]
)
AC_SUBST(TEST_IPV6, [$ipv6])

working_ipv6=yes
if test ! "$ipv6" = "no" && test "$working_ipv6" = "yes"; then
  IU_CHECK_MACRO(AF_INET6, [#include <sys/socket.h>], , , working_ipv6=no)
fi

if test ! "$ipv6" = "no" && test "$working_ipv6" = "yes"; then
  IU_CHECK_MACRO(IPV6_V6ONLY, [#include <netinet/in.h>], , , working_ipv6=no)
fi

if test ! "$ipv6" = "no" && test "$working_ipv6" = "yes"; then
  AC_CHECK_TYPE(struct sockaddr_storage, , working_ipv6=no, [#include <sys/socket.h>])
fi

if test ! "$ipv6" = "no" && test "$working_ipv6" = "yes"; then
  AC_CHECK_TYPE(struct sockaddr_in6, , working_ipv6=no, [#include <sys/types.h>
#include <netinet/in.h>])
fi

if test ! "$ipv6" = "no" && test "$working_ipv6" = "yes"; then
  AC_CHECK_TYPE(struct addrinfo, , working_ipv6=no, [#include <netdb.h>])
fi

if test ! "$ipv6" = "no" && test "$working_ipv6" = "yes"; then
  AC_CHECK_FUNC(getnameinfo, ,working_ipv6=no, [#include <netdb.h>])
fi

if test ! "$ipv6" = "no" && test "$working_ipv6" = "yes"; then
  AC_DEFINE([IPV6], 1, [Define to one if you want IPv6.])
else
  if test "$ipv6" = "yes"; then
    AC_MSG_FAILURE([IPv6 support not available])
  else
    if test "$ipv6" = "auto"; then
      AC_MSG_WARN([IPv6 support not available, disabling IPv6])
    fi
  fi
fi

if test "$ipv6" = "no" || test "$working_ipv6" = "no";then
  ping6_BUILD=''
else
  working_icmp6=yes
  AC_CHECK_HEADER(netinet/icmp6.h, , working_icmp6=no,
    [IU_FLUSHLEFT([#include <sys/types.h>
		   #include <sys/socket.h>
		   #include <netinet/in.h>])])

  if test "$working_icmp6" = "yes"; then
    AC_CHECK_HEADER(netinet/ip6.h, , working_icmp6=no,
      [IU_FLUSHLEFT([#include <sys/types.h>
		     #include <sys/socket.h>
		     #include <netinet/in.h>])])

  fi

  if test "$working_icmp6" = "yes"; then
    AC_CHECK_TYPE(struct icmp6_filter, , working_icmp6=no,
      [IU_FLUSHLEFT([#include <sys/types.h>
		     #include <sys/socket.h>
		     #include <netinet/in.h>
		     #include <netinet/icmp6.h>])])
  fi

  if test "$working_icmp6" = "yes"; then
    AC_CHECK_TYPE(struct icmp6_hdr, , working_icmp6=no,
      [IU_FLUSHLEFT([#include <sys/types.h>
		     #include <sys/socket.h>
		     #include <netinet/in.h>
		     #include <netinet/icmp6.h>])])
  fi

  if test "$working_icmp6" = "yes"; then
    IU_CHECK_MACRO(IPV6_HOPLIMIT, [#include <netinet/in.h>], ,
		   [have_hoplimit=yes], [have_hoplimit=no])
    IU_CHECK_MACRO(IPV6_2292HOPLIMIT, [#include <netinet/in.h>], ,
		   [have_2292hoplimit=yes], [have_2292hoplimit=no])
    IU_CHECK_MACRO(IPV6_RECVHOPLIMIT, [#include <netinet/in.h>], ,
		   [have_recvhoplimit=yes], [have_recvhoplimit=no])

    # Reset, ping6 can't compile without HOPLIMIT/RECVHOPLIMIT (or
    # similar).
    working_icmp6=no

    if test $have_recvhoplimit = yes && test $have_hoplimit = yes; then
      # ping6 uses HOPLIMIT and RECVLIMIT, if those exist, happy happy
      # joy joy.
      working_icmp6=yes
    fi

    if test $have_2292hoplimit = yes && test $have_hoplimit = no; then
      # Some systems (BSDs) define these macros prefixed with "2292",
      # violating RFC 2292; work around it.
      AC_DEFINE([IPV6_HOPLIMIT], IPV6_2292HOPLIMIT,
                [Define to IPV6_2292HOPLIMIT if not available])
      have_hoplimit=yes
      working_icmp6=yes
    fi

    if test $have_recvhoplimit = no && test $have_hoplimit = yes; then
      # There exist strange systems (FIXME: which ones?) where
      # RECVHOPLIMIT might not be defined, but HOPLIMIT is.  So define
      # that.
      AC_DEFINE([IPV6_RECVHOPLIMIT], IPV6_HOPLIMIT,
                [Define to IPV6_HOPLIMIT if not available])
      working_icmp6=yes
    fi

    # If we got this far then we are SOL, and the reset above takes
    # care of the rest.
  fi

  if test "$working_icmp6" = "yes"; then
    IU_CHECK_MACRO(ICMP6_DST_UNREACH_BEYONDSCOPE,
                   [#include <sys/types.h>
		    #include <sys/socket.h>
		    #include <netinet/in.h>
		    #include <netinet/icmp6.h>], ,
		   [have_beyondscope=yes], [have_beyondscope=no])
    if test "$have_beyondscope" = "no"; then
      IU_CHECK_MACRO(ICMP6_DST_UNREACH_NOTNEIGHBOR, [#include <netinet/icmp6.h>], , [
        AC_DEFINE([ICMP6_DST_UNREACH_BEYONDSCOPE], ICMP6_DST_UNREACH_NOTNEIGHBOR,
                  [Define to ICMP6_DST_UNREACH_NOTNEIGHBOR if not available])
      ], [working_icmp6=no])
    fi
  fi

  if test "$working_icmp6" = "no"; then
    AC_MSG_WARN([ICMPv6 support not available, disabling ping6])
    ping6_BUILD=''
  fi
fi


# Check if they want support for Wrap.  Certain daemons like ftpd have
# support for it.

if test "$with_wrap" = yes; then
# See if we have libwrap.a.  We cannot use AC_CHECK_LIB reliably since
# for the wrap lib you have to define some global variables.
#AC_CHECK_LIB(wrap, hosts_ctl, LIBWRAP=-lwrap)
  AC_CHECK_HEADERS([tcpd.h])
  AC_CHECK_DECLS([hosts_ctl], , , [#include <tcpd.h>])
  AC_MSG_CHECKING(hosts_ctl in -lwrap);
  save_LIBS=$LIBS
  LIBS="$save_LIBS -lwrap"
  AC_TRY_LINK([int allow_severity = 1; int deny_severity = 1;],
              hosts_ctl(), eval "ac_cv_lib_wrap_hosts_ctl=yes",
              eval "ac_cv_lib_wrap_hosts_ctl=no")
  LIBS=$save_LIBS
  if test "$ac_cv_lib_wrap_hosts_ctl" = yes \
      && test "$ac_cv_header_tcpd_h" = yes; then
    AC_DEFINE([WITH_WRAP], 1, [Define to one if you have -lwrap and <tcpd.h>.])
    LIBWRAP=-lwrap
  fi
  AC_MSG_RESULT($ac_cv_lib_wrap_hosts_ctl);
fi
AC_SUBST(LIBWRAP)

### Check for GNU Libidn
AC_CHECK_HEADERS([locale.h])
AC_CHECK_FUNCS([setlocale])

IU_CHECK_LIBIDN([$with_libidn_prefix], [$with_idn])

### Checks for header files.

AC_HEADER_STDC
AC_HEADER_TIME
AC_HEADER_DIRENT

AC_CHECK_HEADERS([arpa/nameser.h errno.h fcntl.h features.h \
		  glob.h memory.h netinet/ether.h netinet/in_systm.h \
		  netinet/ip.h netinet/ip_icmp.h netinet/ip_var.h \
		  security/pam_appl.h shadow.h \
		  stdarg.h stdlib.h string.h stropts.h sys/tty.h \
		  sys/utsname.h sys/ptyvar.h sys/msgbuf.h sys/filio.h \
		  sys/ioctl_compat.h sys/cdefs.h sys/stream.h sys/mkdev.h \
		  sys/sockio.h sys/sysmacros.h sys/param.h sys/file.h \
		  sys/proc.h sys/select.h sys/time.h sys/wait.h \
                  sys/resource.h \
		  stropts.h tcpd.h utmp.h utmpx.h unistd.h \
                  vis.h], [], [], [
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#include <netinet/in.h>
#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
]

)

### Checks for types.

# Checks return type of signal handlers
AC_TYPE_SIGNAL

AC_CHECK_TYPE(socklen_t, ,
  [AC_DEFINE([socklen_t], int,
    [Define to int if <sys/types.h> does not define])],
  [IU_FLUSHLEFT([#include <sys/types.h>
                 #include <sys/socket.h>])])

AC_CHECK_TYPES(struct if_nameindex, , ,
  [IU_FLUSHLEFT([#include <sys/types.h>
                 #include <sys/socket.h>
                 #include <net/if.h>])])

# sigset_t is a set of signals (4.4BSD)
AC_CHECK_TYPE(sigset_t, ,
  [AC_DEFINE([sigset_t], unsigned long,
    [Define to `unsigned long' if not defined in <signal.h>])],
  [IU_FLUSHLEFT([#include <sys/types.h>
                 #include <signal.h> ])])

# I'm not sure what this is, but netbsd has it; just use int if
# necessary.
AC_CHECK_TYPE(sig_atomic_t, ,
  [AC_DEFINE([sig_atomic_t], int,
    [Define to `int' if not defined in <signal.h>])],
  [IU_FLUSHLEFT([#include <sys/types.h>
                 #include <signal.h> ])])

### Checks for structures.
IU_CHECK_MEMBERS([struct sockaddr_in.sin_len], , ,
    [ #include <sys/types.h>
      #include <netinet/in.h> ])
IU_CHECK_MEMBERS([struct sockaddr_in6.sin6_len], , ,
    [ #include <sys/types.h>
      #include <sys/socket.h>
      #include <netinet/in.h> ])
IU_CHECK_MEMBERS([struct sockaddr_storage.ss_len], , ,
    [ #include <sys/types.h>
      #include <sys/socket.h>
      #include <netinet/in.h> ])

IU_CHECK_MEMBERS([struct passwd.pw_expire, struct passwd.pw_change],
    [], [],
    [ #include <sys/types.h>
      #include <pwd.h>])

if test "$ac_cv_header_utmp_h" = yes; then
  AC_CHECK_TYPES(struct lastlog, , , [#include <utmp.h>])
  IU_CHECK_MEMBERS([struct utmp.ut_type, struct utmp.ut_pid,
		    struct utmp.ut_line, struct utmp.ut_id,
		    struct utmp.ut_user, struct utmp.ut_host,
		    struct utmp.ut_exit, struct utmp.ut_session,
		    struct utmp.ut_tv, struct utmp.ut_time,
		    struct utmp.ut_addr_v6, struct utmp.ut_name], , ,
		   [#include <sys/types.h>
		    #include <utmp.h>])
fi
if test "$ac_cv_header_utmpx_h" = yes; then
  AC_CHECK_TYPES(struct lastlogx, , , [#include <utmp.h>])
  IU_CHECK_MEMBERS([struct lastlogx.ll_ss], , ,
		   [#include <sys/types.h>
		    #include <sys/utmpx.h>])
  IU_CHECK_MEMBERS([struct utmpx.ut_user, struct utmpx.ut_name,
		    struct utmpx.ut_id, struct utmpx.ut_line,
		    struct utmpx.ut_pid, struct utmpx.ut_type,
		    struct utmpx.ut_exit, struct utmpx.ut_tv,
		    struct utmpx.ut_session, struct utmpx.ut_syslen,
		    struct utmpx.ut_host, struct utmpx.ut_ss,
		    struct utmpx.ut_addr_v6], , , [#include <utmpx.h>])
fi

IU_CHECK_MEMBERS([struct ifreq.ifr_index,
		  struct ifreq.ifr_netmask,
		  struct ifreq.ifr_broadaddr,
		  struct ifreq.ifr_mtu,
		  struct ifreq.ifr_map], , ,
		 [#include <sys/types.h>
		  #include <sys/socket.h>
		  #include <net/if.h>])

IU_CHECK_MEMBERS([struct sockaddr.sa_len], , ,
                 [#include <sys/types.h>
                  #include <sys/socket.h>])
IU_CHECK_MEMBERS([struct hostent.h_addr_list], , , [#include <netdb.h>])

IU_CHECK_MEMBERS([struct stat.st_atim.tv_nsec,
		  struct stat.st_atim.tv_usec,
		  struct stat.st_blksize,
		  struct stat.st_ctim.tv_nsec,
		  struct stat.st_ctim.tv_usec,
		  struct stat.st_mtim.tv_nsec,
		  struct stat.st_mtim.tv_usec], , ,
		 [#include <sys/types.h>
		  #include <sys/stat.h>])

# OpenSolaris does not use a union for `struct tftphdr.th_u'.  As a
# consequence `struct tftphdr.th_stuff' is a macro resolving to a
# `ushort_t'.  BSD and Linux produce `char *'.
IU_CHECK_MEMBERS([struct tftphdr.th_u], , ,
		 [#include <sys/types.h>
		  #include <sys/socket.h>
		  #include <arpa/tftp.h>])

### Checks for compiler characteristics.
AC_C_CONST

# See if `weak refs' are possible; these make it possible (with shared
# libraries) to check for the existance of a standard function at
# run-time instead of compile time, which is very handy for
# distributing binary version programs that automatically adapt -- in
# inetutils case, to whether or not crypt is available.
IU_CHECK_WEAK_REFS

### Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_SETVBUF_REVERSED
AC_FUNC_FORK
AC_FUNC_STRCOLL
AC_FUNC_MMAP

AC_CHECK_FUNCS(cfsetspeed cgetent dirfd fchdir flock \
               fork fpathconf ftruncate \
               getcwd getmsg getpwuid_r getspnam getutxent getutxuser \
               initgroups initsetproctitle killpg \
               ptsname pututline pututxline \
               setegid seteuid setpgid setlogin \
               setsid setregid setreuid setresgid setresuid setutent_r \
               sigaction sigvec strchr setproctitle tcgetattr tzset utimes \
               utime uname \
               updwtmp updwtmpx vhangup wait3 wait4 __opendir2 \
	       __rcmd_errstr __check_rhosts_file )

# Variant functions for user accounting.
# These need $LIBUTIL for linking.
_SAVE_LIBS="$LIBS"
LIBS="$LIBS $LIBUTIL"
AC_CHECK_FUNCS(login loginx logout logoutx logwtmp logwtmpx)
LIBS="$_SAVE_LIBS"

# Functions that we will define if necessary.
AC_REPLACE_FUNCS(getpass getusershell memcmp memcpy memmove memset \
		 strdup poll)

# Look for the crypt function itself (in libcrypt if possible)
_SAVE_LIBS="$LIBS"
LIBS="$LIBCRYPT $LIBS"
AC_CHECK_FUNCS(crypt)
LIBS="$_SAVE_LIBS"

AC_CHECK_FUNC(_obstack_free)

# Use (posix) `setpgid' if it's around, otherwise assume a BSD setpgrp
AC_CHECK_FUNC(setpgid, , AC_DEFINE([setpgid], setpgrp,
                          [Define to `setpgrp' if you don't have `setpgid'.]))

# We use our own version of getopt (including our own header file) if
# the system one doesn't have getopt_long.
AC_CHECK_FUNC(getopt_long)

AC_CHECK_HEADER(poll.h)

# See if snprintf exists, otherwise just use a bogus version
AC_CHECK_FUNC(snprintf,
  [AC_DEFINE([HAVE_SNPRINTF], 1, [Define to one if you have snprintf])])

# See if vsnprintf exists, otherwise use our own.
AC_CHECK_FUNC(vsnprintf,
  [AC_DEFINE([HAVE_VSNPRINTF], 1, [Define to one if you have vsnprintf])])

# See if the system has strerror, and replace it if not
AC_CHECK_FUNC(strerror)
if test "$ac_cv_func_strerror" = no; then
  # No strerror, so see if the SYS_ERRLIST variable can be used by
  # ours
  AC_CHECK_FUNC(sys_errlist, AC_DEFINE([HAVE_SYS_ERRLIST], 1,
                              [Define to one if you have sys_errlist.]))
  if test "$ac_cv_func_sys_errlist" = yes; then
    AC_CHECK_DECL(sys_errlist, , ,
      [IU_FLUSHLEFT([#include <stdio.h>
                    #include <errno.h>])])
  fi
fi

# See if the system has hstrerror, and replace it if not
save_LIBS=$LIBS

AC_CHECK_FUNC(hstrerror)
if test "$ac_cv_func_hstrerror" != yes; then
  # Is hstrerror contained in a separate library?
  AC_CHECK_LIB([resolv], [hstrerror], LIBRESOLV=-lresolv)
  LIBS="$LIBRESOLV $LIBS"
fi
if test "$ac_cv_func_hstrerror" = yes; then
  AC_CHECK_DECL(hstrerror, , , [#include <netdb.h>])
else
  # No hstrerror, so see if the H_ERRLIST variable can be used by ours
  AC_CHECK_FUNC(h_errlist)
  if test "$ac_cv_func_h_errlist" = yes; then
    AC_CHECK_DECL(h_errlist, , , [#include <netdb.h>])
  fi
fi
if test "$ac_cv_func_hstrerror" = yes \
   || test "$ac_cv_func_h_errlist" = yes; then
  # If there's a system hstrerror, or we can reasonably replace it,
  # say so.  We still provide some definition, regardless, but this
  # allows people to use a reasonable alternative if the situation
  # allows, rather than using a degenerate version that only says
  # `Host lookup error N'.
  AC_DEFINE([HAVE_HSTRERROR], 1, [Define to one if you have hstrerror.])
fi
LIBS=$save_LIBS
AC_SUBST(LIBRESOLV)

# OpenSolaris does not provide an external reference.
AC_CHECK_DECLS(telcmds, , ,
  [IU_FLUSHLEFT([#undef TELCMDS
                 #include <arpa/telnet.h>])])

# NetBSD does include an external reference.
AC_CHECK_DECLS(telopts, , ,
  [IU_FLUSHLEFT([#undef TELOPTS
                 #include <arpa/telnet.h>])])

# OpenSolaris provides Schedule and Session_Key.
AC_CHECK_TYPE(Schedule,
  AC_DEFINE([HAVE_ARPA_TELNET_H_SCHEDULE], 1,
	    [Define to one if <arpa/telnet.h> defines a type Schedule.]), ,
  [#include <arpa/telnet.h>])
AC_CHECK_TYPE(Session_Key,
  AC_DEFINE([HAVE_ARPA_TELNET_H_SESSION_KEY], 1,
	    [Define to 1 if <arpa/telnet.h> defines a type Session_Key.]), ,
  [#include <arpa/telnet.h>])

# At least NetBSD demands `const char []'.
AC_MSG_CHECKING([whether enctype_names is const char])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
     [[#include <arpa/telnet.h>
       const char *enctype_names[] = { "ANY", "CFB64", 0 };]],
     [[ return 0;]])],
  [AC_DEFINE([HAVE_CONST_CHAR_ENCTYPE_NAMES], 1,
	     [Define to 1 if enctype_names[] must be const char.])
   ac_iu_const_enctype_names=yes],
  [ac_iu_const_enctype_names=no])
AC_MSG_RESULT([$ac_iu_const_enctype_names])

## Checks for function declarations.

AC_DECL_SYS_SIGLIST

AC_CHECK_DECLS(crypt, , , [#include <unistd.h>])

# EWOULDBLOCK is more or less the BSD version of posix EAGAIN.
IU_CHECK_MACRO(EWOULDBLOCK, [#include <errno.h>], , ,
  [AC_DEFINE([EWOULDBLOCK], EAGAIN, 1,
     [Define to EAGAIN if system does not provide EWOULDBLOCK])])

AC_CHECK_DECLS(errno, , , [#include <errno.h>])

# See whether <syslog.h> will declare special internal stuff if we
# define SYSLOG_NAMES before including it.  We check for various
# syslog internal macros, the typedef `CODE', and the variable
# `prioritynames'.
define([iu_syslog_includes],
  [[#include <stdlib.h>
    #define SYSLOG_NAMES
    #include <syslog.h> ]])
IU_CHECK_MACRO(syslog internal macros,
  iu_syslog_includes,
  [LOG_PRI LOG_FAC],
  AC_CHECK_TYPE(CODE,
    AC_CHECK_DECL(prioritynames,
      [AC_DEFINE([HAVE_SYSLOG_INTERNAL], 1,
        [Define to 1 if <syslog.h> defines prioritynames])], ,
      iu_syslog_includes), , iu_syslog_includes),)
undefine([iu_syslog_includes])

# Some systems don't declare common functions (especially if they
# return int), at least in the expected header file.  Check.
AC_CHECK_DECLS([fclose, pclose], , , [#include <stdio.h>])
AC_CHECK_DECLS([getcwd, getlogin, getpass, getusershell, ttyname], , ,
               [#include <unistd.h>])

AC_CHECK_DECLS([getgrnam, initgroups], , , [#include <grp.h>])

AC_CHECK_DECLS(htons, , ,
  [IU_FLUSHLEFT([# include <sys/types.h>
                 #ifdef HAVE_SYS_PARAM_H
                 # include <sys/param.h>
                 #endif
                 #include <netinet/in.h> ])])

# Our home-grown version of setenv needs to use ENVIRON
AC_CHECK_DECLS(environ, , ,
  [IU_FLUSHLEFT([#include <unistd.h>
                 #include <stdlib.h>])])

# See if we have h_errno (the test is here so we can use -lresolv if
# necessary).
AC_CACHE_CHECK([for h_errno], inetutils_cv_var_h_errno,
  AC_TRY_COMPILE([#include <netdb.h>],
    [ extern int h_errno; int iu_x = h_errno; ],
    inetutils_cv_var_h_errno=yes, inetutils_cv_var_h_errno=no))
if test "$inetutils_cv_var_h_errno" = yes; then
  # FIXME: Isn't this wrong? Shouldn't the declaration check define
  #        this if we find h_errno?
  AC_DEFINE([HAVE_H_ERRNO], 1, [Define to one if you have h_errno.])
  AC_CHECK_DECL(h_errno, , , [#include <netdb.h>])
fi

## Checks for CPP macros.

# Look for the posix SEEK_ macros (for lseek), and if not found, try
# the similar berkeley L_ macros; if neither can be found, use the
# classic unix values.
IU_CHECK_MACRO(SEEK_ macros,
  [#include <unistd.h>], SEEK_SET SEEK_CUR SEEK_END,
  :,
  IU_CHECK_MACRO(L_ seek macros,
    [#include <unistd.h>], L_SET L_INCR L_XTND,
    AC_DEFINE([SEEK_SET], L_SET, [Define to L_SET as replacement])
    AC_DEFINE([SEEK_CUR], L_INCR, [Define to L_INCR as replacement])
    AC_DEFINE([SEEK_END], L_XTND, [Define to L_XTND as replacement]),
    AC_DEFINE([SEEK_SET], 0, [Define to 0 if missing])
    AC_DEFINE([SEEK_CUR], 1, [Define to 1 if missing])
    AC_DEFINE([SEEK_END], 2, [Define to 2 if missing])))

# Look for the posix _FILENO macros; if not found, use the classic
# unix values.
IU_CHECK_MACRO(_FILENO macros,
  [#include <unistd.h>], STDIN_FILENO STDOUT_FILENO STDERR_FILENO,
  :,
  AC_DEFINE([STDIN_FILENO], 0, [Define to 0 if missing])
  AC_DEFINE([STDOUT_FILENO], 1, [Define to 1 if missing])
  AC_DEFINE([STDERR_FILENO], 2, [Define to 2 if missing]))

# See where to find fd_set (bit-vectors for select) manipulation
# macros.
IU_CHECK_MACRO(fd_set macros, [#include <sys/types.h>],
  FD_ZERO FD_CLR FD_SET FD_ISSET)
if test "$inetutils_cv_macro_fd_set_macros" = no; then
  IU_CHECK_MACRO(fd_set macros in <sys/time.h>, [#include <sys/time.h>],
    FD_ZERO FD_CLR FD_SET FD_ISSET)
fi

# Set convenience variables for inetutils.
iu_LIBRARIES='$(top_builddir)/libinetutils/libinetutils.a $(top_builddir)/lib/libgnu.a'
iu_INCLUDES='-I$(top_srcdir)/lib -I$(top_builddir)/lib -I$(top_srcdir)/libinetutils'
AC_SUBST(iu_LIBRARIES)
AC_SUBST(iu_INCLUDES)

# Configure paths used by inetutils programs.  See the file `paths'
# for details.
PATHDEFS_MAKE=pathdefs.make
PATHS_DEFS=paths.defs
IU_CONFIG_PATHS($srcdir/paths, PATHDEFS_MAKE, PATHS_DEFS)
AH_BOTTOM(
[/* Defaults for PATH_ variables.  */
#include <confpaths.h>])

# Ok this is the end of the rope, even autoconf can't help.  For
# example on certain system login(1) will not work if we don't have an
# entry in utmpx.  Also for some OS they are better handled with
# STREAMS.
case "$host" in
*-freebsd9* | *-freebsd10* )
  # FreeBSD 9.0 has implemented a non-standard and singular
  # UTMPX interface.  The readutmp module from GNUlib does
  # not support it, but we can fall back to getutxuser() for
  # use in syslogd and talkd.  For ftpd, rlogind, telnetd,
  # and uucpd the use of pututxent() should suffice, but
  # this needs continued observations.
  #
  # Other issues?
  ;;
*olaris* )
  case "$host" in
  *olaris*10 | *olaris*11 )
    AC_DEFINE([SOLARIS10], 1, [Solaris 10 and newer variants.])
    ;;
  * )
    # Older Solaris variants.
    ;;
  esac
  AC_DEFINE([SOLARIS], 1, [Define to 1 for any Solaris system])
  AC_DEFINE([HAVE_STREAMSPTY], 1,
    [Define to 1 for a system using streams for ptys])
  ;;
*irix* | *hpux* )
  # Known and needed work-arounds?
  ;;
esac

# Typically the loopback addresses, but can be set otherwise.
TARGET=${TARGET:-127.0.0.1}
TARGET6=${TARGET6:-::1}

# Work around code peculiarities.
AM_CONDITIONAL([ENABLE_hstrerror], [test "$ipv6" = no])

AC_CONFIG_FILES([
Makefile
summary.sh
lib/Makefile
libinetutils/Makefile
libtelnet/Makefile
libicmp/Makefile
libls/Makefile
src/Makefile
telnet/Makefile
telnetd/Makefile
ftp/Makefile
ftpd/Makefile
talk/Makefile
talkd/Makefile
whois/Makefile
ping/Makefile
ifconfig/Makefile
ifconfig/system/Makefile
doc/Makefile
man/Makefile
tests/Makefile
confpaths.h:confpaths.h.in
])

AC_CONFIG_COMMANDS([summary], [sh ./summary.sh])

# Collect printable information on each
# available (i.e., reportable) target.
#
# Group similar properties and append
# by succession in an orderly manner.

# Kerberised services.

rcp_PROPS=$KERBEROS_VERSION
rlogin_PROPS=$KERBEROS_VERSION
rlogind_PROPS=$KERBEROS_VERSION
rsh_PROPS=$KERBEROS_VERSION
rshd_PROPS=$KERBEROS_VERSION
telnet_PROPS=$KERBEROS_VERSION
telnetd_PROPS=$KERBEROS_VERSION

# PAM enabled services.

ftpd_PROPS="${ftpd_PROPS:+$ftpd_PROPS }${FTPD_LIBPAM:+PAM}"
rexecd_PROPS="${rexecd_PROPS:+$rexecd_PROPS }${LIBPAM:+PAM}"
rlogind_PROPS="${rlogind_PROPS:+$rlogind_PROPS }${LIBPAM:+PAM}"
rshd_PROPS="${rshd_PROPS:+$rshd_PROPS }${LIBPAM:+PAM}"

# Support for libwrap.

ftpd_PROPS="${ftpd_PROPS:+$ftpd_PROPS }$LIBWRAP"
rlogind_PROPS="${rlogind_PROPS:+$rlogind_PROPS }$LIBWRAP"

# Support for libidn.

ftp_PROPS="${ftp_PROPS:+$ftp_PROPS }${LIBIDN:+-lidn}"
ping_PROPS="${ping_PROPS:+$ping_PROPS }${LIBIDN:+-lidn}"
talk_PROPS="${talk_PROPS:+$talk_PROPS }${LIBIDN:+-lidn}"
telnet_PROPS="${telnet_PROPS:+$telnet_PROPS }${LIBIDN:+-lidn}"
tftp_PROPS="${tftp_PROPS:+$tftp_PROPS }${LIBIDN:+-lidn}"
traceroute_PROPS="${traceroute_PROPS:+$traceroute_PROPS }${LIBIDN:+-lidn}"

# Termcap variants.

telnet_PROPS="${telnet_PROPS:+$telnet_PROPS }$LIBTERMCAP"
telnetd_PROPS="${telnetd_PROPS:+$telnetd_PROPS }$LIBTERMCAP"

# Readline variants.

ftp_PROPS="${ftp_PROPS:+$ftp_PROPS }$LIBREADLINE"

# Variants of libcurses.

talk_PROPS="${talk_PROPS:+$talk_PROPS }$LIBCURSES"

AC_OUTPUT
