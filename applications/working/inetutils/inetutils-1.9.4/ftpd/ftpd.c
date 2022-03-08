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

/* - Ftp Server
 * Copyright (c) 1985, 1988, 1990, 1992, 1993, 1994
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
 * FTP server.
 */

#include <config.h>

#include <alloca.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif

#define FTP_NAMES
#include <arpa/ftp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <grp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
/* Include glob.h last, because it may define "const" which breaks
   system headers on some platforms. */
#include <glob.h>
#include <argp.h>
#include <error.h>
#include <xgetcwd.h>

#include <progname.h>
#include <libinetutils.h>
#include "extern.h"
#include "unused-parameter.h"

#ifndef LINE_MAX
# define LINE_MAX 2048
#endif

#ifndef LOG_FTP
# define LOG_FTP LOG_DAEMON	/* Use generic facility.  */
#endif

#ifndef MAP_FAILED
# define MAP_FAILED (void*)-1
#endif

#if !HAVE_DECL_FCLOSE
/* Some systems don't declare fclose in <stdio.h>, so do it ourselves.  */
extern int fclose (FILE *);
#endif

/* Exported to ftpcmd.h.  */
struct sockaddr_storage data_dest;	/* Data port.  */
socklen_t data_dest_len;
struct sockaddr_storage his_addr;	/* Peer address.  */
socklen_t his_addrlen;
int logging;			/* Enable log to syslog.  */
int no_version;			/* Don't print version to client.  */
int type = TYPE_A;		/* Default TYPE_A.  */
int form = FORM_N;		/* Default FORM_N.  */
int debug;			/* Enable debug mode if 1.  */
int rfc2577 = 1;		/* Follow suggestions in RFC 2577.  */
int timeout = 900;		/* Timeout after 15 minutes of inactivity.  */
int maxtimeout = 7200;		/* Don't allow idle time to be set
				   beyond 2 hours.  */
int pdata = -1;			/* For passive mode.  */
char *hostname;			/* Who we are.  */
int usedefault = 1;		/* For data transfers.  */
char tmpline[7];		/* Temp buffer use in OOB.  */
char addrstr[NI_MAXHOST];	/* Host name or address string.  */
char portstr[8];		/* Numeric port as string.  */

/* Requester credentials.  */
struct credentials cred;

static struct sockaddr_storage ctrl_addr;	/* Control address.  */
static socklen_t ctrl_addrlen;
static struct sockaddr_storage data_source;	/* Port address.  */
static socklen_t data_source_len;
static struct sockaddr_storage pasv_addr;	/* Pasv address.  */
static socklen_t pasv_addrlen;

static int data = -1;		/* Port data connection socket.  */
static jmp_buf urgcatch;
static int stru = STRU_F;	/* Avoid C keyword.  */
static int stru_mode = MODE_S;	/* Default STRU mode stru_mode = MODE_S.  */
static int anon_only;		/* Allow only anonymous login.  */
static int daemon_mode;		/* Start in daemon mode.  */
static off_t file_size;
static off_t byte_count;
static sig_atomic_t transflag;	/* Flag where in a middle of transfer.  */
static const char *pid_file = PATH_FTPDPID;
#if !defined CMASK || CMASK == 0
# undef CMASK
# define CMASK 027
#endif
static int defumask = CMASK;	/* Default umask value.  */
static int login_attempts;	/* Number of failed login attempts.  */
static int askpasswd;		/* Had user command, ask for passwd.  */
static char curname[10];	/* Current USER name.  */
static char ttyline[20];	/* Line to log in utmp.  */


#define NUM_SIMUL_OFF_TO_STRS 4

/* Returns a string with the decimal representation of the off_t OFF, taking
   into account that off_t might be longer than a long.  The return value is
   a pointer to a static buffer, but a return value will only be reused every
   NUM_SIMUL_OFF_TO_STRS calls, to allow multiple off_t's to be conveniently
   printed with a single printf statement.  */
static char *
off_to_str (off_t off)
{
  static char bufs[NUM_SIMUL_OFF_TO_STRS][80];
  static char (*next_buf)[80] = bufs;

  if (next_buf >= (bufs + NUM_SIMUL_OFF_TO_STRS))
    next_buf = bufs;

  if (sizeof (off) > sizeof (long))
    sprintf (*next_buf, "%lld", (long long int) off);
  else if (sizeof (off) == sizeof (long))
    sprintf (*next_buf, "%ld", (long) off);
  else
    sprintf (*next_buf, "%d", (int) off);

  return *next_buf++;
}

/*
 * Timeout intervals for retrying connections
 * to hosts that don't accept PORT cmds.  This
 * is a kludge, but given the problems with TCP...
 */
#define SWAITMAX	90	/* wait at most 90 seconds */
#define SWAITINT	5	/* interval between retries */

static int swaitmax = SWAITMAX;
static int swaitint = SWAITINT;

#ifdef HAVE_SETPROCTITLE
char proctitle[LINE_MAX];	/* initial part of title */
#endif /* SETPROCTITLE */

#define LOGCMD(cmd, file) \
	if (logging > 1) \
	    syslog(LOG_INFO,"%s %s%s", cmd, \
		*(file) == '/' ? "" : curdir(), file);
#define LOGCMD2(cmd, file1, file2) \
	 if (logging > 1) \
	    syslog(LOG_INFO,"%s %s%s %s%s", cmd, \
		*(file1) == '/' ? "" : curdir(), file1, \
		*(file2) == '/' ? "" : curdir(), file2);
#define LOGBYTES(cmd, file, cnt) \
	if (logging > 1) { \
		if (cnt == (off_t)-1) \
		    syslog(LOG_INFO,"%s %s%s", cmd, \
			*(file) == '/' ? "" : curdir(), file); \
		else \
		    syslog(LOG_INFO, "%s %s%s = %s bytes", \
			cmd, (*(file) == '/') ? "" : curdir(), file, \
			   off_to_str (cnt)); \
	}

extern int yyparse (void);

static void ack (const char *);
#ifdef HAVE_LIBWRAP
static int check_host (struct sockaddr *sa, socklen_t len);
#endif
static void complete_login (struct credentials *);
static char *curdir (void);
static FILE *dataconn (const char *, off_t, const char *);
static void dolog (struct sockaddr *, socklen_t, struct credentials *);
static void end_login (struct credentials *);
static FILE *getdatasock (const char *);
static char *gunique (const char *);
static void lostconn (int);
static void myoob (int);
static int receive_data (FILE *, FILE *, off_t);
static void send_data (FILE *, FILE *, off_t);
static void sigquit (int);

const char doc[] =
#ifdef WITH_PAM
  "File Transfer Protocol daemon, offering PAM service 'ftp'.";
#else
  "File Transfer Protocol daemon.";
#endif

enum {
  OPT_NONRFC2577 = CHAR_MAX + 1,
};

