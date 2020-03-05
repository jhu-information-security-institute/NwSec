/*
  Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
  2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014,
  2015 Free Software Foundation, Inc.

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
 * Copyright (c) 1983, 1990, 1993
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
 * rlogin - remote login
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
#include <sys/wait.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_STREAM_H
# include <sys/stream.h>
#endif
#ifdef HAVE_SYS_TTY_H
# include <sys/tty.h>
#endif
#ifdef HAVE_SYS_PTYVAR_H
# include <sys/ptyvar.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif

#include <netinet/in.h>
#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif

#include <argp.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <termios.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdarg.h>

#include <progname.h>
#include "libinetutils.h"
#include "unused-parameter.h"
#include "xalloc.h"

#ifdef KERBEROS
# ifdef HAVE_KERBEROSIV_DES_H
#  include <kerberosIV/des.h>
# endif
# ifdef HAVE_KERBEROSIV_KRB_H
#  include <kerberosIV/krb.h>
# endif
#endif /* KERBEROS */

#ifdef SHISHI
# include <shishi.h>
# include "shishi_def.h"
# define REALM_SZ 1040
#endif

#if defined KERBEROS || defined SHISHI
int use_kerberos = 1, doencrypt;
# ifdef KERBEROS
char dest_realm_buf[REALM_SZ];
const char *dest_realm = NULL;
CREDENTIALS cred;
Key_schedule schedule;

# elif defined(SHISHI)
char dest_realm_buf[REALM_SZ];
const char *dest_realm = NULL;

Shishi *handle;
Shishi_key *key;
shishi_ivector iv1, iv2;
shishi_ivector *ivtab[2];

int keytype;
int keylen;
int rc;
int wlen;
# endif /* SHISHI */
#endif /* KERBEROS || SHISHI */

/*
  The TIOCPKT_* macros may not be implemented in the pty driver.
  Defining them here allows the program to be compiled.  */
#ifndef TIOCPKT
# define TIOCPKT                 _IOW('t', 112, int)
# define TIOCPKT_FLUSHWRITE      0x02
# define TIOCPKT_NOSTOP          0x10
# define TIOCPKT_DOSTOP          0x20
#endif /*TIOCPKT*/
/* The server sends us a TIOCPKT_WINDOW notification when it starts up.
   The value for this (0x80) cannot overlap the kernel defined TIOCPKT_xxx
   values.  */
#ifndef TIOCPKT_WINDOW
# define TIOCPKT_WINDOW	0x80
#endif
/* Concession to Sun.  */
#ifndef SIGUSR1
# define SIGUSR1	30
#endif
#ifndef _POSIX_VDISABLE
# ifdef VDISABLE
#  define _POSIX_VDISABLE VDISABLE
# else
#  define _POSIX_VDISABLE ((cc_t)'\377')
# endif
#endif
/* Returned by speed() when the specified fd is not associated with a
   terminal.  */
#define SPEED_NOTATTY	(-1)
int eight = 0, rem;

int dflag = 0;
int noescape;
char * host = NULL;
char * user = NULL;
unsigned char escapechar = '~';
#if defined WITH_ORCMD_AF || defined WITH_RCMD_AF || defined SHISHI
sa_family_t family = AF_UNSPEC;
#endif

#ifdef OLDSUN

struct winsize
{
  unsigned short ws_row;	/* Rows, in characters.  */
  unsigned short ws_col;	/* Columns , in characters.  */
  unsigned short ws_xpixel;	/* Horizontal size, pixels.  */
  unsigned short ws_ypixel;	/* Vertical size. pixels.  */
};

int get_window_size (int, struct winsize *);
#else /* !OLDSUN */
# define get_window_size(fd, wp)	ioctl (fd, TIOCGWINSZ, wp)
#endif
struct winsize winsize;

void catch_child (int);
void copytochild (int);
void doit (sigset_t *);
void done (int);
void echo (char);
u_int getescape (char *);
void lostpeer (int);
void mode (int);
void oob (int);
int reader (sigset_t *);
void sendwindow (void);
void setsignal (int);
int speed (int);
unsigned int speed_translate (unsigned int);
void sigwinch (int);
void stop (char);
void writer (void);
void writeroob (int);

#if defined KERBEROS || defined SHISHI
void warning (const char *, ...);
#endif

const char args_doc[] = "HOST";
const char doc[] = "Starts a terminal session on a remote host.";

