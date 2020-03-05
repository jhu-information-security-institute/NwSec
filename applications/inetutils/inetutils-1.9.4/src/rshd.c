/*
  Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
  2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013,
  2014, 2015 Free Software Foundation, Inc.

  This file is part of GNU Inetutils.

  GNU Inetutils is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or (at
  your option) any later version.

  GNU Inetutils is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see `http://www.gnu.org/licenses/'. */

/*
 * Copyright (c) 1988, 1989, 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * PAM implementation by Mats Erik Andersson.
 *
 * TODO: Check cooperation between PAM and Shishi/Kerberos.
 *
 * Sample settings:
 *
 *   GNU/Linux, et cetera:
 *
 * auth      required    pam_nologin.so
 * auth      required    pam_rhosts.so
 * auth      required    pam_env.so
 * auth      required    pam_group.so
 * account   required    pam_nologin.so
 * account   required    pam_unix.so
 * session   required    pam_unix.so
 * session   required    pam_lastlog.so silent
 * password  required    pam_deny.so
 *
 *   OpenSolaris:
 *
 * auth      required    pam_rhosts_auth.so
 * auth      required    pam_unix_cred.so
 * account   required    pam_roles.so
 * account   required    pam_unix_account.so
 * session   required    pam_unix_session.so
 * password  required    pam_deny.so
 *
 *   BSD:
 *
 * auth      required    pam_nologin.so     # NetBSD
 * auth      required    pam_rhosts.so
 * account   required    pam_nologin.so     # FreeBSD
 * account   required    pam_unix.so
 * session   required    pam_lastlog.so
 * password  required    pam_deny.so
 */

/*
 * remote shell server exchange protocol (server view!):
 *	[port]\0
 *	remuser\0
 *	locuser\0
 *	command\0
 *	data
 *
 * Kerberized exchange delays the remote user name:
 *
 *      \0
 *      locuser\0
 *      command\0
 *      remuser\0
 *      data
 */

#include <config.h>

#include <alloca.h>

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>	/* n_time */
#endif
#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h>	/* IPOPT_* */
#endif

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#include <pwd.h>
#ifdef HAVE_SHADOW_H
# include <shadow.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <grp.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <error.h>
#include <progname.h>
#include <argp.h>
#include <unused-parameter.h>
#include <libinetutils.h>
#include "xalloc.h"

#ifdef HAVE_SECURITY_PAM_APPL_H
# include <security/pam_appl.h>
#endif

#ifdef KRB4
# ifdef HAVE_KERBEROSIV_DES_H
#  include <kerberosIV/des.h>
# endif
# ifdef HAVE_KERBEROSIV_KRB_H
#  include <kerberosIV/krb.h>
# endif
#elif defined KRB5	/* !KRB4 */
# ifdef HAVE_KRB5_H
#  include <krb5.h>
# endif
# ifdef HAVE_COM_ERR_H
#  include <com_err.h>
# endif
#endif /* KRB4 || KRB5 */

#ifdef SHISHI
# include <shishi.h>
# include <shishi_def.h>
#endif

#ifndef MAX
# define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef DAY
# define DAY (24 * 60 * 60)
#endif

int keepalive = 1;		/* flag for SO_KEEPALIVE scoket option */
int check_all;
int log_success;		/* If TRUE, log all successful accesses */
int reverse_required = 0;	/* Demand IP to host name resolution.  */
int sent_null;

void doit (int, struct sockaddr *, socklen_t);
void rshd_error (const char *, ...);
char *getstr (const char *);
int local_domain (const char *);
const char *topdomain (const char *);

#ifdef WITH_PAM
static int pam_rc = PAM_AUTH_ERR;

static pam_handle_t *pam_handle = NULL;
static int rsh_conv (int, const struct pam_message **,
		     struct pam_response **, void *);
static struct pam_conv pam_conv = { rsh_conv, NULL };
#endif /* WITH_PAM */

#if defined KERBEROS || defined SHISHI
# ifdef KRB4
Key_schedule schedule;
char authbuf[sizeof (AUTH_DAT)];
char tickbuf[sizeof (KTEXT_ST)];
# elif defined KRB5
# elif defined(SHISHI)
Shishi *h;
Shishi_ap *ap;
Shishi_key *enckey;
shishi_ivector iv1, iv2, iv3, iv4;
shishi_ivector *ivtab[4];
int protocol, uses_encryption = 0;
# endif /* SHISHI */
# define VERSION_SIZE	9
# define SECURE_MESSAGE  "This rsh session is using DES encryption for all transmissions.\r\n"
int doencrypt, use_kerberos, vacuous;
char *servername = NULL;
#else
#endif /* KERBEROS || SHISHI */

static struct argp_option options[] = {
#define GRP 10
  { "reverse-required", 'r', NULL, 0,
    "require reverse resolving of remote host IP", GRP },
  { "verify-hostname", 'a', NULL, 0,
    "ask hostname for verification", GRP },
#ifdef HAVE___CHECK_RHOSTS_FILE
  { "no-rhosts", 'l', NULL, 0,
    "ignore .rhosts file", GRP },
#endif
  { "no-keepalive", 'n', NULL, 0,
    "do not set SO_KEEPALIVE", GRP },
  { "log-sessions", 'L', NULL, 0,
    "log successful logins", GRP },
#undef GRP
#if defined KERBEROS || defined SHISHI
# define GRP 20
  /* FIXME: The option semantics does not match that of other r* utilities */
  { "kerberos", 'k', NULL, 0,
    "use kerberos authentication", GRP },
  /* FIXME: Option name is misleading */
  { "vacuous", 'v', NULL, 0,
    "fail for non-Kerberos authentication", GRP },
  { "server-principal", 'S', "NAME", 0,
    "set Kerberos server name, overriding canonical hostname", GRP },
# if defined ENCRYPTION
  { "encrypt", 'x', NULL, 0,
    "fail for non-encrypted, Kerberized sessions", GRP },
# endif
# undef GRP
#endif /* KERBEROS || SHISHI */
  { NULL, 0, NULL, 0, NULL, 0 }
};

#ifdef HAVE___CHECK_RHOSTS_FILE
extern int __check_rhosts_file;	/* hook in rcmd(3) */
#endif

#ifndef WITH_PAM
# if defined __GLIBC__ && defined WITH_IRUSEROK
extern int iruserok (uint32_t raddr, int superuser,
                     const char *ruser, const char *luser);
# endif
#endif /* WITH_PAM */