static struct argp_option options[] = {
#define GRID 0
  { "anonymous-only", 'A', NULL, 0,
    "server configured for anonymous service only",
    GRID+1 },
  { "daemon", 'D', NULL, 0,
    "start the ftpd standalone",
    GRID+1 },
  { "debug", 'd', NULL, 0,
    "debug mode",
    GRID+1 },
  { "ipv4", '4', NULL, 0,
    "restrict daemon to IPv4",
    GRID+1 },
  { "ipv6", '6', NULL, 0,
    "restrict daemon to IPv6",
    GRID+1 },
  { "logging", 'l', NULL, 0,
    "increase verbosity of syslog messages",
    GRID+1 },
  { "pidfile", 'p', "PIDFILE", OPTION_ARG_OPTIONAL,
    "change default location of pidfile",
    GRID+1 },
  { "no-version", 'q', NULL, 0,
    "do not display version in banner",
    GRID+1 },
  { "timeout", 't', "TIMEOUT", 0,
    "set default idle timeout",
    GRID+1 },
  { "max-timeout", 'T', NULL, 0,
    "reset maximum value of timeout allowed",
    GRID+1 },
  { "non-rfc2577", OPT_NONRFC2577, NULL, 0,
    "neglect RFC 2577 by giving info on missing users",
    GRID+1 },
  { "umask", 'u', "VAL", 0,
    "set default umask",
    GRID+1 },
  { "auth", 'a', "AUTH", 0,
    "use AUTH for authentication",
    GRID+1 },
  { NULL, 0, NULL, 0, "AUTH can be one of the following:", GRID+2 },
  { "  default", 0, NULL, OPTION_DOC|OPTION_NO_TRANS,
    "passwd authentication",
    GRID+3 },
#ifdef WITH_PAM
  { "  pam", 0, NULL, OPTION_DOC|OPTION_NO_TRANS,
    "using PAM service 'ftp'",
    GRID+3 },
#endif
#ifdef WITH_KERBEROS
  { "  kerberos", 0, NULL, OPTION_DOC|OPTION_NO_TRANS,
    "",
    GRID+3 },
#endif
#ifdef WITH_KERBEROS5
  { "  kerberos5", 0, NULL, OPTION_DOC|OPTION_NO_TRANS,
    "",
    GRID+3 },
#endif
#ifdef WITH_OPIE
  { "  opie", 0, NULL, OPTION_DOC|OPTION_NO_TRANS,
    "",
    GRID+3 },
#endif
  { NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case '4':
      /* Active in daemon mode only.  */
      usefamily = AF_INET;
      break;

    case '6':
      /* Active in daemon mode only.  */
      usefamily = AF_INET6;
      break;

    case 'A':
      /* Anonymous ftp only.  */
      anon_only = 1;
      break;

    case 'a':
      if (strcasecmp (arg, "default") == 0)
	cred.auth_type = AUTH_TYPE_PASSWD;
#ifdef WITH_PAM
      else if (strcasecmp (arg, "pam") == 0)
	cred.auth_type = AUTH_TYPE_PAM;
#endif
#ifdef WITH_KERBEROS
      else if (strcasecmp (arg, "kerberos") == 0)
	cred.auth_type = AUTH_TYPE_KERBEROS;
#endif
#ifdef WITH_KERBEROS5
      else if (strcasecmp (arg, "kerberos5") == 0)
	cred.auth_type = AUTH_TYPE_KERBEROS5;
#endif
#ifdef WITH_OPIE
      else if (strcasecmp (arg, "opie") == 0)
	cred.auth_type = AUTH_TYPE_OPIE;
#endif
      break;

    case 'D':
      /* Run ftpd as daemon.  */
      daemon_mode = 1;
      break;

    case 'd':
      /* Enable debug mode.  */
      debug = 1;
      break;

    case 'l':
      /* Increase logging level.  */
      logging++;		/* > 1 == Extra logging.  */
      break;

    case 'p':
      /* Override pid file */
      pid_file = arg;
      break;

    case 'q':
      /* Don't include version number in banner.  */
      no_version = 1;
      break;

    case 't':
      /* Set default timeout value.  */
      timeout = atoi (arg);
      if (maxtimeout < timeout)
	maxtimeout = timeout;
      break;

    case 'T':		/* Maximum timeout allowed.  */
      maxtimeout = atoi (arg);
      if (timeout > maxtimeout)
	timeout = maxtimeout;
      break;

    case 'u':		/* Set umask.  */
      {
	long val = 0;

	val = strtol (arg, &arg, 8);
	if (*arg != '\0' || val < 0)
	  argp_error (state, "bad value for -u");
	else
	  defumask = val;
	break;
      }

    case OPT_NONRFC2577:
      rfc2577 = 0;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp = {
  options,
  parse_opt,
  NULL,
  doc,
  NULL,
  NULL,
  NULL
};

int
main (int argc, char *argv[], char **envp)
{
  int index;

  set_program_name (argv[0]);

#ifdef HAVE_TZSET
  tzset ();			/* In case no timezone database in ~ftp.  */
#endif

#ifdef HAVE_INITSETPROCTITLE
  /* Save start and extent of argv for setproctitle.  */
  initsetproctitle (argc, argv, envp);
#else /* !HAVE_INITSETPROCTITLE */
  (void) envp;		/* Silence warnings.  */
#endif

  /* Parse the command line */
  iu_argp_init ("ftpd", default_program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  /* Bail out, wrong usage */
  if (argc - index != 0)
    error (EXIT_FAILURE, 0, "surplus arguments; try `%s --help' for more info",
	   program_name);

  /* LOG_NDELAY sets up the logging connection immediately,
     necessary for anonymous ftp's that chroot and can't do it later.  */
  openlog ("ftpd", LOG_PID | LOG_NDELAY, LOG_FTP);
  freopen (PATH_DEVNULL, "w", stderr);

  /* If not running via inetd, we detach and dup(fd, 0), dup(fd, 1) the
     fd = accept(). tcpd is check if compile with the support  */
  if (daemon_mode)
    {
#ifndef HAVE_FORK
      /* Shift out the daemon option in subforks  */
      int i;
      for (i = 0; i < argc; ++i)
	if (strcmp (argv[i], "-D") == 0)
	  {
	    int j;
	    for (j = i; j < argc; ++j)
	      argv[j] = argv[j + 1];
	    argv[--argc] = NULL;
	  }
#endif
      his_addrlen = sizeof (his_addr);
      if (server_mode (pid_file, (struct sockaddr *) &his_addr,
			&his_addrlen, argv) < 0)
	exit (EXIT_FAILURE);
    }
  else
    {
      his_addrlen = sizeof (his_addr);
      if (getpeername (STDIN_FILENO, (struct sockaddr *) &his_addr,
		       &his_addrlen) < 0)
	{
	  syslog (LOG_ERR, "getpeername (%s): %m", program_name);
	  exit (EXIT_FAILURE);
	}
    }

  signal (SIGHUP, sigquit);
  signal (SIGINT, sigquit);
  signal (SIGQUIT, sigquit);
  signal (SIGTERM, sigquit);
  signal (SIGPIPE, lostconn);
  signal (SIGCHLD, SIG_IGN);
  if (signal (SIGURG, myoob) == SIG_ERR)
    syslog (LOG_ERR, "signal: %m");

  /* Get info on the ctrl connection.  */
  {
    ctrl_addrlen = sizeof (ctrl_addr);
    if (getsockname (STDIN_FILENO, (struct sockaddr *) &ctrl_addr,
		     &ctrl_addrlen) < 0)
      {
	syslog (LOG_ERR, "getsockname (%s): %m", program_name);
	exit (EXIT_FAILURE);
      }
  }

#if defined IP_TOS && defined IPTOS_LOWDELAY && defined IPPROTO_IP
  /* To  minimize delays for interactive traffic.  */
  if (ctrl_addr.ss_family == AF_INET)
    {
      int tos = IPTOS_LOWDELAY;
      if (setsockopt (STDIN_FILENO, IPPROTO_IP, IP_TOS,
		    (char *) &tos, sizeof (int)) < 0)
	syslog (LOG_WARNING, "setsockopt (IP_TOS): %m");
    }
#endif

#ifdef SO_OOBINLINE
  /* Try to handle urgent data inline.  */
  {
    int on = 1;
    if (setsockopt (STDIN_FILENO, SOL_SOCKET, SO_OOBINLINE,
		    (char *) &on, sizeof (on)) < 0)
      syslog (LOG_ERR, "setsockopt: %m");
  }
#endif

#ifdef SO_KEEPALIVE
  /* Set keepalives on the socket to detect dropped connections.  */
  {
    int keepalive = 1;
    if (setsockopt (STDIN_FILENO, SOL_SOCKET, SO_KEEPALIVE,
		    (char *) &keepalive, sizeof (keepalive)) < 0)
      syslog (LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
  }
#endif

#ifdef	F_SETOWN
  if (fcntl (STDIN_FILENO, F_SETOWN, getpid ()) == -1)
    syslog (LOG_ERR, "fcntl F_SETOWN: %m");
#endif

  dolog ((struct sockaddr *) &his_addr, his_addrlen, &cred);

  /* Deal with login disable.  */
  if (display_file (PATH_NOLOGIN, 530) == 0)
    {
      reply (530, "System not available.");
      exit (EXIT_SUCCESS);
    }

  hostname = localhost ();
  if (!hostname)
    perror_reply (550, "Local resource failure: malloc");

  /* Display a Welcome message if it exists.
     N.B. a reply(220,) must follow as continuation.  */
  display_file (PATH_FTPWELCOME, 220);

  /* Tell them we're ready to roll.  */
  if (!no_version)
    reply (220, "%s FTP server (%s %s) ready.",
	   hostname, PACKAGE_NAME, PACKAGE_VERSION);
  else
    reply (220, "%s FTP server ready.", hostname);

  /* Set the jump, if we have an error parsing,
     come here and start fresh.  */
  setjmp (errcatch);

  /* Roll.  */
  for (;;)
    yyparse ();
}

static char *
curdir (void)
{
  static char *path = 0;

  free (path);
  path = xgetcwd ();
  if (!path)
    return (char *) "";
  if (path[1] != '\0')		/* special case for root dir. */
    {
      char *tmp = realloc (path, strlen (path) + 2);	/* '/' + '\0' */
      if (!tmp)
	{
	  free (path);
	  return (char *) "";
	}
      strcat (tmp, "/");
      path = tmp;
    }
  /* For guest account, skip / since it's chrooted */
  return (cred.guest ? path + 1 : path);
}

static void
sigquit (int signo)
{
  syslog (LOG_ERR, "got signal %s", strsignal (signo));
  dologout (-1);
}


static void
lostconn (int signo _GL_UNUSED_PARAMETER)
{
  if (debug)
    syslog (LOG_DEBUG, "lost connection");
  dologout (-1);
}

/* Helper function.  */
char *
sgetsave (const char *s)
{
  char *string;
  size_t len;

  if (s == NULL)
    s = "";

  len = strlen (s) + 1;
  string = malloc (len);
  if (string == NULL)
    {
      perror_reply (421, "Local resource failure: malloc");
      dologout (1);
    }
  /*  (void) strcpy (string, s); */
  memcpy (string, s, len);
  return string;
}

static void
complete_login (struct credentials *pcred)
{
  char *cwd;

  if (setegid ((gid_t) pcred->gid) < 0)
    {
      reply (550, "Can't set gid.");
      goto bad;
    }

#ifdef HAVE_INITGROUPS
  initgroups (pcred->name, pcred->gid);
#endif

  /* open wtmp before chroot */
  snprintf (ttyline, sizeof (ttyline), "ftp%d", (int) getpid ());
  logwtmp_keep_open (ttyline, pcred->name, pcred->remotehost);

  if (pcred->guest)
    {
      /* We MUST do a chdir () after the chroot. Otherwise
         the old current directory will be accessible as "."
         outside the new root!  */
      if (chroot (pcred->rootdir) < 0 || chdir (pcred->homedir) < 0)
	{
	  reply (550, "Can't set guest privileges.");
	  goto bad;
	}
    }
  else if (pcred->dochroot)
    {
      if (chroot (pcred->rootdir) < 0 || chdir (pcred->homedir) < 0)
	{
	  reply (550, "Can't change root.");
	  goto bad;
	}
    }

  if (seteuid ((uid_t) pcred->uid) < 0)
    {
      reply (550, "Can't set uid.");
      goto bad;
    }

  if (!pcred->guest && !pcred->dochroot)	/* Remaining case.  */
    {
      if (chdir (pcred->rootdir) < 0)
	{
	  if (chdir ("/") < 0)
	    {
	      reply (530, "User %s: can't change directory to %s.",
		     pcred->name, pcred->homedir);
	      goto bad;
	    }

	  lreply (230, "No directory! Logging in with home=/");
	}
    }

  cwd = xgetcwd ();
  if (cwd)
    {
      setenv ("HOME", cwd, 1);
      free (cwd);
    }

  /* Display a login message, if it exists.
     N.B. a reply(230,) must follow after this message.  */
  display_file (PATH_FTPLOGINMESG, 230);

  if (pcred->guest)
    {
      reply (230, "Guest login ok, access restrictions apply.");
#ifdef HAVE_SETPROCTITLE
      snprintf (proctitle, sizeof (proctitle), "%s: anonymous",
		pcred->remotehost);
      setproctitle ("%s", proctitle);
#endif /* HAVE_SETPROCTITLE */
      if (logging)
	syslog (LOG_INFO, "ANONYMOUS FTP LOGIN FROM %s", pcred->remotehost);
    }
  else
    {
      reply (230, "User %s logged in.", pcred->name);
#ifdef HAVE_SETPROCTITLE
      snprintf (proctitle, sizeof (proctitle),
		"%s: %s", pcred->remotehost, pcred->name);
      setproctitle ("%s", proctitle);
#endif /* HAVE_SETPROCTITLE */
      if (logging)
	syslog (LOG_INFO, "FTP LOGIN FROM %s as %s",
		pcred->remotehost, pcred->name);
    }
  umask (defumask);
  return;
bad:
  /* Forget all about it... */
  end_login (pcred);	/* Resets pcred->logged_in.  */
}

/* USER command.
   Sets global passwd pointer pw if named account exists and is acceptable;
   sets askpasswd if a PASS command is expected.  If logged in previously,
   need to reset state.  */
void
user (const char *name)
{
  int ret;

  if (cred.logged_in)
    {
      if (cred.guest || cred.dochroot)
	{
	  reply (530, "Can't change user from guest login.");
	  return;
	}
      end_login (&cred);
    }

  /* Non zero means failed.  */
  ret = auth_user (name, &cred);
  if (!rfc2577 && ret != 0)
    {
      /* If they gave us a reason.  */
      if (cred.message)
	{
	  reply (530, "%s", cred.message);
	  free (cred.message);
	  cred.message = NULL;
	}
      else
	reply (530, "User %s access denied.", name);
      if (logging)
	syslog (LOG_NOTICE, "FTP LOGIN REFUSED FROM %s, %s",
		cred.remotehost, name);
      return;
    }
  else if (rfc2577 && ret != 0)
    cred.delayed_reject = 1;
  else
    cred.delayed_reject = 0;

  /* Only messages for anonymous guests are accepted.  */
  if (rfc2577 && !cred.guest && cred.message)
    {
      free (cred.message);
      cred.message = NULL;
    }

  /* If the server is set to serve anonymous service only
     the request have to come from a guest or a chrooted.  */
  if (anon_only && !cred.guest && !cred.dochroot)
    {
      reply (530, "Sorry, only anonymous ftp allowed");
      return;
    }

  if (logging)
    {
      strncpy (curname, name, sizeof (curname) - 1);
      curname[sizeof (curname) - 1] = '\0';	/* Make sure null terminated.  */
    }

  if (cred.message)
    {
      /* Stacked PAM modules for authentication may have
       * produced a multiline message at this point.
       * The FTP protocol does not cope well with this,
       * so we transfer only the very last line, which
       * should reflect the active authentication mechanism.
       */
      char *msg = strrchr (cred.message, '\n');

      if (msg)
	msg++;		/* Step over separator.  */
      else
	msg = cred.message;

      reply (331, "%s", msg);
      free (cred.message);
      cred.message = NULL;
    }
  else
    reply (331, "Password required for %s.", name);

  askpasswd = 1;

  /* Delay before reading passwd after first failed
     attempt to slow down passwd-guessing programs.  */
  if (login_attempts)
    sleep ((unsigned) login_attempts);
}

/* Terminate login as previous user, if any, resetting state;
   used when USER command is given or login fails.  */
static void
end_login (struct credentials *pcred)
{
  char *remotehost = pcred->remotehost;
  int atype = pcred->auth_type;

  seteuid ((uid_t) 0);
  if (pcred->logged_in)
    {
      logwtmp_keep_open (ttyline, "", "");
#ifdef WITH_PAM
      pam_end_login (pcred);
#endif
    }

  free (pcred->name);
  if (pcred->passwd)
    {
      memset (pcred->passwd, 0, strlen (pcred->passwd));
      free (pcred->passwd);
    }
  free (pcred->homedir);
  free (pcred->rootdir);
  free (pcred->shell);
  if (pcred->pass)		/* Properly erase old password.  */
    {
      memset (pcred->pass, 0, strlen (pcred->pass));
      free (pcred->pass);
    }
  free (pcred->message);
  memset (pcred, 0, sizeof (*pcred));
  pcred->remotehost = remotehost;
  pcred->auth_type = atype;
  pcred->logged_in = 0;
  pcred->delayed_reject = 0;
}

void
pass (const char *passwd)
{
  if (cred.logged_in || askpasswd == 0)
    {
      reply (503, "Login with USER first.");
      return;
    }
  askpasswd = 0;

  if (!cred.guest)		/* "ftp" is the only account allowed no password.  */
    {
      /* Try to authenticate the user.  Failed if != 0.  */
      if (auth_pass (passwd, &cred) != 0 || cred.delayed_reject)
	{
	  /* Any particular reason?  */
	  if (rfc2577)
	    {
	      if (cred.message)
		{
		  free (cred.message);
		  cred.message = NULL;
		}
	      reply (530, "Login incorrect.");
	    }
	  else if (cred.message)
	    {
	      reply (530, "%s", cred.message);
	      free (cred.message);
	      cred.message = NULL;
	    }
	  else if (cred.expired & AUTH_EXPIRED_ACCT)
	    reply (530, "Account is expired.");
	  else if (cred.expired & AUTH_EXPIRED_PASS)
	    reply (530, "Password has expired.");
	  else
	    reply (530, "Login incorrect.");

	  if (logging)
	    syslog (LOG_NOTICE, "FTP LOGIN FAILED FROM %s, %s",
		    cred.remotehost, curname);
	  if (login_attempts++ >= 5)
	    {
	      syslog (LOG_NOTICE, "repeated login failures from %s",
		      cred.remotehost);
	      reply (421,
		     "Service not available, closing control connection.");
	      exit (EXIT_SUCCESS);
	    }
	  return;
	}
      if (cred.message)
	{
	  /* At least PAM might have committed additional messages.
	   * Reply code 230 is used, since at this point the client
	   * has been accepted.  */
	  lreply_multiline (230, cred.message);
	  free (cred.message);
	  cred.message = NULL;
	}
    }
  cred.logged_in = 1;		/* Everything seems to be allright.  */
  complete_login (&cred);
  if (cred.logged_in)
    login_attempts = 0;		/* This time successful.  */
  else
    ++login_attempts;
}

void
retrieve (const char *cmd, const char *name)
{
  FILE *fin, *dout;
  struct stat st;
  int (*closefunc) (FILE *);
  size_t buffer_size = BUFSIZ;	/* Dynamic buffer.  */

  if (cmd == 0)
    {
      fin = fopen (name, "r"), closefunc = fclose;
      st.st_size = 0;
    }
  else
    {
      char line[BUFSIZ];

      snprintf (line, sizeof line, cmd, name);
      name = line;
      fin = ftpd_popen (line, "r"), closefunc = ftpd_pclose;
      st.st_size = -1;
    }

  if (fin == NULL)
    {
      if (errno != 0)
	{
	  perror_reply (550, name);
	  if (cmd == 0)
	    {
	      LOGCMD ("get", name);
	    }
	}
      return;
    }
  byte_count = -1;
  if (cmd == 0 && (fstat (fileno (fin), &st) < 0 || !S_ISREG (st.st_mode)))
    {
      reply (550, "%s: not a plain file.", name);
      goto done;
    }
  else if (cmd == 0)
    buffer_size = st.st_blksize;	/* Depends on file system.  */

  if (restart_point)
    {
      if (type == TYPE_A)
	{
	  off_t i, n;
	  int c;

	  n = restart_point;
	  i = 0;
	  while (i++ < n)
	    {
	      c = getc (fin);
	      if (c == EOF)
		{
		  /* Error code 554 was introduced in RFC 1123.  */
		  reply (554,
			 "Action not taken: invalid REST value %jd for %s.",
			 restart_point, name);
		  goto done;
		}
	      if (c == '\n')
		i++;
	    }
	}
      else if (lseek (fileno (fin), restart_point, SEEK_SET) < 0)
	{
	  if (errno == EINVAL)
	    reply (554, "Action not taken: invalid REST value %jd for %s.",
		   restart_point, name);
	  else
	    perror_reply (550, name);
	  goto done;
	}
    }
  dout = dataconn (name, st.st_size, "w");
  if (dout == NULL)
    goto done;
  send_data (fin, dout, buffer_size);
  fclose (dout);
  data = -1;
  pdata = -1;
done:
  if (cmd == 0)
    LOGBYTES ("get", name, byte_count);
  (*closefunc) (fin);
}

void
store (const char *name, const char *mode, int unique)
{
  FILE *fout, *din;
  struct stat st;
  int (*closefunc) (FILE *);

  if (unique && stat (name, &st) == 0)
    {
      const char *name_unique = gunique (name);

      if (name_unique)
        name = name_unique;
      else
        {
          LOGCMD (*mode == 'w' ? "put" : "append", name);
          return;
        }
    }

  if (restart_point)
    mode = "r+";
  fout = fopen (name, mode);
  closefunc = fclose;
  if (fout == NULL || fstat (fileno (fout), &st) < 0)
    {
      perror_reply (553, name);
      LOGCMD (*mode == 'w' ? "put" : "append", name);
      return;
    }
  byte_count = -1;
  if (restart_point)
    {
      if (type == TYPE_A)
	{
	  off_t i, n;
	  int c;

	  n = restart_point;
	  i = 0;
	  while (i++ < n)
	    {
	      c = getc (fout);
	      if (c == EOF)
		{
		  /* Error code 554 was introduced in RFC 1123.  */
		  reply (554,
			 "Action not taken: invalid REST value %jd for %s.",
			 restart_point, name);
		  goto done;
		}
	      if (c == '\n')
		i++;
	    }
	  /* We must do this seek to "current" position
	     because we are changing from reading to
	     writing.  */
	  if (fseeko (fout, 0L, SEEK_CUR) < 0)
	    {
	      perror_reply (550, name);
	      goto done;
	    }
	}
      else if (lseek (fileno (fout), restart_point, SEEK_SET) < 0)
	{
	  if (errno == EINVAL)
	    reply (554, "Action not taken: invalid REST value %jd for %s.",
		   restart_point, name);
	  else
	    perror_reply (550, name);
	  goto done;
	}
    }
  din = dataconn (name, (off_t) - 1, "r");
  if (din == NULL)
    goto done;
  if (receive_data (din, fout, st.st_blksize) == 0)
    {
      if (unique)
	reply (226, "Transfer complete (unique file name:%s).", name);
      else
	reply (226, "Transfer complete.");
    }
  fclose (din);
  data = -1;
  pdata = -1;
done:
  LOGBYTES (*mode == 'w' ? "put" : "append", name, byte_count);
  (*closefunc) (fout);
}

static FILE *
getdatasock (const char *mode)
{
  int s, t, tries;

  if (data >= 0)
    return fdopen (data, mode);
  seteuid ((uid_t) 0);
  s = socket (ctrl_addr.ss_family, SOCK_STREAM, 0);
  if (s < 0)
    goto bad;

  /* Enables local reuse address.  */
  {
    int on = 1;
    if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR,
		    (char *) &on, sizeof (on)) < 0)
      goto bad;
  }

  /* Anchor socket to avoid multi-homing problems.  */
  memcpy (&data_source, &ctrl_addr, sizeof (data_source));
  data_source_len = ctrl_addrlen;

  /* Erase port number, suggesting bind() to allocate a new port.  */
  switch (data_source.ss_family)
    {
    case AF_INET6:
      ((struct sockaddr_in6 *) &data_source)->sin6_port = 0;
      break;
    case AF_INET:
      ((struct sockaddr_in *) &data_source)->sin_port = 0;
      break;
    default:
      break;	/* Do nothing; should not happen!  */
    }

  for (tries = 1;; tries++)
    {
      if (bind (s, (struct sockaddr *) &data_source, data_source_len) >= 0)
	break;
      if (errno != EADDRINUSE || tries > 10)
	goto bad;
      sleep (tries);
    }
  if (seteuid ((uid_t) cred.uid) != 0)
    _exit (EXIT_FAILURE);

#if defined IP_TOS && defined IPTOS_THROUGHPUT && defined IPPROTO_IP
  if (ctrl_addr.ss_family == AF_INET)
    {
      int on = IPTOS_THROUGHPUT;
      if (setsockopt (s, IPPROTO_IP, IP_TOS, (char *) &on, sizeof (int)) < 0)
	syslog (LOG_WARNING, "setsockopt (IP_TOS): %m");
    }
#endif

  return (fdopen (s, mode));
bad:
  /* Return the real value of errno (close may change it) */
  t = errno;
  if (seteuid ((uid_t) cred.uid) != 0)
    _exit (EXIT_FAILURE);
  close (s);
  errno = t;
  return NULL;
}

static FILE *
dataconn (const char *name, off_t size, const char *mode)
{
  char sizebuf[32];
  FILE *file;
  int retry = 0;

  file_size = size;
  byte_count = 0;
  if (size != (off_t) - 1)
    snprintf (sizebuf, sizeof (sizebuf), " (%s bytes)", off_to_str (size));
  else
    *sizebuf = '\0';
  if (pdata >= 0)
    {
      struct sockaddr_storage from;
      int s;
      socklen_t fromlen = sizeof (from);

      signal (SIGALRM, toolong);
      alarm ((unsigned) timeout);
      s = accept (pdata, (struct sockaddr *) &from, &fromlen);
      alarm (0);
      if (s < 0)
	{
	  reply (425, "Can't open data connection.");
	  close (pdata);
	  pdata = -1;
	  return NULL;
	}
      close (pdata);
      pdata = s;
#if defined IP_TOS && defined IPTOS_THROUGHPUT && defined IPPROTO_IP
      /* Optimize throughput.  */
      if (from.ss_family == AF_INET)
	{
	  int tos = IPTOS_THROUGHPUT;
	  setsockopt (s, IPPROTO_IP, IP_TOS, (char *) &tos, sizeof (int));
	}
#endif
#ifdef SO_KEEPALIVE
      /* Set keepalives on the socket to detect dropped conns.  */
      {
	int keepalive = 1;
	setsockopt (s, SOL_SOCKET, SO_KEEPALIVE,
		    (char *) &keepalive, sizeof (int));
      }
#endif
      reply (150, "Opening %s mode data connection for '%s'%s.",
	     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
      return fdopen (pdata, mode);
    }
  if (data >= 0)
    {
      reply (125, "Using existing data connection for '%s'%s.",
	     name, sizebuf);
      usedefault = 1;
      return fdopen (data, mode);
    }
  if (usedefault)
    {
      memcpy (&data_dest, &his_addr, sizeof (data_dest));
      data_dest_len = his_addrlen;
    }
  usedefault = 1;
  file = getdatasock (mode);
  if (file == NULL)
    {
      int oerrno = errno;
      (void) getnameinfo ((struct sockaddr *) &data_source, data_source_len,
			  addrstr, sizeof (addrstr),
			  portstr, sizeof (portstr),
			  NI_NUMERICSERV);
      reply (425, "Can't create data socket (%s,%s): %s.",
	     addrstr, portstr, strerror (oerrno));
      return NULL;
    }
  data = fileno (file);
  while (connect (data, (struct sockaddr *) &data_dest, data_dest_len) < 0)
    {
      if (errno == EADDRINUSE && retry < swaitmax)
	{
	  sleep ((unsigned) swaitint);
	  retry += swaitint;
	  continue;
	}
      perror_reply (425, "Can't build data connection");
      fclose (file);
      data = -1;
      return NULL;
    }
  reply (150, "Opening %s mode data connection for '%s'%s.",
	 type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
  return file;
}

#define IU_MMAP_SIZE 0x800000	/* 8 MByte */

/* Tranfer the contents of "instr" to "outstr" peer using the appropriate
   encapsulation of the data subject * to Mode, Structure, and Type.

   NB: Form isn't handled.  */
static void
send_data (FILE * instr, FILE * outstr, off_t blksize)
{
  int c, cnt, filefd, netfd;
  char *buf = MAP_FAILED, *bp;
  off_t curpos;
  off_t len, filesize;

  transflag++;
  if (setjmp (urgcatch))
    {
      transflag = 0;
      return;
    }

  netfd = fileno (outstr);
  filefd = fileno (instr);
#ifdef HAVE_MMAP
  /* Last argument in mmap() must be page aligned,
   * at least for Solaris and Linux, so use mmap()
   * only with null offset retrievals.
   */
  if (file_size > 0 && file_size < IU_MMAP_SIZE && restart_point == 0)
    {
      curpos = lseek (filefd, 0, SEEK_CUR);
      if (debug)
	syslog (LOG_DEBUG, "Position is %jd. Attempting mmap call.",
		curpos);
      if (curpos >= 0)
	{
	  filesize = file_size - curpos;
	  buf = mmap (0, filesize, PROT_READ, MAP_SHARED, filefd, curpos);
	}
    }
#endif

  switch (type)
    {

    case TYPE_A:
#ifdef HAVE_MMAP
      if (file_size > 0 && curpos >= 0 && buf != MAP_FAILED)
	{
	  if (debug)
	    syslog (LOG_DEBUG, "Reading file as ascii in mmap mode.");
	  len = 0;
	  while (len < filesize)
	    {
	      byte_count++;
	      if (buf[len] == '\n')
		{
		  if (ferror (outstr))
		    break;
		  putc ('\r', outstr);
		}
	      putc (buf[len], outstr);
	      len++;
	    }
	  fflush (outstr);
	  transflag = 0;
	  munmap (buf, filesize);
	  if (ferror (outstr))
	    goto data_err;
	  reply (226, "Transfer complete.");
	  return;
	}
#endif
      if (debug)
	syslog (LOG_DEBUG, "Reading file as ascii in byte mode.");
      while ((c = getc (instr)) != EOF)
	{
	  byte_count++;
	  if (c == '\n')
	    {
	      if (ferror (outstr))
		goto data_err;
	      putc ('\r', outstr);
	    }
	  putc (c, outstr);
	}
      fflush (outstr);
      transflag = 0;
      if (ferror (instr))
	goto file_err;
      if (ferror (outstr))
	goto data_err;
      reply (226, "Transfer complete.");
      return;

    case TYPE_I:
    case TYPE_L:
#ifdef HAVE_MMAP
      if (file_size > 0 && curpos >= 0 && buf != MAP_FAILED)
	{
	  if (debug)
	    syslog (LOG_DEBUG, "Reading file as image in mmap mode.");
	  bp = buf;
	  len = filesize;
	  do
	    {
	      cnt = write (netfd, bp, len);
	      len -= cnt;
	      bp += cnt;
	      if (cnt > 0)
		byte_count += cnt;
	    }
	  while (cnt > 0 && len > 0);
	  transflag = 0;
	  munmap (buf, (size_t) filesize);
	  if (cnt < 0)
	    goto data_err;
	  reply (226, "Transfer complete.");
	  return;
	}
#endif
      if (debug)
	{
	  syslog (LOG_DEBUG, "Reading file as image in block mode.");
	  curpos = lseek (filefd, 0, SEEK_CUR);
	  if (curpos < 0)
	    syslog (LOG_DEBUG, "Input file: %m");
	  else
	    syslog (LOG_DEBUG, "Starting at position %jd.", curpos);
	}

      buf = malloc ((u_int) blksize);
      if (buf == NULL)
	{
	  transflag = 0;
	  perror_reply (451, "Local resource failure: malloc");
	  return;
	}
      while ((cnt = read (filefd, buf, (u_int) blksize)) > 0 &&
	     write (netfd, buf, cnt) == cnt)
	byte_count += cnt;

      transflag = 0;
      free (buf);
      if (cnt != 0)
	{
	  if (cnt < 0)
	    goto file_err;
	  goto data_err;
	}
      reply (226, "Transfer complete.");
      return;
    default:
      transflag = 0;
      reply (550, "Unimplemented TYPE %d in send_data", type);
      return;
    }

data_err:
  transflag = 0;
  perror_reply (426, "Data connection");
  return;

file_err:
  transflag = 0;
  perror_reply (551, "Error on input file");
}

/* Transfer data from peer to "outstr" using the appropriate encapulation of
   the data subject to Mode, Structure, and Type.

   N.B.: Form isn't handled.  */
static int
receive_data (FILE * instr, FILE * outstr, off_t blksize)
{
  int c;
  int cnt, bare_lfs = 0;
  char *buf;

  transflag++;
  if (setjmp (urgcatch))
    {
      transflag = 0;
      return -1;
    }
  switch (type)
    {
    case TYPE_I:
    case TYPE_L:
      buf = malloc ((u_int) blksize);
      if (buf == NULL)
	{
	  transflag = 0;
	  perror_reply (451, "Local resource failure: malloc");
	  return -1;
	}

      while ((cnt = read (fileno (instr), buf, blksize)) > 0)
	{
	  if (write (fileno (outstr), buf, cnt) != cnt)
	    {
	      free (buf);
	      goto file_err;
	    }
	  byte_count += cnt;
	}
      free (buf);
      if (cnt < 0)
	goto data_err;
      transflag = 0;
      return 0;

    case TYPE_E:
      reply (553, "TYPE E not implemented.");
      transflag = 0;
      return -1;

    case TYPE_A:
      while ((c = getc (instr)) != EOF)
	{
	  byte_count++;
	  if (c == '\n')
	    bare_lfs++;
	  while (c == '\r')
	    {
	      if (ferror (outstr))
		goto data_err;
	      c = getc (instr);
	      if (c != '\n')
		{
		  putc ('\r', outstr);
		  if (c == '\0' || c == EOF)
		    goto contin2;
		}
	    }
	  putc (c, outstr);
	contin2:;
	}
      fflush (outstr);
      if (ferror (instr))
	goto data_err;
      if (ferror (outstr))
	goto file_err;
      transflag = 0;
      if (bare_lfs)
	{
	  lreply (226, "WARNING! %d bare linefeeds received in ASCII mode",
		  bare_lfs);
	  printf ("   File may not have transferred correctly.\r\n");
	}
      return (0);
    default:
      reply (550, "Unimplemented TYPE %d in receive_data", type);
      transflag = 0;
      return -1;
    }

data_err:
  transflag = 0;
  perror_reply (426, "Data Connection");
  return -1;

file_err:
  transflag = 0;
  perror_reply (452, "Error writing file");
  return -1;
}

void
statfilecmd (const char *filename)
{
  FILE *fin;
  int c;
  char line[LINE_MAX];

  snprintf (line, sizeof (line), "/bin/ls -lgA %s", filename);
  fin = ftpd_popen (line, "r");
  lreply (211, "status of %s:", filename);
  while ((c = getc (fin)) != EOF)
    {
      if (c == '\n')
	{
	  if (ferror (stdout))
	    {
	      perror_reply (421, "control connection");
	      ftpd_pclose (fin);
	      dologout (1);
	    }
	  if (ferror (fin))
	    {
	      perror_reply (551, filename);
	      ftpd_pclose (fin);
	      return;
	    }
	  putc ('\r', stdout);
	}
      putc (c, stdout);
    }
  ftpd_pclose (fin);
  reply (211, "End of Status");
}

void
statcmd (void)
{
  struct sockaddr_storage *sin;
  unsigned char *a, *p;

  lreply (211, "%s FTP server status:", hostname);
  if (!no_version)
    printf ("     ftpd (%s) %s\r\n", PACKAGE_NAME, PACKAGE_VERSION);
  printf ("     Connected to %s", cred.remotehost);
  (void) getnameinfo ((struct sockaddr *) &his_addr, his_addrlen,
		      addrstr, sizeof (addrstr), NULL, 0, NI_NUMERICHOST);
  if (!isdigit (cred.remotehost[0]))
    printf (" (%s)", addrstr);
  printf ("\r\n");
  printf ("     Session timeout is %d seconds\r\n", timeout);
  if (cred.logged_in)
    {
      if (cred.guest)
	printf ("     Logged in anonymously\r\n");
      else
	printf ("     Logged in as %s\r\n", cred.name);
    }
  else if (askpasswd)
    printf ("     Waiting for password\r\n");
  else
    printf ("     Waiting for user name\r\n");
  printf ("     TYPE: %s", typenames[type]);
  if (type == TYPE_A || type == TYPE_E)
    printf (", FORM: %s", formnames[form]);
  if (type == TYPE_L)
#ifdef CHAR_BIT
    printf (" %d", CHAR_BIT);
#else
# if NBBY == 8
    printf (" %d", NBBY);
# else
    printf (" %d", bytesize);	/* need definition! */
# endif
#endif
  printf ("; STRUcture: %s; transfer MODE: %s\r\n",
	  strunames[stru], modenames[stru_mode]);
  if (data != -1)
    printf ("     Data connection open\r\n");
  else if (pdata != -1)
    {
      printf ("     in Passive mode");
      sin = &pasv_addr;
      goto printaddr;
    }
  else if (usedefault == 0)
    {
      printf ("     PORT");
      sin = &data_dest;
    printaddr:
      a = (unsigned char *) & ((struct sockaddr_in *) sin)->sin_addr;
      p = (unsigned char *) & ((struct sockaddr_in *) sin)->sin_port;
#define UC(b) (((int) b) & 0xff)
      printf (" (%d,%d,%d,%d,%d,%d)\r\n", UC (a[0]),
	      UC (a[1]), UC (a[2]), UC (a[3]), UC (p[0]), UC (p[1]));
#undef UC
    }
  else
    printf ("     No data connection\r\n");
  reply (211, "End of status");
}

void
fatal (const char *s)
{
  reply (451, "Error in server: %s\n", s);
  reply (221, "Closing connection due to server error.");
  dologout (0);
}

void
reply (int n, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  printf ("%d ", n);
  vprintf (fmt, ap);
  va_end (ap);
  printf ("\r\n");
  fflush (stdout);
  if (debug)
    {
      syslog (LOG_DEBUG, "<--- %d ", n);
      va_start (ap, fmt);
      vsyslog (LOG_DEBUG, fmt, ap);
      va_end (ap);
    }
}

void
lreply (int n, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  printf ("%d- ", n);
  vprintf (fmt, ap);
  va_end (ap);
  printf ("\r\n");
  fflush (stdout);
  if (debug)
    {
      syslog (LOG_DEBUG, "<--- %d- ", n);
      va_start (ap, fmt);
      vsyslog (LOG_DEBUG, fmt, ap);
      va_end (ap);
    }
}

/* Send a possibly multiline reply as individual
 * lines of message with identical status code.
 * No format string input!
 */
void
lreply_multiline (int n, const char *text)
{
  char *line;

  line = strdup (text);
  if (line == NULL)
    return;
  else
    {
      int stop = 0;
      char *p1 = line, *p2;

      do
	{
	  p2 = strchrnul (p1, '\n');
	  stop = (*p2 == '\0');		/* End of input string?  */
	  *p2 = '\0';
	  printf ("%d- ", n);
	  printf ("%s\r\n", p1);
	  if (debug)
	    {
	      syslog (LOG_DEBUG, "<--- %d- ", n);
	      syslog (LOG_DEBUG, "%s", p1);
	    }
	  p1 = ++p2;			/* P1 is used within bounds.  */
	}
      while (!stop);
      free (line);
    }
}

static void
ack (const char *s)
{
  reply (250, "%s command successful.", s);
}

void
nack (const char *s)
{
  reply (502, "%s command not implemented.", s);
}

void
delete (const char *name)
{
  struct stat st;

  LOGCMD ("delete", name);
  if (stat (name, &st) < 0)
    {
      perror_reply (550, name);
      return;
    }
  if (S_ISDIR (st.st_mode))
    {
      if (rmdir (name) < 0)
	{
	  perror_reply (550, name);
	  return;
	}
      goto done;
    }
  if (unlink (name) < 0)
    {
      perror_reply (550, name);
      return;
    }
done:
  ack ("DELE");
}

void
cwd (const char *path)
{
  if (chdir (path) < 0)
    perror_reply (550, path);
  else
    ack ("CWD");
}

void
makedir (const char *name)
{
  LOGCMD ("mkdir", name);
  if (mkdir (name, 0777) < 0)
    perror_reply (550, name);
  else if (name[0] == '/')
    reply (257, "\"%s\" new directory created.", name);
  else
    {
      /* We have to figure out what our current directory is so that we can
         give an absolute name in the reply.  */
      char *current = xgetcwd ();
      if (current)
	{
	  if (current[1] == '\0')
	    current[0] = '\0';
	  reply (257, "\"%s/%s\" new directory created.", current, name);
	  free (current);
	}
      else
	reply (257, "(unknown absolute name) new directory created.");
    }
}

void
removedir (const char *name)
{
  LOGCMD ("rmdir", name);
  if (rmdir (name) < 0)
    perror_reply (550, name);
  else
    ack ("RMD");
}

void
pwd (void)
{
  char *path = xgetcwd ();

  if (path)
    {
      reply (257, "\"%s\" is current directory.", path);
      free (path);
    }
  else
    reply (550, "%s.", strerror (errno));
}

char *
renamefrom (const char *name)
{
  struct stat st;

  if (stat (name, &st) < 0)
    {
      perror_reply (550, name);
      return ((char *) 0);
    }
  reply (350, "File exists, ready for destination name");
  return (char *) (name);
}

void
renamecmd (const char *from, const char *to)
{
  LOGCMD2 ("rename", from, to);
  if (rename (from, to) < 0)
    perror_reply (550, "rename");
  else
    ack ("RNTO");
}

static void
dolog (struct sockaddr *sa, socklen_t salen, struct credentials *pcred)
{
  (void) getnameinfo (sa, salen, addrstr, sizeof (addrstr), NULL, 0, 0);

  free (pcred->remotehost);
  pcred->remotehost = sgetsave (addrstr);

#ifdef HAVE_SETPROCTITLE
  snprintf (proctitle, sizeof (proctitle), "%s: connected",
	    pcred->remotehost);
  setproctitle ("%s", proctitle);
#endif /* HAVE_SETPROCTITLE */

  if (logging)
    syslog (LOG_INFO, "connection from %s", pcred->remotehost);
}

/*  Record logout in wtmp file
    and exit with supplied status.  */
void
dologout (int status)
{
  /* Race condition with SIGURG: If SIGURG is received
     here, it will jump back has root in the main loop.
     David Greenman:dg@root.com.  */
  transflag = 0;
  end_login (&cred);

  /* Beware of flushing buffers after a SIGPIPE.  */
  _exit (status);
}

static void
myoob (int signo _GL_UNUSED_PARAMETER)
{
  char *cp;

  /* only process if transfer occurring */
  if (!transflag)
    return;
  cp = tmpline;
  if (telnet_fgets (cp, 7, stdin) == NULL)
    {
      reply (221, "You could at least say goodbye.");
      dologout (0);
    }
  upper (cp);
  if (strcmp (cp, "ABOR\r\n") == 0)
    {
      tmpline[0] = '\0';
      reply (426, "Transfer aborted. Data connection closed.");
      reply (226, "Abort successful");
      longjmp (urgcatch, 1);
    }
  if (strcmp (cp, "STAT\r\n") == 0)
    {
      if (file_size != (off_t) - 1)
	reply (213, "Status: %s of %s bytes transferred",
	       off_to_str (byte_count), off_to_str (file_size));
      else
	reply (213, "Status: %s bytes transferred", off_to_str (byte_count));
    }
}

/* Note: a response of 425 is not mentioned as a possible response to
   the PASV command in RFC959. However, it has been blessed as
   a legitimate response by Jon Postel in a telephone conversation
   with Rick Adams on 25 Jan 89.  */
void
passive (int epsv, int af)
{
  char *p, *a;
  int try_af;

  /* EPSV might ask for a particular address family.  */
  if (epsv == PASSIVE_EPSV && af > 0)
    try_af = af;
  else
    try_af = ctrl_addr.ss_family;

  pdata = socket (try_af, SOCK_STREAM, 0);
  if (pdata < 0)
    {
      perror_reply (425, "Can't open passive connection");
      return;
    }
  memcpy (&pasv_addr, &ctrl_addr, sizeof (pasv_addr));
  pasv_addrlen = ctrl_addrlen;

  /* Erase the port number.  */
  if (pasv_addr.ss_family == AF_INET6)
    ((struct sockaddr_in6 *) &pasv_addr)->sin6_port = 0;
  else	/* !AF_INET6 */
    ((struct sockaddr_in *) &pasv_addr)->sin_port = 0;

  seteuid ((uid_t) 0);
  if (bind (pdata, (struct sockaddr *) &pasv_addr, pasv_addrlen) < 0)
    {
      if (seteuid ((uid_t) cred.uid))
	_exit (EXIT_FAILURE);
      goto pasv_error;
    }
  if (seteuid ((uid_t) cred.uid))
    _exit (EXIT_FAILURE);
  pasv_addrlen = sizeof (pasv_addr);
  if (getsockname (pdata, (struct sockaddr *) &pasv_addr, &pasv_addrlen) < 0)
    goto pasv_error;
  if (listen (pdata, 1) < 0)
    goto pasv_error;

  if (epsv == PASSIVE_EPSV)
    {
      /* EPSV for IPv4 and IPv6.  */
      reply (229, "Entering Extended Passive Mode (|||%u|)",
	     ntohs((pasv_addr.ss_family == AF_INET)
		    ? ((struct sockaddr_in *) &pasv_addr)->sin_port
		    : ((struct sockaddr_in6 *) &pasv_addr)->sin6_port));
      return;
    }
  else /* !EPSV */
    {
      /* PASV for IPv4, or LPSV for IPv4 or IPv6.
       *
       * Some systems, like OpenSolaris, prefer to return
       * an IPv4-mapped-IPv6 address, which must be processed
       * for printout.  */

#define UC(b) (((int) b) & 0xff)

      if (pasv_addr.ss_family == AF_INET6
	  && IN6_IS_ADDR_V4MAPPED (&((struct sockaddr_in6 *) &pasv_addr)->sin6_addr))
	{
	  a = (char *) &((struct sockaddr_in6 *) &pasv_addr)->sin6_addr;
	  a += 3 * sizeof (struct in_addr);	/* Skip padding up to IPv4 content.  */
	  p = (char *) &((struct sockaddr_in6 *) &pasv_addr)->sin6_port;
	}
      else if (pasv_addr.ss_family == AF_INET6)
	{
	  /* LPSV for IPv6, not mapped. */
	  a = (char *) &((struct sockaddr_in6 *) &pasv_addr)->sin6_addr;
	  p = (char *) &((struct sockaddr_in6 *) &pasv_addr)->sin6_port;

	  reply (228, "Entering Long Passive Mode "
		 "(6,16,%d,%d,%d,%d,%d,%d,%d,%d"	/* a[0..7] */
		 ",%d,%d,%d,%d,%d,%d,%d,%d"	/* a[8..15] */
		 ",2,%d,%d)",	/* p0, p1 */
		 UC (a[0]), UC (a[1]), UC (a[2]), UC (a[3]),
		 UC (a[4]), UC (a[5]), UC (a[6]), UC (a[7]),
		 UC (a[8]), UC (a[9]), UC (a[10]), UC (a[11]),
		 UC (a[12]), UC (a[13]), UC (a[14]), UC (a[15]),
		 UC (p[0]), UC (p[1]));
	  return;
	}
      else
	{
	  a = (char *) &((struct sockaddr_in *) &pasv_addr)->sin_addr;
	  p = (char *) &((struct sockaddr_in *) &pasv_addr)->sin_port;
	}

      if (epsv == PASSIVE_LPSV)
	reply (228, "Entering Long Passive Mode "
	       "(4,4,%d,%d,%d,%d,2,%d,%d)",
	       UC (a[0]), UC (a[1]), UC (a[2]), UC (a[3]),
	       UC (p[0]), UC (p[1]));
      else
	reply (227, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
	       UC (a[0]), UC (a[1]), UC (a[2]), UC (a[3]),
	       UC (p[0]), UC (p[1]));
      return;
    }

pasv_error:
  close (pdata);
  pdata = -1;
  perror_reply (425, "Can't open passive connection");
  return;
}

/* Generate unique name for file with basename "local".
   The file named "local" is already known to exist.
   Generates failure reply on error.  */
static char *
gunique (const char *local)
{
  static char *string = 0;
  struct stat st;
  int count;
  char *cp;

  cp = strrchr (local, '/');
  if (cp)
    *cp = '\0';
  if (stat (cp ? local : ".", &st) < 0)
    {
      perror_reply (553, cp ? local : ".");
      return ((char *) 0);
    }
  if (cp)
    *cp = '/';

  free (string);
  string = malloc (strlen (local) + 5);	/* '.' + DIG + DIG + '\0' */
  if (string)
    {
      strcpy (string, local);
      cp = string + strlen (string);
      *cp++ = '.';
      for (count = 1; count < 100; count++)
	{
	  sprintf (cp, "%d", count);
	  if (stat (string, &st) < 0)
	    return string;
	}
    }

  reply (452, "Unique file name cannot be created.");
  return NULL;
}

/*
 * Format and send reply containing system error number.
 */
void
perror_reply (int code, const char *string)
{
  reply (code, "%s: %s.", string, strerror (errno));
}

static char *onefile[] = {
  "",
  0
};

void
send_file_list (const char *whichf)
{
  struct stat st;
  DIR *dirp = NULL;
  struct dirent *dir;
  FILE *dout = NULL;
  char **dirlist, *dirname;
  int simple = 0;
  int freeglob = 0;
  glob_t gl;
  char *p = NULL;

  if (strpbrk (whichf, "~{[*?") != NULL)
    {
      int flags = GLOB_NOCHECK;

#ifdef GLOB_BRACE
      flags |= GLOB_BRACE;
#endif
#ifdef GLOB_QUOTE
      flags |= GLOB_QUOTE;
#endif
#ifdef GLOB_TILDE
      flags |= GLOB_TILDE;
#endif

      memset (&gl, 0, sizeof (gl));
      freeglob = 1;
      if (glob (whichf, flags, 0, &gl))
	{
	  reply (550, "not found");
	  goto out;
	}
      else if (gl.gl_pathc == 0)
	{
	  errno = ENOENT;
	  perror_reply (550, whichf);
	  goto out;
	}
      dirlist = gl.gl_pathv;
    }
  else
    {
      p = strdup (whichf);
      onefile[0] = p;
      dirlist = onefile;
      simple = 1;
    }

  if (setjmp (urgcatch))
    {
      transflag = 0;
      goto out;
    }
  while ((dirname = *dirlist++))
    {
      if (stat (dirname, &st) < 0)
	{
	  /* If user typed "ls -l", etc, and the client
	     used NLST, do what the user meant.  */
	  if (dirname[0] == '-' && *dirlist == NULL && transflag == 0)
	    {
	      retrieve ("/bin/ls %s", dirname);
	      goto out;
	    }
	  perror_reply (550, whichf);
	  if (dout != NULL)
	    {
	      fclose (dout);
	      transflag = 0;
	      data = -1;
	      pdata = -1;
	    }
	  goto out;
	}

      if (S_ISREG (st.st_mode))
	{
	  if (dout == NULL)
	    {
	      dout = dataconn ("file list", (off_t) - 1, "w");
	      if (dout == NULL)
		goto out;
	      transflag++;
	    }
	  fprintf (dout, "%s%s\n", dirname, type == TYPE_A ? "\r" : "");
	  byte_count += strlen (dirname) + 1;
	  continue;
	}
      else if (!S_ISDIR (st.st_mode))
	continue;

      dirp = opendir (dirname);
      if (dirp == NULL)
	continue;

      while ((dir = readdir (dirp)) != NULL)
	{
	  char *nbuf;

	  if (dir->d_name[0] == '.' && dir->d_name[1] == '\0')
	    continue;
	  if (dir->d_name[0] == '.' && dir->d_name[1] == '.' &&
	      dir->d_name[2] == '\0')
	    continue;

	  nbuf = alloca (strlen (dirname) + 1 + strlen (dir->d_name) + 1);
	  sprintf (nbuf, "%s/%s", dirname, dir->d_name);

	  /* We have to do a stat to insure it's
	     not a directory or special file.  */
	  if (simple || (stat (nbuf, &st) == 0 && S_ISREG (st.st_mode)))
	    {
	      if (dout == NULL)
		{
		  dout = dataconn ("file list", (off_t) - 1, "w");
		  if (dout == NULL)
		    goto out;
		  transflag++;
		}
	      if (nbuf[0] == '.' && nbuf[1] == '/')
		fprintf (dout, "%s%s\n", &nbuf[2],
			 type == TYPE_A ? "\r" : "");
	      else
		fprintf (dout, "%s%s\n", nbuf, type == TYPE_A ? "\r" : "");
	      byte_count += strlen (nbuf) + 1;
	    }
	}
      closedir (dirp);
    }

  if (dout == NULL)
    reply (550, "No files found.");
  else if (ferror (dout) != 0)
    perror_reply (550, "Data connection");
  else
    reply (226, "Transfer complete.");

  transflag = 0;
  if (dout != NULL)
    fclose (dout);
  data = -1;
  pdata = -1;
out:
  free (p);
  if (freeglob)
    {
      freeglob = 0;
      globfree (&gl);
    }
}