static struct argp_option argp_options[] = {
#define GRP 0
  {"8-bit", '8', NULL, 0, "allows an eight-bit input data path at all times",
   GRP+1},
  {"debug", 'd', NULL, 0, "set the SO_DEBUG option", GRP+1},
  {"escape", 'e', "CHAR", 0, "allows user specification of the escape "
   "character, which is ``~'' by default", GRP+1},
  {"no-escape", 'E', NULL, 0, "stops any character from being recognized as "
   "an escape character", GRP+1},
  {"user", 'l', "USER", 0, "run as USER on the remote system", GRP+1},
#if defined WITH_ORCMD_AF || defined WITH_RCMD_AF || defined SHISHI
  { "ipv4", '4', NULL, 0, "use only IPv4", GRP+1 },
  { "ipv6", '6', NULL, 0, "use only IPv6", GRP+1 },
#endif
#undef GRP
#if defined KERBEROS || defined SHISHI
# define GRP 10
# ifdef ENCRYPTION
  {"encrypt", 'x', NULL, 0, "turns on encryption for all data passed via "
   "the rlogin session", GRP+1},
# endif
  {"kerberos", 'K', NULL, 0, "turns off all Kerberos authentication", GRP+1},
  {"realm", 'k', "REALM", 0, "obtain tickets for the remote host in REALM "
   "realm instead of the remote's realm", GRP+1},
# undef GRP
#endif /* KERBEROS || SHISHI */
  {NULL, 0, NULL, 0, NULL, 0}
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
#if defined WITH_ORCMD_AF || defined WITH_RCMD_AF || defined SHISHI
    case '4':
      family = AF_INET;
      break;
    case '6':
      family = AF_INET6;
      break;
#endif
    /* 8-bit input Specifying this forces us to use RAW mode input from
       the user's terminal.  Also, in this mode we won't perform any
       local flow control.  */
    case '8':
      eight = 1;
      break;

    case 'd':
      dflag = 1;
      break;

    case 'e':
      noescape = 0;
      escapechar = getescape (arg);
      if (escapechar == 0)
	error (EXIT_FAILURE, 0, "illegal option value -- e");
      break;

    case 'E':
      noescape = 1;
      break;

    case 'l':
      user = arg;
      break;

#if defined KERBEROS || defined SHISHI
# ifdef ENCRYPTION
    case 'x':
      doencrypt = 1;
#  ifdef KERBEROS
      des_set_key (cred.session, schedule);
#  endif
      break;
# endif

    case 'K':
      use_kerberos = 0;
      break;

    case 'k':
      strncpy (dest_realm_buf, arg, sizeof (dest_realm_buf));
      /* Make sure it's null termintated.  */
      dest_realm_buf[sizeof (dest_realm_buf) - 1] = '\0';
      dest_realm = dest_realm_buf;
      break;
#endif