static error_t
parse_opt (int key, char *arg, struct argp_state *state _GL_UNUSED_PARAMETER)
{
  switch (key)
    {
    case 'a':
      check_all = 1;
      break;

#ifdef HAVE___CHECK_RHOSTS_FILE
    case 'l':
      __check_rhosts_file = 0;	/* don't check .rhosts file */
      break;
#endif

    case 'n':
      keepalive = 0;	/* don't enable SO_KEEPALIVE */
      break;

    case 'r':
      reverse_required = 1;
      break;

#if defined KERBEROS || defined SHISHI
    case 'k':
      use_kerberos = 1;
      break;

    case 'v':
      vacuous = 1;
      break;

# ifdef ENCRYPTION
    case 'x':
      doencrypt = 1;
      break;
# endif

    case 'S':
      servername = arg;
      break;
#endif /* KERBEROS || SHISHI */

    case 'L':
      log_success = 1;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}


const char doc[] =
#ifdef WITH_PAM
# if defined KERBEROS || defined SHISHI
  "Remote shell server, using PAM services 'rsh' and 'krsh'.";
# else
  "Remote shell server, using PAM service 'rsh'.";
# endif
#else /* !WITH_PAM */
  "Remote shell server.";
#endif
static struct argp argp = { options, parse_opt, NULL, doc, NULL, NULL, NULL};


/* Remote shell server. We're invoked by the rcmd(3) function. */
int
main (int argc, char *argv[])
{
  int index;
  struct linger linger;
  int on = 1;
  socklen_t fromlen;
  struct sockaddr_storage from;
  int sockfd;

  set_program_name (argv[0]);
  iu_argp_init ("rshd", default_program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  openlog ("rshd", LOG_PID | LOG_ODELAY, LOG_DAEMON);

  argc -= index;
  if (argc > 0)
    {
      syslog (LOG_ERR, "%d extra arguments", argc);
      exit (EXIT_FAILURE);
    }

#if defined KERBEROS || defined SHISHI
  if (use_kerberos && vacuous)
    {
      syslog (LOG_ERR, "only one of -k and -v allowed");
      exit (EXIT_FAILURE);
    }
# ifdef ENCRYPTION
  if (doencrypt && !use_kerberos)
    {
      syslog (LOG_ERR, "-k is required for -x");
      exit (EXIT_FAILURE);
    }
# endif /* ENCRYPTION */
#endif /* KERBEROS || SHISHI */

  /*
   * We assume we're invoked by inetd, so the socket that the
   * connection is on, is open on descriptors 0, 1 and 2.
   * STD{IN,OUT,ERR}_FILENO.
   * We may in the future make it standalone for certain platform.
   */
  sockfd = STDIN_FILENO;

  /*
   * First get the Internet address of the client process.
   * This is requored for all the authentication we perform.
   */

  fromlen = sizeof from;
  if (getpeername (sockfd, (struct sockaddr *) &from, &fromlen) < 0)
    {
      syslog (LOG_ERR, "getpeername: %m");
      _exit (EXIT_FAILURE);
    }

  /* Set the socket options: SO_KEEPALIVE and SO_LINGER */
  if (keepalive && setsockopt (sockfd, SOL_SOCKET, SO_KEEPALIVE, (char *) &on,
			       sizeof on) < 0)
    syslog (LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
  linger.l_onoff = 1;
  linger.l_linger = 60;		/* XXX */
  if (setsockopt (sockfd, SOL_SOCKET, SO_LINGER, (char *) &linger,
		  sizeof linger) < 0)
    syslog (LOG_WARNING, "setsockopt (SO_LINGER): %m");
  doit (sockfd, (struct sockaddr *) &from, fromlen);
  return 0;
}

char username[32 + sizeof ("USER=")] = "USER=";
char logname[32 + sizeof ("LOGNAME=")] = "LOGNAME=";
char homedir[256 + sizeof ("HOME=")] = "HOME=";
char shell[64 + sizeof ("SHELL=")] = "SHELL=";
char path[sizeof (PATH_DEFPATH) + sizeof ("PATH=")] = "PATH=";
char rhost[128 + sizeof ("RHOST=")] = "RHOST=";

#ifndef WITH_PAM
char *envinit[] = { homedir, shell, path, logname, username, rhost, NULL };
#endif
extern char **environ;

void
doit (int sockfd, struct sockaddr *fromp, socklen_t fromlen)
{
#ifdef HAVE___RCMD_ERRSTR
  extern char *__rcmd_errstr;	/* syslog hook from libc/net/rcmd.c. */
#endif
#ifdef HAVE_GETPWNAM_R
  char *pwbuf;
  int ret, pwbuflen;
  struct passwd *pwd, pwstor;
#else /* !HAVE_GETPWNAM_R */
  struct passwd *pwd;
#endif
  unsigned short port, inport;
  fd_set ready, readfrom;
  int cc, nfd, pv[2], pid, s = sockfd;
  int rc, one = 1;
  char portstr[8], addrstr[INET6_ADDRSTRLEN];
#if HAVE_DECL_GETNAMEINFO
  char addrname[NI_MAXHOST];
#else /* !HAVE_DECL_GETNAMEINFO */
  struct hostent *hp;
#endif
  const char *hostname, *errorstr, *errorhost = NULL;
  char *cp, sig, buf[BUFSIZ];
  char *cmdbuf, *locuser, *remuser;
  char *rprincipal = NULL;
#if defined WITH_IRUSEROK_AF && !defined WITH_PAM
  void * fromaddrp;	/* Pointer to remote address.  */
#endif
#ifdef WITH_PAM
  char *service;
#endif

#ifdef	KERBEROS
# ifdef KRB4
  AUTH_DAT *kdata = (AUTH_DAT *) NULL;
  KTEXT ticket = (KTEXT) NULL;
  char instance[INST_SZ], version[VERSION_SIZE];
# elif defined KRB5	/* !KRB4 */
  krb5_context context;
  krb5_auth_context auth_ctx;
  krb5_authenticator *author;
  krb5_principal client;
  krb5_rcache rcache;
  krb5_keytab keytab;
  krb5_ticket *ticket;
# endif /* KRB4 || KRB5 */
  struct sockaddr_in fromaddr;
  long authopts;
  int pv1[2], pv2[2];
  fd_set wready, writeto;
#elif defined SHISHI /* !KERBEROS */
  int n;
  int pv1[2], pv2[2];
  fd_set wready, writeto;
  int keytype, keylen;
  int cksumtype;
  size_t cksumlen;
  char *cksum = NULL;
#endif /* KERBEROS || SHISHI */

#ifdef KERBEROS
  memcpy (&fromaddr, fromp, sizeof (fromaddr));
#endif

#ifdef HAVE_GETPWNAM_R
  pwbuflen = sysconf (_SC_GETPW_R_SIZE_MAX);
  if (pwbuflen <= 0)
    pwbuflen = 1024;	/* Guessing only.  */

  pwbuf = xmalloc (pwbuflen);
#endif /* HAVE_GETPWNAM_R */

  signal (SIGINT, SIG_DFL);
  signal (SIGQUIT, SIG_DFL);
  signal (SIGTERM, SIG_DFL);
#ifdef DEBUG
  {
    int t = open (PATH_TTY, O_RDWR);
    if (t >= 0)
      {
	ioctl (t, TIOCNOTTY, (char *) 0);
	close (t);
      }
  }
#endif

#if HAVE_DECL_GETNAMEINFO
  rc = getnameinfo (fromp, fromlen,
		    addrstr, sizeof (addrstr),
		    portstr, sizeof (portstr),
		    NI_NUMERICHOST | NI_NUMERICSERV);
  if (rc != 0)
    {
      syslog (LOG_WARNING, "getnameinfo: %s", gai_strerror (rc));
      exit (EXIT_FAILURE);
    }
  inport = atoi (portstr);
#else /* !HAVE_DECL_GETNAMEINFO */
  strncpy (addrstr, inet_ntoa (((struct sockaddr_in *) fromp)->sin_addr),
	   sizeof (addrstr));
  inport = ntohs (((struct sockaddr_in *) fromp)->sin_port);
  snprintf (portstr, sizeof (portstr), "%u", inport);
#endif

  /* Verify that the client's address is an Internet adress. */
#ifdef KERBEROS
  if (fromp->sa_family != AF_INET)
    {
      syslog (LOG_ERR, "malformed originating address (af %d)\n",
	      fromp->sa_family);
      exit (EXIT_FAILURE);
    }
#endif /* KERBEROS */
#ifdef IP_OPTIONS
  {
    unsigned char optbuf[BUFSIZ / 3], *cp;
    char lbuf[BUFSIZ], *lp;
    socklen_t optsize = sizeof (optbuf);
    int ipproto;
    struct protoent *ip;

    ip = getprotobyname ("ip");
    if (ip != NULL)
      ipproto = ip->p_proto;
    else
      ipproto = IPPROTO_IP;
    if (!getsockopt (sockfd, ipproto, IP_OPTIONS, (char *) optbuf,
		     &optsize) && optsize != 0)
      {
	lp = lbuf;
	/* The client has set IP options.  This isn't allowed.
	 * Use syslog() to record the fact.  Only the option
	 * types are printed, not their contents.
	 */
	for (cp = optbuf; optsize > 0; )
	  {
	    sprintf (lp, " %2.2x", *cp);
	    lp += 3;

	    if (*cp == IPOPT_SSRR || *cp == IPOPT_LSRR)
	      {
		/* Already the TCP handshake suffices for
		 * a man-in-the-middle attack vector.
		 */
		syslog (LOG_NOTICE,
			"Discarding connection from %s with set source routing",
			addrstr);
		exit (EXIT_FAILURE);
	      }
	    if (*cp == IPOPT_EOL)
	      break;
	    if (*cp == IPOPT_NOP)
	      cp++, optsize--;
	    else
	      {
		/* Options using a length octet, see RFC 791.  */
		int inc = cp[1];

		optsize -= inc;
		cp += inc;
	      }
	  }

	/* At this point presumably harmless options are present.
	 * Make a report about them, erase them, and continue.  */
	syslog (LOG_NOTICE,
		"Connection received from %s using IP options (erased):%s",
		addrstr, lbuf);

	/* Turn off the options.  If this doesn't work, we quit.  */
	if (setsockopt (sockfd, ipproto, IP_OPTIONS,
			(char *) NULL, optsize) != 0)
	  {
	    syslog (LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
	    exit (EXIT_FAILURE);
	  }
      }
  }
#endif

  /* Verify that the client's address was bound to a reserved port */
#if defined KERBEROS || defined SHISHI
  if (!use_kerberos)
#endif
    if (inport >= IPPORT_RESERVED || inport < IPPORT_RESERVED / 2)
      {
	syslog (LOG_NOTICE | LOG_AUTH,
		"Connection from %s on illegal port %s",
		addrstr, portstr);
	exit (EXIT_FAILURE);
      }

  /* Read the ASCII string specifying the secondary port# from
   * the socket.  We set a timer of 60 seconds to do this read,
   * else we assume something is wrong.  If the client doesn't want
   * the secondary port, they just send the terminating null byte.
   */
  alarm (60);
  port = 0;
  for (;;)
    {
      char c;

      cc = read (sockfd, &c, 1);
      if (cc != 1)
	{
	  if (cc < 0)
	    syslog (LOG_NOTICE, "read: %m");
	  shutdown (sockfd, SHUT_RDWR);
	  exit (EXIT_FAILURE);
	}
      /* null byte terminates the string */
      if (c == 0)
	break;
      port = port * 10 + c - '0';
    }

  alarm (0);
  if (port != 0)
    {
      /* If the secondary port# is non-zero, then we have to
       * connect to that port (which the client has already
       * created and is listening on).  The secondary port#
       * that the client tells us to connect to has also to be
       * a reserved port#.  Also, our end of this secondary
       * connection has also to have a reserved TCP port bound
       * to it, plus.
       */
      int lport = IPPORT_RESERVED - 1;
#ifdef WITH_RRESVPORT_AF
      s = rresvport_af (&lport, fromp->sa_family);
#else
      s = rresvport (&lport);
#endif
      if (s < 0)
	{
	  syslog (LOG_ERR, "can't get stderr port: %m");
	  exit (EXIT_FAILURE);
	}
#if defined KERBEROS || defined SHISHI
      if (!use_kerberos)
#endif
	if (port >= IPPORT_RESERVED || port < IPPORT_RESERVED / 2)
	  {
	    syslog (LOG_ERR, "Second port outside reserved range.");
	    exit (EXIT_FAILURE);
	  }
      /* Use the fromp structure that we already have available.
       * The 32-bit Internet address is obviously that of the
       * client; just change the port# to the one specified
       * as secondary port by the client.
       */
      switch (fromp->sa_family)
	{
	case AF_INET6:
	  ((struct sockaddr_in6 *) fromp)->sin6_port = htons (port);
	  break;
	case AF_INET:
	default:
	  ((struct sockaddr_in *) fromp)->sin_port = htons (port);
	}
      if (connect (s, fromp, fromlen) < 0)
	{
	  syslog (LOG_INFO, "connect second port %d: %m", port);
	  exit (EXIT_FAILURE);
	}
    }

#if defined KERBEROS || defined SHISHI
  if (vacuous)
    {
      rshd_error ("rshd: remote host requires Kerberos authentication\n");
      exit (EXIT_FAILURE);
    }
#endif /* KERBEROS || SHISHI */

  /* from inetd, socket is already on 0, 1, 2 */
  if (sockfd != STDIN_FILENO)
    {
      dup2 (sockfd, STDIN_FILENO);
      dup2 (sockfd, STDOUT_FILENO);
      dup2 (sockfd, STDERR_FILENO);
    }

  /* Get the "name" of the client from its Internet address.  This is
   * used for the authentication below.
   */
  errorstr = NULL;
#if HAVE_DECL_GETNAMEINFO
  rc = getnameinfo (fromp, fromlen, addrname, sizeof (addrname),
		    NULL, 0, NI_NAMEREQD);
  if (rc == 0)
    {
      hostname = addrname;
# if defined KERBEROS || defined SHISHI
      if (!use_kerberos)
# endif
	if (check_all || local_domain (addrname))
	  {
	    struct addrinfo hints, *ai, *res;

	    errorhost = addrname;
	    memset (&hints, 0, sizeof (hints));
	    hints.ai_family = fromp->sa_family;
	    hints.ai_socktype = SOCK_STREAM;

	    rc = getaddrinfo (hostname, NULL, &hints, &res);
	    if (rc != 0)
	      {
		syslog (LOG_INFO, "Could not resolve address for %s.",
			hostname);
		errorstr = "Could not resolve address for your host (%s).\n";
		hostname = addrstr;
	      }
	    else
	      {
		for (ai = res; ai; ai = ai->ai_next)
		  {
		    char astr[INET6_ADDRSTRLEN] = "";

		    if (getnameinfo (ai->ai_addr, ai->ai_addrlen,
				     astr, sizeof (astr),
				     NULL, 0, NI_NUMERICHOST))
		      continue;
		    if (!strcmp (addrstr, astr))
		      {
			hostname = addrname;
			break;	/* equal, OK */
		      }
		  }
		freeaddrinfo (res);
		if (ai == NULL)
		  {
		    syslog (LOG_NOTICE,
			    "Host addr %s not listed for host %s.",
			    addrstr, hostname);
		    errorstr = "Host address mismatch for %s.\n";
		    hostname = addrstr;
		  }
	      }
	  }
    }
#else /* !HAVE_DECL_GETNAMEINFO */
  switch (fromp->sa_family)
    {
    case AF_INET6:
      hp = gethostbyaddr ((void *) &((struct sockaddr_in6 *) fromp)->sin6_addr,
			  sizeof (struct in6_addr), fromp->sa_family);
      break;
    case AF_INET:
    default:
      hp = gethostbyaddr ((void *) &((struct sockaddr_in *) fromp)->sin_addr,
			  sizeof (struct in_addr), fromp->sa_family);
    }
  if (hp)
    {
      /*
       * If name returned by gethostbyaddr is in our domain,
       * attempt to verify that we haven't been fooled by someone
       * in a remote net; look up the name and check that this
       * address corresponds to the name.
       */
      hostname = strdup (hp->h_name);
# if defined KERBEROS || defined SHISHI
      if (!use_kerberos)
# endif
	if (check_all || local_domain (hp->h_name))
	  {
	    char *remotehost = alloca (strlen (hostname) + 1);
	    if (!remotehost)
	      errorstr = "Out of memory.\n";
	    else
	      {
		strcpy (remotehost, hostname);
		errorhost = remotehost;
		hp = gethostbyname (remotehost);
		if (hp == NULL)
		  {
		    syslog (LOG_INFO,
			    "Couldn't look up address for %s.", remotehost);
		    errorstr =
		      "Couldn't look up address for your host (%s).\n";
		    hostname = addrstr;
		  }
		else
		  for (;; hp->h_addr_list++)
		    {
		      if (hp->h_addr_list[0] == NULL)
			{
			  syslog (LOG_NOTICE,
				  "Host addr %s not listed for host %s.",
				  addrstr, hp->h_name);
			  errorstr = "Host address mismatch for %s.\n";
			  hostname = addrstr;
			  break;
			}
		      if (!memcmp (hp->h_addr_list[0],
				   (fromp->sa_family == AF_INET6)
				   ? (void *) & ((struct sockaddr_in6 *) fromp)->sin6_addr
				   : (void *) & ((struct sockaddr_in *) fromp)->sin_addr,
				   hp->h_length))
			{
			  hostname = strdup (hp->h_name);
			  break;	/* equal, OK */
			}
		    }
	      }
	  }
    }
#endif /* !HAVE_DECL_GETNAMEINFO */

  else if (reverse_required)
    {
      syslog (LOG_NOTICE,
	      "Could not resolve remote %s.", addrstr);
      rshd_error ("Permission denied.\n");
      exit (EXIT_FAILURE);
    }
  else
    errorhost = hostname = addrstr;

#ifdef KRB4
  if (use_kerberos)
    {
      kdata = (AUTH_DAT *) authbuf;
      ticket = (KTEXT) tickbuf;
      authopts = 0L;
      strcpy (instance, "*");
      version[VERSION_SIZE - 1] = '\0';
# ifdef ENCRYPTION
      if (doencrypt)
	{
	  struct sockaddr_in local_addr;
	  rc = sizeof local_addr;
	  if (getsockname (STDIN_FILENO,
			   (struct sockaddr *) &local_addr, &rc) < 0)
	    {
	      syslog (LOG_ERR, "getsockname: %m");
	      rshd_error ("rshd: getsockname: %s", strerror (errno));
	      exit (EXIT_FAILURE);
	    }
	  authopts = KOPT_DO_MUTUAL;
	  rc = krb_recvauth (authopts, 0, ticket,
			     "rcmd", instance, &fromaddr,
			     &local_addr, kdata, "", schedule, version);
	  des_set_key (kdata->session, schedule);
	}
      else
# endif /* ENCRYPTION */
	rc = krb_recvauth (authopts, 0, ticket, "rcmd",
			   instance, &fromaddr,
			   (struct sockaddr_in *) 0,
			   kdata, "", (bit_64 *) 0, version);
      if (rc != KSUCCESS)
	{
	  rshd_error ("Kerberos authentication failure: %s\n",
		      krb_err_txt[rc]);
	  exit (EXIT_FAILURE);
	}
    }
  else
#elif defined KRB5
  if (use_kerberos)
    {
      krb5_principal server;

      /* Set up context data.  */
      rc = krb5_init_context (&context);

      if (!rc && servername && *servername)
	{
	  rc = krb5_parse_name (context, servername, &server);

	  /* A realm name missing in `servername' has been augmented
	   * by krb5_parse_name(), so setting it again is harmless.
	   */
	  if (!rc)
	    {
	      rc = krb5_set_default_realm (context,
					   krb5_princ_realm
						(context, server)->data);
	      krb5_free_principal (context, server);
	    }
	}

      if (!rc)
        rc = krb5_auth_con_init (context, &auth_ctx);
      if (!rc)
	rc = krb5_auth_con_genaddrs (context, auth_ctx, sockfd,
			KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR);
      if (!rc)
	rc = krb5_auth_con_getrcache (context, auth_ctx, &rcache);

      if (!rc && !rcache)
	{
	  rc = krb5_sname_to_principal (context, 0, 0,
					KRB5_NT_SRV_HST, &server);
	  if (!rc)
	    {
	      krb5_data *pdata;

	      pdata = krb5_princ_component (context, server, 0);

	      rc = krb5_get_server_rcache (context, pdata, &rcache);
	      krb5_free_principal (context, server);

	      if (!rc)
		rc = krb5_auth_con_setrcache (context, auth_ctx, rcache);
	    }
	}

      if (rc)
	{
	  syslog (LOG_ERR, "Error initializing krb5: %s",
		  error_message (rc));
	  rshd_error ("Permission denied.\n");
	  exit (EXIT_FAILURE);
	}

# ifdef ENCRYPTION
      if (doencrypt)
	{
	  struct sockaddr_in local_addr;
	  rc = sizeof local_addr;
	  if (getsockname (STDIN_FILENO,
			   (struct sockaddr *) &local_addr, &rc) < 0)
	    {
	      syslog (LOG_ERR, "getsockname: %m");
	      rshd_error ("rshd: getsockname: %s", strerror (errno));
	      exit (EXIT_FAILURE);
	    }
	  authopts = KOPT_DO_MUTUAL;
	  rc = krb_recvauth (authopts, 0, ticket,
			     "rcmd", instance, &fromaddr,
			     &local_addr, kdata, "", schedule, version);
	  des_set_key (kdata->session, schedule);
	}
      else
# endif /* ENCRYPTION */
	rc = krb5_recvauth (context, &auth_ctx, &sockfd, "rcmd",
			    0, 0, keytab, &ticket);

      if (!rc)
	rc = krb5_auth_con_getauthenticator (context, auth_ctx, &author);

      if (!rc)
	{
	  rshd_error ("Kerberos authentication failure: %s\n",
		      error_message(rc));
	  exit (EXIT_FAILURE);
	}
    }
  else
#elif defined (SHISHI)	/* !KRB4 && !KRB5 */
  if (use_kerberos)
    {
      int rc;
      const char *err_msg;

      rc = get_auth (STDIN_FILENO, &h, &ap, &enckey, &err_msg, &protocol,
		     &cksumtype, &cksum, &cksumlen, servername);
      if (rc != SHISHI_OK)
	{
	  rshd_error ("Kerberos authentication failure: %s\n",
		      (err_msg && *err_msg) ? err_msg : shishi_strerror (rc));
	  exit (EXIT_FAILURE);
	}
    }
  else
#endif /* KERBEROS || SHISHI */
    remuser = getstr ("remuser");	/* The requesting user!  */

  /* Read three strings from the client. */
  locuser = getstr ("locuser");		/* The acting user!  */
  cmdbuf = getstr ("command");

#ifdef SHISHI
  if (use_kerberos)
    {
      int error;
      int rc;
      char *compcksum;
      size_t compcksumlen;
      char cksumdata[100];
      struct sockaddr_storage sock;
      socklen_t socklen;

      if (strlen (cmdbuf) >= 3)
	if (!strncmp (cmdbuf, "-x ", 3))
# ifdef ENCRYPTION
	  {
	    int i;

	    uses_encryption = 1;

	    ivtab[0] = &iv1;
	    ivtab[1] = &iv2;
	    ivtab[2] = &iv3;
	    ivtab[3] = &iv4;

	    keytype = shishi_key_type (enckey);
	    keylen = shishi_cipher_blocksize (keytype);

	    for (i = 0; i < 4; i++)
	      {
		ivtab[i]->ivlen = keylen;

		switch (keytype)
		  {
		  case SHISHI_DES_CBC_CRC:
		  case SHISHI_DES_CBC_MD4:
		  case SHISHI_DES_CBC_MD5:
		  case SHISHI_DES_CBC_NONE:
		  case SHISHI_DES3_CBC_HMAC_SHA1_KD:
		    ivtab[i]->keyusage = SHISHI_KEYUSAGE_KCMD_DES;
		    ivtab[i]->iv = xmalloc (ivtab[i]->ivlen);
		    memset (ivtab[i]->iv, 2 * i - 3 * (i >= 2),
			    ivtab[i]->ivlen);
		    ivtab[i]->ctx =
		      shishi_crypto (h, enckey, ivtab[i]->keyusage,
				     shishi_key_type (enckey),
				     ivtab[i]->iv, ivtab[i]->ivlen);
		    break;

		  case SHISHI_ARCFOUR_HMAC:
		  case SHISHI_ARCFOUR_HMAC_EXP:
		    ivtab[i]->keyusage =
		      SHISHI_KEYUSAGE_KCMD_DES + 4 * (i < 2) + 2 + 2 * (i % 2);
		    ivtab[i]->ctx =
		      shishi_crypto (h, enckey, ivtab[i]->keyusage,
				     shishi_key_type (enckey), NULL, 0);
		    break;

		  default:
		    ivtab[i]->keyusage =
		      SHISHI_KEYUSAGE_KCMD_DES + 4 * (i < 2) + 2 + 2 * (i % 2);
		    ivtab[i]->iv = xmalloc (ivtab[i]->ivlen);
		    memset (ivtab[i]->iv, 0, ivtab[i]->ivlen);
		    if (protocol == 2)
		      ivtab[i]->ctx =
			shishi_crypto (h, enckey, ivtab[i]->keyusage,
				       shishi_key_type (enckey),
				       ivtab[i]->iv, ivtab[i]->ivlen);
		  }
	      }
	  }
# else /* !ENCRYPTION */
	  {
	    shishi_ap_done (ap);
	    rshd_error ("Encrypted sessions are not supported.\n");
	    exit (EXIT_FAILURE);
	  }
# endif /* ENCRYPTION */

    remuser = getstr ("remuser");	/* The requesting user!  */

    rc = read (STDIN_FILENO, &error, sizeof (int)); /* XXX: not protocol */
    if ((rc != sizeof (int)) || error)
      exit (EXIT_FAILURE);

    /* verify checksum */
    {
      unsigned short pport;

      socklen = sizeof (sock);
      if (getsockname (STDIN_FILENO, (struct sockaddr *)&sock, &socklen) < 0)
	{
	  syslog (LOG_ERR, "Can't get sock name");
	  exit (EXIT_FAILURE);
	}

      pport = (sock.ss_family == AF_INET6)
	      ? ((struct sockaddr_in6 *) &sock)->sin6_port
	      : ((struct sockaddr_in *) &sock)->sin_port;

      snprintf (cksumdata, 100, "%u:%s%s", ntohs (pport), cmdbuf, locuser);
    }

    rc = shishi_checksum (h, enckey, 0, cksumtype,
			  cksumdata, strlen (cksumdata),
			  &compcksum, &compcksumlen);
    if (rc != SHISHI_OK
	|| compcksumlen != cksumlen
	|| memcmp (compcksum, cksum, cksumlen) != 0)
      {
	/* err_msg crash ? */
	/* *err_msg = "checksum verify failed"; */
	syslog (LOG_ERR, "checksum verify failed: %s", shishi_error (h));
	free (compcksum);
	shishi_ap_done (ap);
	rshd_error ("Authentication exchange failed.\n");
	exit (EXIT_FAILURE);
      }

    if (doencrypt && !uses_encryption)
      {
	syslog (LOG_INFO, "non-encrypted session denied from %s", hostname);
	free (compcksum);
	shishi_ap_done (ap);
	rshd_error ("Only encrypted sessions are allowed.\n");
	exit (EXIT_FAILURE);
      }
    else
      doencrypt = uses_encryption;

    rc = shishi_authorized_p (h, shishi_ap_tkt (ap), locuser);
    if (!rc)
      {
	syslog (LOG_AUTH | LOG_ERR,
		"User %s@%s is not authorized to run as: %s.",
		remuser, hostname, locuser);
	shishi_ap_done (ap);
	rshd_error ("Failed to get authorized as `%s'.\n", locuser);
	exit (EXIT_FAILURE);
      }

    free (compcksum);

    rc = shishi_encticketpart_clientrealm (h,
			shishi_tkt_encticketpart (shishi_ap_tkt (ap)),
			&rprincipal, NULL);
    if (rc != SHISHI_OK)
      rprincipal = NULL;

    shishi_ap_done (ap);

  }
#elif defined KRB5	/* !SHISHI */
  if (use_kerberos)
    {
      remuser = getstr ("remuser");	/* The requesting user!  */

      rc = krb5_copy_principal (context, ticket->enc_part2->client,
				&client);
      if (rc)
	goto fail;	/* FIXME: Temporary handler.  */

      if (client && !krb5_kuserok (context, client, locuser))
	goto fail;	/* FIXME: Temporary handler.  */

      rprincipal = NULL;
      krb5_unparse_name (context, client, &rprincipal);
    }
#endif /* KRB5 || SHISHI */

  /* Look up locuser in the passwd file.  The locuser has to be a
   * valid account on this system.
   */
  setpwent ();
#ifdef HAVE_GETPWNAM_R
  ret = getpwnam_r (locuser, &pwstor, pwbuf, pwbuflen, &pwd);
  if (ret || pwd == NULL)
#else /* !HAVE_GETPWNAM_R */
  pwd = getpwnam (locuser);
  if (pwd == NULL)
#endif /* HAVE_GETPWNAM_R */
    {
      syslog (LOG_INFO | LOG_AUTH, "%s@%s as %s: unknown login. cmd='%.80s'",
	      remuser, hostname, locuser, cmdbuf);
      if (errorstr == NULL)
	errorstr = "Login incorrect.\n";
      goto fail;
    }

#ifdef WITH_PAM
# if defined KERBEROS || defined SHISHI
  if (use_kerberos)
    service = "krsh";
  else
# endif
    service = "rsh";

  pam_rc = pam_start (service, locuser, &pam_conv, &pam_handle);
  if (pam_rc == PAM_SUCCESS)
    pam_rc = pam_set_item (pam_handle, PAM_RHOST, hostname);
  if (pam_rc == PAM_SUCCESS)
    pam_rc = pam_set_item (pam_handle, PAM_RUSER, remuser);
  if (pam_rc == PAM_SUCCESS)
    pam_rc = pam_set_item (pam_handle, PAM_TTY, service);
  if (pam_rc != PAM_SUCCESS)
    {
      errorstr = "Try again.\n";
      goto fail;
    }

  /* Checks existence of account, and more.
   */
  pam_rc = pam_authenticate (pam_handle, PAM_SILENT);
  if (pam_rc != PAM_SUCCESS)
    {
      switch (pam_rc)
	{
	case PAM_ABORT:
	  pam_end (pam_handle, pam_rc);
	  exit (EXIT_FAILURE);
	case PAM_NEW_AUTHTOK_REQD:
	  pam_rc = pam_chauthtok (pam_handle, PAM_CHANGE_EXPIRED_AUTHTOK);
	  if (pam_rc == PAM_SUCCESS)
	    {
	      pam_rc = pam_authenticate (pam_handle, PAM_SILENT);
	      if (pam_rc == PAM_SUCCESS)
		break;
	    }
	default:
	  errorstr = "Password incorrect.\n";
	  goto fail;
	}
    }

  /* Checks expiration of account, and more.
   */
  pam_rc = pam_acct_mgmt (pam_handle, PAM_SILENT);
  if (pam_rc != PAM_SUCCESS)
    {
      switch (pam_rc)
	{
	case PAM_NEW_AUTHTOK_REQD:
	  pam_rc = pam_chauthtok (pam_handle, PAM_CHANGE_EXPIRED_AUTHTOK);
	  if (pam_rc == PAM_SUCCESS)
	    {
	      pam_rc = pam_acct_mgmt (pam_handle, PAM_SILENT);
	      if (pam_rc == PAM_SUCCESS)
		break;
	    }
	case PAM_AUTH_ERR:
	  errorstr = "Password incorrect.\n";
	  goto fail;
	  break;
	case PAM_ACCT_EXPIRED:
	case PAM_PERM_DENIED:
	case PAM_USER_UNKNOWN:
	default:
	  errorstr = "Permission denied.\n";
	  goto fail;
	  break;
	}
    }

  /* Renew client information, since the PAM stack may have
   * mapped the request onto another identity.
   */
  free (locuser);
  locuser = NULL;
  pam_rc = pam_get_item (pam_handle, PAM_USER, (const void **) &locuser);
  if (pam_rc != PAM_SUCCESS)
    {
      syslog (LOG_NOTICE | LOG_AUTH, "pam_get_item(PAM_USER): %s",
	      pam_strerror (pam_handle, pam_rc));
      /* Intentionally let `locuser' be ill defined.  */
    }
# ifdef HAVE_GETPWNAM_R
  ret = getpwnam_r (locuser, &pwstor, pwbuf, pwbuflen, &pwd);
  if (ret || pwd == NULL)
# else /* !HAVE_GETPWNAM_R */
  pwd = getpwnam (locuser);
  if (pwd == NULL)
# endif /* HAVE_GETPWNAM_R */
    {
      syslog (LOG_INFO | LOG_AUTH, "%s@%s as %s: unknown login. cmd='%.80s'",
	      remuser, hostname, locuser, cmdbuf);
      errorstr = "Login incorrect.\n";
      goto fail;
    }
#else /* !WITH_PAM */
  /*
   * The account exists by a previous call to getpwnam().
   * Is the account locked, or has it expired?
   */
  {
    time_t now;

# ifdef HAVE_GETSPNAM
    struct spwd *spwd;

    /*
     * GNU/Linux, Solaris
     *
     * Locked account?
     */
    spwd = getspnam (pwd->pw_name);
    if (!spwd)
      {
	syslog (LOG_ERR | LOG_AUTH, "No access to encrypted password.");
	if (errorstr == NULL)
	  errorstr = "Login incorrect.\n";
	goto fail;
      }
    else
      {
	/* Locked accounts have their passwords prefixed with a blocker.  */
	if (!strncmp ("!", spwd->sp_pwdp, strlen ("!"))
	    || !strncmp ("*LK*", spwd->sp_pwdp, strlen ("*OK*")))
	  {
	    syslog (LOG_INFO | LOG_AUTH,
		    "%s@%s as %s: account is locked. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
	    if (errorstr == NULL)
	      errorstr = "Permission denied.\n";
	    goto fail;
	  }
      }

    /*
     * Expired account?
     */
    time (&now);
    if (spwd->sp_expire > 0)
      {
	time_t end_acct = DAY * spwd->sp_expire;

	if (difftime (now, end_acct) > 0)
	  {
	    syslog (LOG_INFO | LOG_AUTH,
		    "%s@%s as %s: account is expired. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
	    if (errorstr == NULL)
	      errorstr = "Permission denied.\n";
	    goto fail;
	  }
      }
# else /* !HAVE_GETSPNAM */
    /*
     * BSD systems.
     *
     * Locked account?
     */
    if (!strncmp ("*LOCKED*", pwd->pw_passwd, strlen ("*LOCKED*")))
      {
	syslog (LOG_INFO | LOG_AUTH,
		"%s@%s as %s: account is locked. cmd='%.80s'",
		remuser, hostname, locuser, cmdbuf);
	if (errorstr == NULL)
	  errorstr = "Permission denied.\n";
	goto fail;
      }

    /*
     * Expired account?
     */
#  ifdef HAVE_STRUCT_PASSWD_PW_EXPIRE
    time (&now);

    /*
     * Negative `pw_expire' indicates on NetBSD
     * an immediate need for change of password.
     */
    if (((pwd->pw_expire > 0) && (difftime (now, pwd->pw_expire) > 0))
	|| (pwd->pw_expire < 0))
      {
	syslog (LOG_INFO | LOG_AUTH,
		"%s@%s as %s: account is expired. cmd='%.80s'",
		remuser, hostname, locuser, cmdbuf);
	if (errorstr == NULL)
	  errorstr = "Permission denied.\n";
	goto fail;
      }
#  endif /* HAVE_STRUCT_PASSWD_PW_EXPIRE */
# endif /* !HAVE_GETSPNAM */
  }

#if defined WITH_IRUSEROK_AF
  switch (fromp->sa_family)
    {
    case AF_INET6:
      fromaddrp = (void *) &((struct sockaddr_in6 *) fromp)->sin6_addr;
      break;
    case AF_INET:
    default:
      fromaddrp = (void *) &((struct sockaddr_in *) fromp)->sin_addr;
    }
# endif /* !WITH_IRUSEROK_AF */
#endif /* !WITH_PAM */

#ifdef KRB4
  if (use_kerberos)
    {
      if (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0')
	{
	  if (kuserok (kdata, locuser) != 0)
	    {
	      syslog (LOG_INFO | LOG_AUTH, "Kerberos rsh denied to %s.%s@%s",
		      kdata->pname, kdata->pinst, kdata->prealm);
	      rshd_error ("Permission denied.\n");
	      exit (EXIT_FAILURE);
	    }
	}
    }
  else
#elif defined KRB5	/* !KRB4 */
  if (use_kerberos)
    {
      if (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0' && client)
	{
	  if (krb5_kuserok (context, client, locuser) != 0)
	    {
	      syslog (LOG_INFO | LOG_AUTH, "Kerberos rsh denied to %s.%s@%s",
		      "kdata->pname", "kdata->pinst", "kdata->prealm");
	      rshd_error ("Permission denied.\n");
	      exit (EXIT_FAILURE);
	    }
	}
    }
  else
#elif defined(SHISHI) /* !KERBEROS */
  if (use_kerberos)
    {				/*
				   if (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0')
				   {
				   if (kuserok (kdata, locuser) != 0)
				   {
				   syslog (LOG_INFO|LOG_AUTH, "Kerberos rsh denied to %s.%s@%s",
				   kdata->pname, kdata->pinst, kdata->prealm);
				   rshd_error ("Permission denied.\n");
				   exit (EXIT_FAILURE);
				   }
				   } */
    }
  else
#endif /* KERBEROS || SHISHI */

#ifndef WITH_PAM
# ifdef WITH_IRUSEROK_SA
    if (errorstr || (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0'
                     && (iruserok_sa ((void *) fromp, fromlen,
				      pwd->pw_uid == 0, remuser, locuser)) < 0))
# elif defined WITH_IRUSEROK_AF
    if (errorstr || (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0'
                     && (iruserok_af (fromaddrp, pwd->pw_uid == 0,
				      remuser, locuser, fromp->sa_family)) < 0))
# elif defined WITH_IRUSEROK
    if (errorstr || (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0'
                     && (iruserok (((struct sockaddr_in *) fromp)->sin_addr.s_addr,
				   pwd->pw_uid == 0, remuser, locuser)) < 0))
# elif defined WITH_RUSEROK_AF
    if (errorstr || (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0'
                     && (ruserok_af (addrstr, pwd->pw_uid == 0,
				  remuser, locuser, fromp->sa_family)) < 0))
# elif defined WITH_RUSEROK
    if (errorstr || (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0'
                     && (ruserok (addrstr, pwd->pw_uid == 0,
				  remuser, locuser)) < 0))
# else /* !WITH_IRUSEROK* && !WITH_RUSEROK* */
# error Unable to use mandatory iruserok/ruserok.  This should not happen.
# endif /* !WITH_IRUSEROK* && !WITH_RUSEROK* */
#else /* WITH_PAM */
    if (0)	/* Wrapper for `fail' jump label.  */
#endif /* !WITH_PAM */
    {
#ifdef HAVE___RCMD_ERRSTR
      if (__rcmd_errstr)
	syslog (LOG_INFO | LOG_AUTH,
		"%s@%s as %s: permission denied (%s). cmd='%.80s'",
		remuser, hostname, locuser, __rcmd_errstr, cmdbuf);
      else
#endif /* HAVE___RCMD_ERRSTR */
	syslog (LOG_INFO | LOG_AUTH,
		"%s@%s as %s: permission denied. cmd='%.80s'",
		remuser, hostname, locuser, cmdbuf);
    fail:
#ifdef WITH_PAM
      if (pam_handle)
	{
	  if (pam_rc != PAM_SUCCESS)
	    syslog (LOG_NOTICE | LOG_AUTH, "%s@%s as %s, PAM: %s",
		    remuser, hostname, locuser,
		    pam_strerror (pam_handle, pam_rc));
	  pam_end (pam_handle, pam_rc);
	}
#endif /* WITH_PAM */
      if (errorstr == NULL)
	errorstr = "Permission denied.\n";
      rshd_error (errorstr, errorhost);
      exit (EXIT_FAILURE);
    }

  /* If the locuser isn't root, then check if logins are disabled. */
  if (pwd->pw_uid && !access (PATH_NOLOGIN, F_OK))
    {
      rshd_error ("Logins currently disabled.\n");
      exit (EXIT_FAILURE);
    }

  /* Now write the null byte back to the client, telling it
   * that everything is OK.
   *
   * Note that this means that any error message that we generate
   * from now on (such as the perror() if the execl() fails), won't
   * be seen by the rcmd() function, but it will be seen by the
   * application that called rcmd() once it reads from the socket.
   */
  if (write (STDERR_FILENO, "\0", 1) < 0)
    {
      rshd_error ("Lost connection.\n");
      exit (EXIT_FAILURE);
    }
  sent_null = 1;

  if (port)
    {
      /* We need a secondary channel.  Here is where we create
       * the control process that will handle this secondary
       * channel.
       * First create a pipe to use for communication between
       * the parent and child, then fork.
       */
      if (pipe (pv) < 0)
	{
	  rshd_error ("Can't make pipe.\n");
	  exit (EXIT_FAILURE);
	}
#ifdef ENCRYPTION
# if defined KERBEROS || defined SHISHI
      if (doencrypt)
	{
	  if (pipe (pv1) < 0)
	    {
	      rshd_error ("Can't make 2nd pipe.\n");
	      exit (EXIT_FAILURE);
	    }
	  if (pipe (pv2) < 0)
	    {
	      rshd_error ("Can't make 3rd pipe.\n");
	      exit (EXIT_FAILURE);
	    }
	}
# endif /* KERBEROS || SHISHI */
#endif /* ENCRYPTION */
      pid = fork ();
      if (pid == -1)
	{
	  rshd_error ("Can't fork; try again.\n");
	  exit (EXIT_FAILURE);
	}
      if (pid)
	{
	  /* Parent process == control process.
	   * We: (1) read from the pipe and write to s;
	   *     (2) read from s and send corresponding
	   *         signal.
	   */
#ifdef ENCRYPTION
# if defined KERBEROS
	  if (doencrypt)
	    {
	      static char msg[] = SECURE_MESSAGE;
	      close (pv1[1]);
	      close (pv2[1]);
	      des_write (s, msg, sizeof (msg) - 1);
	    }
	  else
# elif defined(SHISHI) /* !KERBEROS */
	  if (doencrypt)
	    {
	      close (pv1[1]);
	      close (pv2[1]);
	    }
	  else
# endif /* KERBEROS || SHISHI */
#endif /* ENCRYPTION */
	    {
	      /* child handles the original socket */
	      close (STDIN_FILENO);
	      /* (0, 1, and 2 were from inetd */
	      close (STDOUT_FILENO);
	    }
	  close (STDERR_FILENO);
	  close (pv[1]);	/* close write end of pipe */

	  FD_ZERO (&readfrom);
	  FD_SET (s, &readfrom);
	  FD_SET (pv[0], &readfrom);
	  /* set max fd + 1 for select */
	  if (pv[0] > s)
	    nfd = pv[0];
	  else
	    nfd = s;
#ifdef ENCRYPTION
# if defined KERBEROS || defined SHISHI
	  if (doencrypt)
	    {
	      FD_ZERO (&writeto);
	      FD_SET (pv2[0], &writeto);
	      FD_SET (pv1[0], &readfrom);

	      nfd = MAX (nfd, pv2[0]);
	      nfd = MAX (nfd, pv1[0]);
	    }
	  else
# endif /* KERBEROS || SHISHI */
#endif /* ENCRYPTION */
	    ioctl (pv[0], FIONBIO, (char *) &one);
	  /* should set s nbio! */
	  nfd++;
	  do
	    {
	      ready = readfrom;
#ifdef ENCRYPTION
# if defined KERBEROS || defined SHISHI
	      if (doencrypt)
		{
#  ifdef SHISHI
		  wready = readfrom;
#  else /* KERBEROS && !SHISHI */
		  wready = writeto;
#  endif
		  if (select (nfd, &ready, &wready, (fd_set *) 0,
			      (struct timeval *) 0) < 0)
		    break;
		}
	      else
# endif /* KERBEROS || SHISHI */
#endif /* ENCRYPTION */
	      if (select (nfd, &ready, (fd_set *) 0,
			    (fd_set *) 0, (struct timeval *) 0) < 0)
		/* wait until there is something to read */
		break;
	      if (FD_ISSET (s, &ready))
		{
		  int ret;
#ifdef ENCRYPTION
# ifdef KERBEROS
		  if (doencrypt)
		    ret = des_read (s, &sig, 1);
		  else
# elif defined(SHISHI) /* !KERBEROS */
		  if (doencrypt)
		    readenc (h, s, &sig, &ret, &iv2, enckey, protocol);
		  else
# endif /* KERBEROS || SHISHI */
#endif /* ENCRYPTION */
		    ret = read (s, &sig, 1);
		  if (ret <= 0)
		    FD_CLR (s, &readfrom);
		  else
		    killpg (pid, sig);
		}
	      if (FD_ISSET (pv[0], &ready))
		{
		  errno = 0;
		  cc = read (pv[0], buf, sizeof buf);
		  if (cc <= 0)
		    {
		      shutdown (s, SHUT_RDWR);
		      FD_CLR (pv[0], &readfrom);
		    }
		  else
		    {
#ifdef ENCRYPTION
# ifdef KERBEROS
		      if (doencrypt)
			des_write (s, buf, cc);
		      else
# elif defined(SHISHI) /* !KERBEROS */
		      if (doencrypt)
			writeenc (h, s, buf, cc, &n, &iv4, enckey, protocol);
		      else
# endif /* KERBEROS || SHISHI */
#endif /* ENCRYPTION */
			write (s, buf, cc);
		    }
		}
#ifdef ENCRYPTION
# if defined KERBEROS || defined SHISHI
	      if (doencrypt && FD_ISSET (pv1[0], &ready))
		{
		  errno = 0;
		  cc = read (pv1[0], buf, sizeof (buf));
		  if (cc <= 0)
		    {
		      shutdown (pv1[0], SHUT_RDWR);
		      FD_CLR (pv1[0], &readfrom);
		    }
		  else
#  ifdef SHISHI
		    writeenc (h, STDOUT_FILENO, buf, cc, &n, &iv3, enckey,
			      protocol);
#  else /* KERBEROS */
		    des_write (STDOUT_FILENO, buf, cc);
#  endif
		}

	      if (doencrypt && FD_ISSET (pv2[0], &wready))
		{
		  errno = 0;
#  ifdef SHISHI
		  readenc (h, STDIN_FILENO, buf, &cc, &iv1, enckey, protocol);
#  else /* KERBEROS */
		  cc = des_read (STDIN_FILENO, buf, sizeof buf);
#  endif
		  if (cc <= 0)
		    {
		      shutdown (pv2[0], SHUT_RDWR);
		      FD_CLR (pv2[0], &writeto);
		    }
		  else
		    write (pv2[0], buf, cc);
		}
# endif /* KERBEROS || SHISHI */
#endif /* ENCRYPTION */
	    }
	  while (FD_ISSET (s, &readfrom) ||
#ifdef ENCRYPTION
# if defined KERBEROS || defined SHISHI
		 (doencrypt && FD_ISSET (pv1[0], &readfrom)) ||
# endif
#endif /* ENCRYPTION */
		 FD_ISSET (pv[0], &readfrom));
	  /* The pipe will generate an EOF when the shell
	   * terminates.  The socket will terminate when the
	   * client process terminates.
	   */
#ifdef WITH_PAM
	  /* The child opened the session; now it
	   * should be closed down properly.  */
	  pam_rc = pam_close_session (pam_handle, PAM_SILENT);
	  if (pam_rc != PAM_SUCCESS)
	    syslog (LOG_WARNING | LOG_AUTH, "pam_close_session: %s",
		    pam_strerror (pam_handle, pam_rc));
	  pam_rc = pam_setcred (pam_handle, PAM_SILENT | PAM_DELETE_CRED);
	  if (pam_rc != PAM_SUCCESS)
	    syslog (LOG_WARNING | LOG_AUTH, "pam_setcred: %s",
		    pam_strerror (pam_handle, pam_rc));
	  pam_end (pam_handle, pam_rc);
#endif /* WITH_PAM */

	  exit (EXIT_SUCCESS);
	} /* Parent process ends.  */

      close (s);		/* control process handles this fd */
      close (pv[0]);		/* close read end of pipe */
#ifdef ENCRYPTION
# if defined KERBEROS || defined SHISHI
      if (doencrypt)
	{
	  close (pv1[0]);
	  close (pv2[0]);
	  dup2 (pv1[1], STDOUT_FILENO);
	  dup2 (pv2[1], STDIN_FILENO);
	  close (pv1[1]);
	  close (pv2[1]);
	}
# endif /* KERBEROS || SHISHI */
#endif /* ENCRYPTION */

#if defined SHISHI
      if (use_kerberos)
	{
	  int i;

	  shishi_done (h);
# ifdef ENCRYPTION
	  if (doencrypt)
	    {
	      shishi_key_done (enckey);
	      for (i = 0; i < 4; i++)
		{
		  shishi_crypto_close (ivtab[i]->ctx);
		  free (ivtab[i]->iv);
		}
	    }
# endif /* ENCRYPTION */
	}

#endif /* SHISHI */

      dup2 (pv[1], STDERR_FILENO);	/* stderr of shell has to go
					   pipe to control process */
      close (pv[1]);
    }
#ifdef WITH_PAM
    /* Session handling must end also in this case.  */
  else
    {
      pid = fork ();
      if (pid < 0)
	{
	  rshd_error ("Can't fork; try again.\n");
	  exit (EXIT_FAILURE);
	}
      if (pid)
	{
	  /* Parent: Wait for child and tear down
	   * the PAM session.  */
	  int status;

	  while (wait (&status) < 0 && errno == EINTR)
	    ;

	  pam_rc = pam_close_session (pam_handle, PAM_SILENT);
	  if (pam_rc != PAM_SUCCESS)
	    syslog (LOG_WARNING | LOG_AUTH, "pam_close_session: %s",
		    pam_strerror (pam_handle, pam_rc));
	  pam_rc = pam_setcred (pam_handle, PAM_SILENT | PAM_DELETE_CRED);
	  if (pam_rc != PAM_SUCCESS)
	    syslog (LOG_WARNING | LOG_AUTH, "pam_setcred: %s",
		    pam_strerror (pam_handle, pam_rc));
	  pam_end (pam_handle, pam_rc);

	  exit (WIFEXITED (status) ? WEXITSTATUS (status) : EXIT_FAILURE);
	} /* Parent process ends.  */
    }
#endif /* WITH_PAM */

  /* Child process, with and without handler for stderr.
   * Become a process group leader, so that the control
   * process above can send signals to all the processes
   * we may be the parent of.  The process group ID
   * (the getpid() value below) equals the childpid value
   * from the fork above.
   */
#ifdef HAVE_SETLOGIN
  /* Not sufficient to call setpgid() on BSD systems.  */
  if (setsid () < 0)
    syslog (LOG_ERR, "setsid() failed: %m");

  if (setlogin (pwd->pw_name) < 0)
    syslog (LOG_ERR, "setlogin() failed: %m");
#else /* !HAVE_SETLOGIN */
  setpgid (0, getpid ());
#endif

  if (*pwd->pw_shell == '\0')
    pwd->pw_shell = PATH_BSHELL;

  /* Set the gid, then uid to become the user specified by "locuser" */
  setegid ((gid_t) pwd->pw_gid);
  setgid ((gid_t) pwd->pw_gid);
#ifdef HAVE_INITGROUPS
  initgroups (pwd->pw_name, pwd->pw_gid);	/* BSD groups */
#endif

#ifdef WITH_PAM
  pam_rc = pam_setcred (pam_handle, PAM_SILENT | PAM_ESTABLISH_CRED);
  if (pam_rc != PAM_SUCCESS)
    {
      syslog (LOG_ERR | LOG_AUTH, "pam_setcred: %s",
	      pam_strerror (pam_handle, pam_rc));
      pam_rc = PAM_SUCCESS;	/* Only report the above anomaly.  */
    }
  pam_rc = pam_open_session (pam_handle, PAM_SILENT);
  if (pam_rc != PAM_SUCCESS)
    {
      syslog (LOG_ERR | LOG_AUTH, "pam_open_session: %s",
	      pam_strerror (pam_handle, pam_rc));
      pam_rc = PAM_SUCCESS;	/* Only report the above anomaly.  */
    }
#endif /* WITH_PAM */

  setuid ((uid_t) pwd->pw_uid);

  /* We'll execute the client's command in the home directory
   * of locuser. Note, that the chdir must be executed after
   * setuid(), otherwise it may fail on NFS mounted directories
   * (root mapped to nobody).
   */
  if (chdir (pwd->pw_dir) < 0)
    {
      syslog (LOG_INFO | LOG_AUTH,
	      "%s@%s as %s: no home directory. cmd='%.80s'", remuser,
	      hostname, locuser, cmdbuf);
      rshd_error ("No remote directory.\n");

      if (chdir ("/") < 0)
	{
	  syslog (LOG_ERR | LOG_AUTH,
		  "%s@%s as %s: access denied to '/'",
		  remuser, hostname, locuser);
	  exit (EXIT_FAILURE);
	}
    }

  /* Set up an initial environment for the shell that we exec() */
  strncat (homedir, pwd->pw_dir, sizeof (homedir) - sizeof ("HOME=") - 1);
  strncat (path, PATH_DEFPATH, sizeof (path) - sizeof ("PATH=") - 1);
  strncat (shell, pwd->pw_shell, sizeof (shell) - sizeof ("SHELL=") - 1);
  strncat (username, pwd->pw_name, sizeof (username) - sizeof ("USER=") - 1);
  strncat (logname, pwd->pw_name, sizeof (logname) - sizeof ("LOGNAME=") - 1);
  strncat (rhost, hostname, sizeof (rhost) - sizeof ("RHOST=") - 1);

#ifdef WITH_PAM
  if (pam_getenv (pam_handle, "PATH") == NULL)
    (void) pam_putenv (pam_handle, path);
  if (pam_getenv (pam_handle, "HOME") == NULL)
    (void) pam_putenv (pam_handle, homedir);
  if (pam_getenv (pam_handle, "SHELL") == NULL)
    (void) pam_putenv (pam_handle, shell);
  if (pam_getenv (pam_handle, "USER") == NULL)
    (void) pam_putenv (pam_handle, username);
  if (pam_getenv (pam_handle, "LOGNAME") == NULL)
    (void) pam_putenv (pam_handle, logname);
  if (pam_getenv (pam_handle, "RHOST") == NULL)
    (void) pam_putenv (pam_handle, rhost);

  environ = pam_getenvlist (pam_handle);
#else /* !WITH_PAM */
  environ = envinit;
#endif /* WITH_PAM */

  cp = strrchr (pwd->pw_shell, '/');
  if (cp)
    cp++;			/* step past last slash */
  else
    cp = pwd->pw_shell;		/* no slash in shell string */
  endpwent ();
  if (log_success || pwd->pw_uid == 0)
    {
#ifdef KRB4
      if (use_kerberos)
	syslog (LOG_INFO | LOG_AUTH,
		"Kerberos shell from %s.%s@%s on %s as %s, cmd='%.80s'",
		kdata->pname, kdata->pinst, kdata->prealm,
		hostname, locuser, cmdbuf);
      else
#endif /* KRB4 */
	syslog (LOG_INFO | LOG_AUTH,
		"%s%s from %s as '%s': cmd='%.80s'",
#ifdef SHISHI
		!use_kerberos ? ""
		  : !doencrypt ? "Kerberized "
		    : "Kerberized and encrypted ",
#else
		"",
#endif
		rprincipal ? rprincipal : remuser,
		hostname, locuser, cmdbuf);
    }
#ifdef SHISHI
  if (doencrypt)
    execl (pwd->pw_shell, cp, "-c", cmdbuf + 3, NULL);
  else
#endif /* SHISHI */
    execl (pwd->pw_shell, cp, "-c", cmdbuf, NULL);

#ifdef WITH_PAM
  pam_end (pam_handle, PAM_SUCCESS);
#endif
  syslog (LOG_ERR, "execl failed for \"%s\": %m", pwd->pw_name);
  error (EXIT_FAILURE, errno, "cannot execute %s", pwd->pw_shell);
}

/*
 * Report error to client.  Note: can't be used until second socket has
 * connected to client, or older clients will hang waiting for that
 * connection first.
 */

void
rshd_error (const char *fmt, ...)
{
  va_list ap;
  int len;
  char *bp, buf[BUFSIZ];
  va_start (ap, fmt);

  bp = buf;
  if (sent_null == 0)
    {
      *bp++ = 1;	/* error indicator */
      len = 1;
    }
  else
    len = 0;
  vsnprintf (bp, sizeof (buf) - 1, fmt, ap);
  va_end (ap);
  write (STDERR_FILENO, buf, len + strlen (bp));
}

char *
getstr (const char *err)
{
  size_t buf_len = 100;
  char *buf = malloc (buf_len), *end = buf;

  if (!buf)
    {
      rshd_error ("Out of space reading %s\n", err);
      exit (EXIT_FAILURE);
    }

  do
    {
      /* Oh this is efficient, oh yes.  [But what can be done?] */
      int rd = read (STDIN_FILENO, end, 1);

      if (rd <= 0)
	{
	  if (rd == 0)
	    rshd_error ("EOF reading %s\n", err);
	  else
	    perror (err);
	  exit (EXIT_FAILURE);
	}

      end += rd;
      if ((buf + buf_len - end) < (ssize_t) (buf_len >> 3))
	{
	  /* Not very much room left in our buffer, grow it. */
	  size_t end_offs = end - buf;
	  buf_len += buf_len;
	  buf = realloc (buf, buf_len);
	  if (!buf)
	    {
	      rshd_error ("Out of space reading %s\n", err);
	      exit (EXIT_FAILURE);
	    }
	  end = buf + end_offs;
	}
    }
  while (*(end - 1));

  return buf;
}

/*
 * Check whether host h is in our local domain,
 * defined as sharing the last two components of the domain part,
 * or the entire domain part if the local domain has only one component.
 * If either name is unqualified (contains no '.'),
 * assume that the host is local, as it will be
 * interpreted as such.
 */
int
local_domain (const char *h)
{
  char *hostname = localhost ();

  if (!hostname)
    return 0;
  else
    {
      int is_local = 0;
      const char *p1 = topdomain (hostname);
      const char *p2 = topdomain (h);

      if (p1 == NULL || p2 == NULL || !strcasecmp (p1, p2))
	is_local = 1;

      free (hostname);

      return is_local;
    }
}

const char *
topdomain (const char *h)
{
  const char *p, *maybe = NULL;
  int dots = 0;

  for (p = h + strlen (h); p >= h; p--)
    {
      if (*p == '.')
	{
	  if (++dots == 2)
	    return p;
	  maybe = p;
	}
    }
  return maybe;
}

#ifdef WITH_PAM
/* Call back function for passing user's information
 * to any PAM module requesting this information.
 */
static int
rsh_conv (int num, const struct pam_message **pam_msg,
	    struct pam_response **pam_resp,
	    void *data _GL_UNUSED_PARAMETER)
{
  struct pam_response *resp;

  /* Reject composite calls at the time being.  */
  if (num <= 0 || num > 1)
    return PAM_CONV_ERR;

  /* Ensure an empty response.  */
  *pam_resp = NULL;

  switch ((*pam_msg)->msg_style)
    {
    case PAM_PROMPT_ECHO_OFF:	/* Return an empty password.  */
      resp = (struct pam_response *) malloc (sizeof (*resp));
      if (!resp)
	return PAM_BUF_ERR;
      resp->resp_retcode = 0;
      resp->resp = strdup ("");
      if (!resp->resp)
	{
	  free (resp);
	  return PAM_BUF_ERR;
	}
      if (log_success)
	syslog (LOG_NOTICE | LOG_AUTH, "PAM message \"%s\".",
		(*pam_msg)->msg);
      *pam_resp = resp;
      return PAM_SUCCESS;
      break;
    case PAM_TEXT_INFO:		/* Not yet supported.  */
    case PAM_ERROR_MSG:		/* Likewise.  */
    case PAM_PROMPT_ECHO_ON:	/* Interactivity is not supported.  */
    default:
      return PAM_CONV_ERR;
    }
  return PAM_CONV_ERR;	/* Never reached.  */
}
#endif /* WITH_PAM */