    case ARGP_KEY_NO_ARGS:
      if (host == NULL)
        argp_error (state, "missing host operand");

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp =
  {argp_options, parse_opt, args_doc, doc, NULL, NULL, NULL};

int
main (int argc, char *argv[])
{
  struct passwd *pw;
  struct servent *sp;
  struct sigaction sa;
  sigset_t smask, osmask;
  uid_t uid;
  int index;
  int term_speed;
  char term[1024];

  set_program_name (argv[0]);

  /* Traditionally, if a symbolic link was made to the rlogin binary
         hostname --> rlogin
     then hostname will be used as the name of the server to access.  */
  {
    char *p = strrchr (argv[0], '/');
    if (p)
      ++p;
    else
      p = argv[0];

    if (strcmp (p, "rlogin") != 0)
      host = p;
  }

  /* Parse command line */
  iu_argp_init ("rlogin", default_program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  if (index < argc)
    host = argv[index++];

  argc -= index;

  /* Get the name of the user invoking us: the client-user-name.  */
  uid = getuid ();
  pw = getpwuid (uid);
  if (!pw)
    error (EXIT_FAILURE, 0, "unknown user id.");

  /* Accept user1@host format, though "-l user2" overrides user1 */
  {
    char *p = strchr (host, '@');
    if (p)
      {
	*p = '\0';
	if (!user && p > host)
	  user = host;
	host = p + 1;
	if (*host == '\0')
          error (EXIT_FAILURE, 0, "invalid host operand");
      }
  }

  sp = NULL;
#if defined KERBEROS || defined SHISHI
  if (use_kerberos)
    {
      sp = getservbyname ((doencrypt ? "eklogin" : "klogin"), "tcp");
      if (sp == NULL)
	{
	  use_kerberos = 0;
	  warning ("can't get entry for %s/tcp service",
		   doencrypt ? "eklogin" : "klogin");
	}
    }
#endif

  /* Get the port number for the rlogin service.  */
  if (sp == NULL)
    sp = getservbyname ("login", "tcp");
  if (sp == NULL)
    error (EXIT_FAILURE, 0, "login/tcp: unknown service.");

  /* Get the name of the terminal from the environment.  Also get the
     terminal's speed.  The name and the speed are passed to the server
     as the argument "cmd" of the rcmd() function.  This is something like
     "vt100/9600".  */
  term_speed = speed (STDIN_FILENO);
  if (term_speed == SPEED_NOTATTY)
    {
      char *p;
      snprintf (term, sizeof term, "%s",
		((p = getenv ("TERM")) ? p : "network"));
    }
  else
    {
      char *p;
      snprintf (term, sizeof term, "%s/%d",
		((p = getenv ("TERM")) ? p : "network"), term_speed);
    }
  get_window_size (STDIN_FILENO, &winsize);

  setsig (SIGPIPE, lostpeer);	/* XXX: Replace setsig()?  */

  /*
   * Block SIGURG and SIGUSR1 signals during connection setup.
   * These signals will be handled distinctly by parent and child
   * after completion of process forking.
   *
   * SIGUSR1 will be be raise by the child for the parent process,
   * in order that the client's finding of window size be transmitted
   * to the remote machine.
   *
   * osmask will be passed along as the desired runtime signal mask.
   */
  sigemptyset (&smask);
  sigemptyset (&osmask);
  sigaddset (&smask, SIGURG);
  sigaddset (&smask, SIGUSR1);
  sigprocmask (SIG_SETMASK, &smask, &osmask);

  /*
   * We set disposition for SIGURG and SIGUSR1 so that an
   * incoming signal will be held pending rather than being
   * discarded. Note that these routines will be ready to get
   * a signal by the time that they are unblocked later on.
   */
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = copytochild;
  (void) sigaction (SIGURG, &sa, NULL);
  sa.sa_handler = writeroob;
  (void) sigaction (SIGUSR1, &sa, NULL);

#if defined KERBEROS || defined SHISHI
try_connect:
  if (use_kerberos)
    {
      int krb_errno = 0;
# if defined KERBEROS
      struct hostent *hp;

      /* Fully qualified hostname (needed for krb_realmofhost).  */
      hp = gethostbyname (host);
      if (hp != NULL && !(host = strdup (hp->h_name)))
	error (EXIT_FAILURE, errno, "strdup");

      rem = KSUCCESS;
      errno = 0;
      if (dest_realm == NULL)
	dest_realm = krb_realmofhost (host);
# elif defined (SHISHI)
      rem = SHISHI_OK;
      errno = 0;
# endif

# ifdef ENCRYPTION
      if (doencrypt)
#  if defined SHISHI
	{
	  int i;

	  rem = krcmd_mutual (&handle, &host, sp->s_port, &user, term, 0,
			      dest_realm, &key, family);
	  krb_errno = errno;
	  if (rem > 0)
	    {
	      keytype = shishi_key_type (key);
	      keylen = shishi_cipher_blocksize (keytype);

	      ivtab[0] = &iv1;
	      ivtab[1] = &iv2;

	      for (i = 0; i < 2; i++)
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
		      memset (ivtab[i]->iv, !i, ivtab[i]->ivlen);
		      ivtab[i]->ctx =
			shishi_crypto (handle, key, ivtab[i]->keyusage,
				       shishi_key_type (key), ivtab[i]->iv,
				       ivtab[i]->ivlen);
		      break;
		    case SHISHI_ARCFOUR_HMAC:
		    case SHISHI_ARCFOUR_HMAC_EXP:
		      ivtab[i]->keyusage =
			SHISHI_KEYUSAGE_KCMD_DES + 2 + 4 * i;
		      ivtab[i]->ctx =
			shishi_crypto (handle, key, ivtab[i]->keyusage,
				       shishi_key_type (key), NULL, 0);
		      break;
		    default:
		      ivtab[i]->keyusage =
			SHISHI_KEYUSAGE_KCMD_DES + 2 + 4 * i;
		      ivtab[i]->iv = xmalloc (ivtab[i]->ivlen);
		      memset (ivtab[i]->iv, 0, ivtab[i]->ivlen);
		      ivtab[i]->ctx =
			shishi_crypto (handle, key, ivtab[i]->keyusage,
				       shishi_key_type (key), ivtab[i]->iv,
				       ivtab[i]->ivlen);
		    }
		}
	    }
	}

      else
#  else /* KERBEROS */
	{
	  rem = krcmd_mutual (&host, sp->s_port, user, term, 0,
			      dest_realm, &cred, schedule);
	  krb_errno = errno;
	}
      else
#  endif
# endif	/* ENCRYPTION */
	{
# if defined SHISHI
	  rem = krcmd (&handle, &host, sp->s_port, &user, term, 0,
		       dest_realm, family);
# else /* KERBEROS */
	  rem = krcmd (&host, sp->s_port, user, term, 0, dest_realm);
# endif
	  krb_errno = errno;
	}
      if (rem < 0)
	{
	  use_kerberos = 0;
	  if (krb_errno == ECONNREFUSED)
	    warning ("remote host doesn't support Kerberos");
	  else if (krb_errno == ENOENT)
	    error (EXIT_FAILURE, 0, "Can't provide Kerberos auth data.");
	  else
	    error (EXIT_FAILURE, 0, "Kerberos authentication failed.");

	  sp = getservbyname ("login", "tcp");
	  if (sp == NULL)
	    error (EXIT_FAILURE, 0, "unknown service login/tcp.");
	  goto try_connect;
	}
    }

  else
    {
      char *p = strchr (host, '/');

# ifdef ENCRYPTION
      if (doencrypt)
	error (EXIT_FAILURE, 0, "the -x flag requires Kerberos authentication.");
# endif	/* ENCRYPTION */
      if (!user)
	user = pw->pw_name;
      if (p)
	host = ++p;	/* Skip prefix like `host/'.  */

# ifdef WITH_ORCMD_AF
      rem = orcmd_af (&host, sp->s_port, pw->pw_name, user, term, 0, family);
# elif defined WITH_RCMD_AF
      rem = rcmd_af (&host, sp->s_port, pw->pw_name, user, term, 0, family);
# elif defined WITH_ORCMD
      rem = orcmd (&host, sp->s_port, pw->pw_name, user, term, 0);
# else /* !WITH_ORCMD_AF && !WITH_RCMD_AF && !WITH_ORCMD */
      rem = rcmd (&host, sp->s_port, pw->pw_name, user, term, 0);
# endif
    }
#else /* !KERBEROS && !SHISHI */
  if (!user)
    user = pw->pw_name;

# ifdef WITH_ORCMD_AF
  rem = orcmd_af (&host, sp->s_port, pw->pw_name, user, term, 0, family);
# elif defined WITH_RCMD_AF
  rem = rcmd_af (&host, sp->s_port, pw->pw_name, user, term, 0, family);
# elif defined WITH_ORCMD
  rem = orcmd (&host, sp->s_port, pw->pw_name, user, term, 0);
# else /* !WITH_ORCMD_AF && !WITH_RCMD_AF && !WITH_ORCMD */
  rem = rcmd (&host, sp->s_port, pw->pw_name, user, term, 0);
# endif
#endif /* KERBEROS */

  if (rem < 0)
    {
      puts ("");	/* Glibc does not close all error strings in rcmd().  */
      /* rcmd() provides its own error messages,
       * but we add a vital addition, caused by
       * insufficient capabilites.
       */
      if (errno == EACCES)
	error (EXIT_FAILURE, 0, "No access to privileged ports.");

      exit (EXIT_FAILURE);
    }

  {
    int one = 1;
    if (dflag && setsockopt (rem, SOL_SOCKET, SO_DEBUG, (char *) &one,
			     sizeof one) < 0)
      error (0, errno, "setsockopt DEBUG (ignored)");
  }

#if defined IP_TOS && defined IPPROTO_IP && defined IPTOS_LOWDELAY
  {
    struct sockaddr_storage ss;
    socklen_t sslen = sizeof (ss);
    int one = IPTOS_LOWDELAY;

    (void) getpeername (rem, (struct sockaddr *) &ss, &sslen);
    if (ss.ss_family == AF_INET &&
	setsockopt (rem, IPPROTO_IP, IP_TOS,
		    (char *) &one, sizeof (int)) < 0)
      error (0, errno, "setsockopt TOS (ignored)");
  }
#endif

  /* Now change to the real user ID.  We have to be set-user-ID root
     to get the privileged port that rcmd () uses.  We now want, however,
     to run as the real user who invoked us.  */
  seteuid (uid);
  setuid (uid);

  doit (&osmask);	/* The old mask will activate SIGURG and SIGUSR1!  */

  return 0;
}

/* On some systems, like QNX/Neutrino, the constants B0, B50, etcetera,
 * map straight to the actual speed (0, 50, etcetera), whereas on other
 * systems like GNU/Linux, they map to ordered constants (0, 1, etcetera),
 * i.e, the values are only order encoded.  cfgetispeed() should according
 * to POSIX return a constant value representing the numerical speed.
 * To be portable we need to do the conversion ourselves.  */
/* Some values are not defined by POSIX.  */
#ifndef B7200
# define B7200   B4800
#endif

#ifndef B14400
# define B14400  B9600
#endif

#ifndef B19200
# define B19200 B14400
#endif

#ifndef B28800
# define B28800  B19200
#endif

#ifndef B38400
# define B38400 B28800
#endif

#ifndef B57600
# define B57600  B38400
#endif

#ifndef B76800
# define B76800  B57600
#endif

#ifndef B115200
# define B115200 B76800
#endif

#ifndef B230400
# define B230400 B115200
#endif
struct termspeeds
{
  unsigned int speed;
  unsigned int sym;
} termspeeds[] =
  {
    {0, B0},
    {50, B50},
    {75, B75},
    {110, B110},
    {134, B134},
    {150, B150},
    {200, B200},
    {300, B300},
    {600, B600},
    {1200, B1200},
    {1800, B1800},
    {2400, B2400},
    {4800, B4800},
    {7200, B7200},
    {9600, B9600},
    {14400, B14400},
    {19200, B19200},
    {28800, B28800},
    {38400, B38400},
    {57600, B57600},
    {115200, B115200},
    {230400, B230400},
    {-1, B230400}
  };

unsigned int
speed_translate (unsigned int sym)
{
  unsigned int i;
  for (i = 0; i < (sizeof (termspeeds) / sizeof (*termspeeds)); i++)
    {
      if (termspeeds[i].sym == sym)
	return termspeeds[i].speed;
    }
  return 0;
}

/* Returns the terminal speed for the file descriptor FD,
   or SPEED_NOTATTY if FD is not associated with a terminal.  */
int
speed (int fd)
{
  struct termios tt;

  if (tcgetattr (fd, &tt) == 0)
    {
      /* speed_t sp; */
      unsigned int sp = cfgetispeed (&tt);
      return speed_translate (sp);
    }
  return SPEED_NOTATTY;
}

pid_t child;
struct termios deftt;
struct termios ixon_state;
struct termios nott;

void
doit (sigset_t * osmask)
{
  int i;
  struct sigaction sa;

  for (i = 0; i < NCCS; i++)
    nott.c_cc[i] = _POSIX_VDISABLE;
  tcgetattr (0, &deftt);
  nott.c_cc[VSTART] = deftt.c_cc[VSTART];
  nott.c_cc[VSTOP] = deftt.c_cc[VSTOP];

  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = SIG_IGN;
  (void) sigaction (SIGINT, &sa, NULL);

  setsignal (SIGHUP);
  setsignal (SIGQUIT);

  child = fork ();
  if (child == -1)
    {
      error (0, errno, "fork");
      done (1);
    }
  if (child == 0)
    {
      mode (1);
      if (reader (osmask) == 0)
	{
	  /* If the reader returns zero, the socket to the server returned
	     an EOF, meaning the client logged out of the remote system.
	     This is the normal termination.  */
#ifdef SHISHI
	  if (use_kerberos)
	    {
# ifdef ENCRYPTION
	      if (doencrypt)
		{
		  shishi_key_done (key);
		  shishi_crypto_close (iv1.ctx);
		  shishi_crypto_close (iv2.ctx);
		  free (iv1.iv);
		  free (iv2.iv);
		}
# endif /* ENCRYPTION */
	      shishi_done (handle);
	    }
#endif /* SHISHI */
          error (0, 0, "Connection to %s closed normally.\r", host);
          /* EXIT_SUCCESS is usually zero. So error might not exit.  */
          exit (EXIT_SUCCESS);
	}
      /* If the reader returns non-zero, the socket to the server
         returned an error.  Something went wrong.  */
      sleep (1);
      error (EXIT_FAILURE, 0, "\007Connection to %s closed with error.\r", host);
    }

  /*
   * Parent process == writer.
   *
   * We may still own the socket, and may have a pending SIGURG (or might
   * receive one soon) that we really want to send to the reader.  When
   * one of these comes in, the trap copytochild simply copies such
   * signals to the child. We can now unblock SIGURG and SIGUSR1
   * that were set above.
   */
  /* Reenable SIGURG and SIGUSR1.  */
  sigprocmask (SIG_SETMASK, osmask, (sigset_t *) 0);

  sa.sa_handler = catch_child;
  (void) sigaction (SIGCHLD, &sa, NULL);

  writer ();

  /* If the writer returns, it means the user entered "~." on the terminal.
     In this case we terminate and the server will eventually get an EOF
     on its end of the network connection.  This should cause the server to
     log you out on the remote system.  */
  error (0, 0, "Connection to %s aborted.\r", host);

#ifdef SHISHI
  if (use_kerberos)
    {
      shishi_done (handle);
# ifdef ENCRYPTION
      if (doencrypt)
	{
	  shishi_key_done (key);
	  shishi_crypto_close (iv1.ctx);
	  shishi_crypto_close (iv2.ctx);
	  free (iv1.iv);
	  free (iv2.iv);
	}
# endif
    }
#endif

  done (0);
}

/*
 * Install an exit handler, unless the signal is already being ignored.
   This function is called before fork () for SIGHUP and SIGQUIT.  */
void
setsignal (int sig)
{
  int rc;
  struct sigaction sa;

  /* Query the present disposition of SIG.
   * This achieves minimal manipulation.  */
  sa.sa_flags = 0;
  sa.sa_handler = NULL;
  sigemptyset (&sa.sa_mask);
  rc = sigaction (sig, NULL, &sa);

  /* Set action to exit, unless the signal is ignored.  */
  if (!rc && sa.sa_handler != SIG_IGN)
    {
      sa.sa_handler = _exit;
      (void) sigaction (sig, &sa, NULL);
    }
}

/* This function is called by the parent:
   (1) at the end (user terminates the client end);
   (2) SIGCHLD signal - the catch_child () function.
   (3) SIGPIPE signal - the connection was lost.

   We send the child a SIGKILL signal, which it can't ignore, then
   wait for it to terminate.  */
void
done (int status)
{
  pid_t w;
  int wstatus;

  mode (0);	/* FIXME: Calls tcgetattr/tcsetattr in signal handler.  */
  if (child > 0)
    {
      /* make sure catch_child does not snap it up */
      setsig (SIGCHLD, SIG_DFL);		/* XXX: Replace setsig()?  */
      if (kill (child, SIGKILL) >= 0)
	while ((w = waitpid (-1, &wstatus, WNOHANG)) > 0 && w != child)
	  continue;
    }
  exit (status);
}

int dosigwinch;

/*
 * This is called when the reader process gets the out-of-band (urgent)
 * request to turn on the window-changing protocol.
 *
 * Input signal is SIGUSR1, but SIGWINCH is being activated.
 *
 * FIXME: Race condition due to sendwindow() in signal handler?
 */
void
writeroob (int signo _GL_UNUSED_PARAMETER)
{
  if (dosigwinch == 0)
    {
      struct sigaction sa;

      sendwindow ();

      sa.sa_flags = SA_RESTART;
      sa.sa_handler = sigwinch;
      sigemptyset (&sa.sa_mask);
      (void) sigaction (SIGWINCH, &sa, NULL);
    }
  dosigwinch = 1;
}

/* FIXME: System dependent race condition due to done().
 */
void
catch_child (int signo _GL_UNUSED_PARAMETER)
{
  int status;
  pid_t pid;

  for (;;)
    {
      pid = waitpid (-1, &status, WNOHANG | WUNTRACED);
      if (pid == 0)
	return;
      /* if the child (reader) dies, just quit */
      if (pid < 0 && errno == EINTR)
	continue;
      if (pid < 0 || (pid == child && !WIFSTOPPED (status)))
	done (WEXITSTATUS (status) | WTERMSIG (status));
    }
}

/*
 * writer: write to remote: 0 -> line.
 * ~.				terminate
 * ~^Z				suspend rlogin process.
 * ~<delayed-suspend char>	suspend rlogin process, but leave reader alone.
 */
void
writer (void)
{
  register int bol, local, n;
  char c;

  bol = 1;			/* beginning of line */
  local = 0;
  for (;;)
    {
      n = read (STDIN_FILENO, &c, 1);
      if (n <= 0)
	{
	  if (n < 0 && errno == EINTR)
	    continue;
	  break;
	}
      /*
       * If we're at the beginning of the line and recognize a
       * command character, then we echo locally.  Otherwise,
       * characters are echo'd remotely.  If the command character
       * is doubled, this acts as a force and local echo is
       * suppressed.
       */
      if (bol)
	{
	  bol = 0;
	  if (!noescape && c == escapechar)
	    {
	      local = 1;
	      continue;
	    }
	}
      else if (local)
	{
	  local = 0;
	  if (c == '.' || c == deftt.c_cc[VEOF])
	    {
	      echo (c);
	      break;
	    }
	  if (c == deftt.c_cc[VSUSP]
#ifdef VDSUSP
	      || c == deftt.c_cc[VDSUSP]
#endif
	    )
	    {
	      bol = 1;
	      echo (c);
	      stop (c);
	      continue;
	    }
	  if (c != escapechar)
	    {
#ifdef ENCRYPTION
# ifdef KERBEROS
	      if (doencrypt)
		des_write (rem, (char *) &escapechar, 1);
	      else
# elif defined(SHISHI)
	      if (doencrypt)
		writeenc (handle, rem, (char *) &escapechar, 1, &wlen,
			  &iv2, key, 2);
	      else
# endif /* SHISHI */
#endif /* ENCRYPTION */
		write (rem, &escapechar, 1);
	    }
	}

#ifdef ENCRYPTION
# ifdef KERBEROS
      if (doencrypt)
	{
	  if (des_write (rem, &c, 1) == 0)
	    {
              error (0, 0, "line gone");
	      break;
	    }
	}
      else
# elif defined(SHISHI)
      if (doencrypt)
	{
	  writeenc (handle, rem, &c, 1, &wlen, &iv2, key, 2);
	  if (wlen == 0)
	    {
              error (0, 0, "line gone");
	      break;
	    }
	}
      else
# endif
#endif
      if (write (rem, &c, 1) == 0)
	{
          error (0, 0, "line gone");
	  break;
	}
      bol = c == deftt.c_cc[VKILL] || c == deftt.c_cc[VEOF] ||
	c == deftt.c_cc[VINTR] || c == deftt.c_cc[VSUSP] ||
	c == '\r' || c == '\n';
    }
}

void
echo (register char c)
{
  register char *p;
  char buf[8];

  p = buf;
  c &= 0177;
  *p++ = escapechar;
  if (c < ' ')
    {
      *p++ = '^';
      *p++ = c + '@';
    }
  else if (c == 0177)
    {
      *p++ = '^';
      *p++ = '?';
    }
  else
    *p++ = c;
  *p++ = '\r';
  *p++ = '\n';
  write (STDOUT_FILENO, buf, p - buf);
}

void
stop (char cmdc)
{
  mode (0);
  setsig (SIGCHLD, SIG_IGN);		/* XXX: Replace setsig()?  */
  kill (cmdc == deftt.c_cc[VSUSP] ? 0 : getpid (), SIGTSTP);
  setsig (SIGCHLD, catch_child);
  mode (1);
  sigwinch (0);			/* check for size changes */
}

void
sigwinch (int signo _GL_UNUSED_PARAMETER)
{
  struct winsize ws;

  if (dosigwinch && get_window_size (STDIN_FILENO, &ws) == 0
      && memcmp (&ws, &winsize, sizeof ws))
    {
      winsize = ws;
      sendwindow ();
    }
}

/*
 * Send the window size to the server via the magic escape
 *
 * FIXME: Used in signal handler. Race condition?
 */
void
sendwindow (void)
{
  struct winsize *wp;
  char obuf[4 + sizeof (struct winsize)];

  wp = (struct winsize *) (obuf + 4);
  obuf[0] = 0377;
  obuf[1] = 0377;
  obuf[2] = 's';
  obuf[3] = 's';
  wp->ws_row = htons (winsize.ws_row);
  wp->ws_col = htons (winsize.ws_col);
  wp->ws_xpixel = htons (winsize.ws_xpixel);
  wp->ws_ypixel = htons (winsize.ws_ypixel);

#ifdef ENCRYPTION
# ifdef KERBEROS
  if (doencrypt)
    des_write (rem, obuf, sizeof obuf);
  else
# elif defined(SHISHI)
  if (doencrypt)
    writeenc (handle, rem, obuf, sizeof obuf, &wlen, &iv2, key, 2);
  else
# endif
#endif
    write (rem, obuf, sizeof obuf);
}

/*
 * reader: read from remote: line -> 1
 */
#define READING	1
#define WRITING	2

jmp_buf rcvtop;
pid_t ppid;
int rcvcnt, rcvstate;
char rcvbuf[8 * 1024];

void
oob (int signo _GL_UNUSED_PARAMETER)
{
  char mark;
  struct termios tt;
  int atmark, n, out, rcvd;
  char waste[BUFSIZ];

  out = O_RDWR;
  rcvd = 0;

#ifdef SHISHI
  if (use_kerberos)
    mark = rcvbuf[4];		/* Payload in fifth byte.  */
  else
#endif
    while (recv (rem, &mark, 1, MSG_OOB) < 0)
      {
	switch (errno)
	  {
	  case EWOULDBLOCK:
	    /*
	     * Urgent data not here yet.  It may not be possible
	     * to send it yet if we are blocked for output and
	     * our input buffer is full.
	     */
	    if ((size_t) rcvcnt < sizeof rcvbuf)
	      {
		n = read (rem, rcvbuf + rcvcnt, sizeof (rcvbuf) - rcvcnt);
		if (n <= 0)
		  return;
		rcvd += n;
	      }
	    else
	      {
		n = read (rem, waste, sizeof waste);
		if (n <= 0)
		  return;
	      }
	    continue;
	  default:
	    return;
	  }
      }

  if (mark & TIOCPKT_WINDOW)
    {
      /* Let server know about window size changes */
      kill (ppid, SIGUSR1);
    }
  if (!eight && (mark & TIOCPKT_NOSTOP))
    {
      tcgetattr (0, &tt);
      tt.c_iflag &= ~(IXON | IXOFF);
      tt.c_cc[VSTOP] = _POSIX_VDISABLE;
      tt.c_cc[VSTART] = _POSIX_VDISABLE;
      tcsetattr (0, TCSANOW, &tt);
    }
  if (!eight && (mark & TIOCPKT_DOSTOP))
    {
      tcgetattr (0, &tt);
      tt.c_iflag |= (IXON | IXOFF);
      tt.c_cc[VSTOP] = deftt.c_cc[VSTOP];
      tt.c_cc[VSTART] = deftt.c_cc[VSTART];
      tcsetattr (0, TCSANOW, &tt);
    }
  if (mark & TIOCPKT_FLUSHWRITE)
    {
#ifdef TIOCFLUSH		/* BSD and Solaris.  */
      ioctl (STDOUT_FILENO, TIOCFLUSH, (char *) &out);
#elif defined TCIOFLUSH		/* Glibc, BSD, and Solaris.  */
      out = TCIOFLUSH;
      ioctl (STDOUT_FILENO, TCIOFLUSH, &out);
#endif
      for (;;)
	{
	  if (ioctl (rem, SIOCATMARK, &atmark) < 0)
	    {
	      error (0, errno, "ioctl SIOCATMARK (ignored)");
	      break;
	    }
	  if (atmark)
	    break;
	  n = read (rem, waste, sizeof waste);
	  if (n <= 0)
	    break;
	}
      /*
       * Don't want any pending data to be output, so clear the recv
       * buffer.  If we were hanging on a write when interrupted,
       * don't want it to restart.  If we were reading, restart
       * anyway.
       */
      rcvcnt = 0;
      longjmp (rcvtop, 1);
    }

  /* oob does not do FLUSHREAD (alas!) */

  /*
   * If we filled the receive buffer while a read was pending, longjmp
   * to the top to restart appropriately.  Don't abort a pending write,
   * however, or we won't know how much was written.
   */
  if (rcvd && rcvstate == READING)
    longjmp (rcvtop, 1);
}

/* reader: read from remote: line -> 1 */
int
reader (sigset_t * osmask)
{
  pid_t pid;
  int n, remaining;
  char *bufp;
  struct sigaction sa;

  pid = getpid ();	/* Modern systems use positive values for pid.  */
  ppid = getppid ();

  fcntl (rem, F_SETOWN, pid);		/* Get ownership early.  */

  sa.sa_flags = SA_RESTART;
  sa.sa_handler = SIG_IGN;
  sigemptyset (&sa.sa_mask);
  (void) sigaction (SIGTTOU, &sa, NULL);
  sa.sa_handler = oob;
  (void) sigaction (SIGURG, &sa, NULL);

  setjmp (rcvtop);
  sigprocmask (SIG_SETMASK, osmask, (sigset_t *) 0);
  bufp = rcvbuf;

  for (;;)
    {
#ifdef SHISHI
      if (use_kerberos)
	{
	  if ((rcvcnt >= 5) && (bufp[0] == '\377') && (bufp[1] == '\377'))
	    if ((bufp[2] == 'o') && (bufp[3] == 'o'))
	      {
		oob (1);
		bufp += 5;
	      }
	}
#endif
      while ((remaining = rcvcnt - (bufp - rcvbuf)) > 0)
	{
	  rcvstate = WRITING;
	  n = write (STDOUT_FILENO, bufp, remaining);
	  if (n < 0)
	    {
	      if (errno != EINTR)
		return -1;
	      continue;
	    }
	  bufp += n;
	}
      bufp = rcvbuf;
      rcvcnt = 0;
      rcvstate = READING;

#ifdef ENCRYPTION
# ifdef KERBEROS
      if (doencrypt)
	rcvcnt = des_read (rem, rcvbuf, sizeof rcvbuf);
      else
# elif defined(SHISHI)
      if (doencrypt)
	readenc (handle, rem, rcvbuf, &rcvcnt, &iv1, key, 2);
      else
# endif
#endif
	rcvcnt = read (rem, rcvbuf, sizeof rcvbuf);
      if (rcvcnt == 0)
	return 0;
      if (rcvcnt < 0)
	{
	  if (errno == EINTR)
	    continue;
	  error (0, errno, "read");
	  return -1;
	}
    }
}

void
mode (int f)
{
  struct termios tt;

  switch (f)
    {
    case 0:
      /* remember whether IXON is set, set it can be restore at mode(1) */
      tcgetattr (0, &ixon_state);
      tcsetattr (0, TCSADRAIN, &deftt);
      break;
    case 1:
      tt = deftt;
      tt.c_oflag &= ~(OPOST);
      tt.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
      tt.c_iflag &= ~(ICRNL);
      tt.c_cc[VMIN] = 1;
      tt.c_cc[VTIME] = 0;
      if (eight)
	{
	  tt.c_iflag &= ~(IXON | IXOFF | ISTRIP);
	  tt.c_cc[VSTOP] = _POSIX_VDISABLE;
	  tt.c_cc[VSTART] = _POSIX_VDISABLE;
	}
      if ((ixon_state.c_iflag & IXON) && !eight)
	tt.c_iflag |= IXON;
      else
	tt.c_iflag &= ~IXON;
      tcsetattr (0, TCSADRAIN, &tt);
      break;

    default:
      return;
    }
}

/* FIXME: Race condition due to done() in signal handler?
 */
void
lostpeer (int signo)
{
  setsig (signo, SIG_IGN);	/* Used with SIGPIPE only.  */
  error (0, 0, "\007Connection to %s lost.\r", host);
  done (1);
}

/* copy SIGURGs to the child process. */
void
copytochild (int signo)
{
  kill (child, signo);
}

#if defined KERBEROS || defined SHISHI
void
warning (const char *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "rlogin: warning, using standard rlogin: ");
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fprintf (stderr, ".\n");
}
#endif

/*
 * The following routine provides compatibility (such as it is) between older
 * Suns and others.  Suns have only a `ttysize', so we convert it to a winsize.
 */
#ifdef OLDSUN
int
get_window_size (int fd, struct winsize *wp)
{
  struct ttysize ts;
  int error;

  error = ioctl (0, TIOCGSIZE, &ts);
  if (error != 0)
    return error;

  wp->ws_row = ts.ts_lines;
  wp->ws_col = ts.ts_cols;
  wp->ws_xpixel = 0;
  wp->ws_ypixel = 0;
  return 0;
}
#endif

u_int
getescape (register char *p)
{
  long val;
  int len;

  len = strlen (p);
  if (len == 1)		/* use any single char, including '\'.  */
    return ((u_int) * p);

  /* otherwise, \nnn */
  if (*p == '\\' && len >= 2 && len <= 4)
    {
      val = strtol (++p, NULL, 8);
      for (;;)
	{
	  if (!*++p)
	    return ((u_int) val);
	  if (*p < '0' || *p > '8')
	    break;
	}
    }
  return 0;
}
