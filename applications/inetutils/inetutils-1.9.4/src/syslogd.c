/* syslogd - log system messages
  Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
  2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
  Foundation, Inc.

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
 * Copyright (c) 1983, 1988, 1993, 1994
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
 *  syslogd -- log system messages
 *
 * This program implements a system log. It takes a series of lines.
 * Each line may have a priority, signified as "<n>" as the first
 * characters of the line.  If this is not present, a default priority
 * is used.
 *
 * To kill syslogd, send a signal 15 (terminate).  A signal 1 (hup)
 * will cause it to reread its configuration file.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximum line length that can be handled.
 * DEFUPRI -- the default priority for user messages
 * DEFSPRI -- the default priority for kernel messages
 *
 * Author: Eric Allman
 * extensive changes by Ralph Campbell
 * more extensive changes by Eric Allman (again)
 */

#include <config.h>

#define IOVCNT          6	/* size of the iovec array */
#define MAXLINE		1024	/* Maximum line length.  */
#define MAXSVLINE	240	/* Maximum saved line length.  */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define DEFSPRI		(LOG_KERN|LOG_CRIT)
#define TIMERINTVL	30	/* Interval for checking flush, mark.  */
#define TTYMSGTIME      10	/* Time out passed to ttymsg.  */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/time.h>
#include <time.h>
#include <sys/resource.h>
#include <poll.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <argp.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include <stdarg.h>

#define SYSLOG_NAMES
#include <syslog.h>
#ifndef HAVE_SYSLOG_INTERNAL
# include "logprio.h"
#endif

#ifndef LOG_MAKEPRI
#  define LOG_MAKEPRI(fac, p)	((fac) | (p))
#endif

#include <error.h>
#include <progname.h>
#include <libinetutils.h>
#include <readutmp.h>		/* May define UTMP_NAME_FUNCTION.  */
#include "unused-parameter.h"
#include "xalloc.h"

/* A mask of all facilities mentioned explicitly in the configuration file
 *
 * This is used to support a virtual facility "**" that covers all the rest,
 * so that messages to unexpected facilities won't be lost when "*" is
 * not logged to a file.
 */
int facilities_seen;

char *selector;			/* Program origin to select.  */

const char *ConfFile = PATH_LOGCONF;	/* Default Configuration file.  */
const char *ConfDir = PATH_LOGCONFD;	/* Default Configuration directory.  */
const char *PidFile = PATH_LOGPID;	/* Default path to tuck pid.  */
char ctty[] = PATH_CONSOLE;	/* Default console to send message info.  */

static int dbg_output;		/* If true, print debug output in debug mode.  */
static int restart;		/* If 1, indicates SIGHUP was dropped.  */

/* Unix socket family to listen.  */
struct funix
{
  const char *name;
  int fd;
} *funix;

size_t nfunix;			/* Number of unix sockets in the funix array.  */

/*
 * Flags to logmsg().
 */

#define IGN_CONS	0x001	/* Don't print on console.  */
#define SYNC_FILE	0x002	/* Do fsync on file after printing.  */
#define ADDDATE		0x004	/* Add a date to the message.  */
#define MARK		0x008	/* This message is a mark.  */

/* This structure represents the files that will have log copies
   printed.  */

struct filed
{
  struct filed *f_next;		/* Next in linked list.  */
  short f_type;			/* Entry type, see below.  */
  short f_file;			/* File descriptor.  */
  time_t f_time;		/* Time this was last written.  */
  unsigned char f_pmask[LOG_NFACILITIES + 1];	/* Priority mask.  */
  union
  {
    struct
    {
      int f_nusers;
      char **f_unames;
    } f_user;			/* Send a message to a user.  */
    struct
    {
      char *f_hname;
      struct sockaddr_storage f_addr;
      socklen_t f_addrlen;
    } f_forw;			/* Forwarding address.  */
    char *f_fname;		/* Name use for Files|Pipes|TTYs.  */
  } f_un;
  char f_prevline[MAXSVLINE];	/* Last message logged.  */
  char f_lasttime[16];		/* Time of last occurrence.  */
  char *f_prevhost;		/* Host from which recd.  */
  char *f_progname;		/* Submitting program.  */
  int f_prognlen;		/* Length of the same.  */
  int f_prevpri;		/* Pri of f_prevline.  */
  int f_prevlen;		/* Length of f_prevline.  */
  int f_prevcount;		/* Repetition cnt of prevline.  */
  size_t f_repeatcount;		/* Number of "repeated" msgs.  */
  int f_flags;			/* Additional flags see below.  */
};

struct filed *Files;		/* Linked list of files to log to.  */
struct filed consfile;		/* Console `file'.  */

/* Values for f_type.  */
#define F_UNUSED	0	/* Unused entry.  */
#define F_FILE		1	/* Regular file.  */
#define F_TTY		2	/* Terminal.  */
#define F_CONSOLE	3	/* Console terminal.  */
#define F_FORW		4	/* Remote machine.  */
#define F_USERS		5	/* List of users.  */
#define F_WALL		6	/* Everyone logged on.  */
#define F_FORW_SUSP	7	/* Suspended host forwarding.  */
#define F_FORW_UNKN	8	/* Unknown host forwarding.  */
#define F_PIPE		9	/* Named pipe.  */

const char *TypeNames[] = {
  "UNUSED",
  "FILE",
  "TTY",
  "CONSOLE",
  "FORW",
  "USERS",
  "WALL",
  "FORW(SUSPENDED)",
  "FORW(UNKNOWN)",
  "PIPE"
};

/* Flags in filed.f_flags.  */
#define OMIT_SYNC	0x001	/* Omit fsync after printing.  */

/* Constants for the F_FORW_UNKN retry feature.  */
#define INET_SUSPEND_TIME 180	/* Number of seconds between attempts.  */
#define INET_RETRY_MAX	10	/* Number of times to try gethostbyname().  */

/* Intervals at which we flush out "message repeated" messages, in
 seconds after previous message is logged.  After each flush, we move
 to the next interval until we reach the largest.  */

int repeatinterval[] = { 30, 60 };	/* Number of seconds before flush.  */

#define MAXREPEAT ((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1)
#define REPEATTIME(f)	((f)->f_time + repeatinterval[(f)->f_repeatcount])
#define BACKOFF(f)	{ if (++(f)->f_repeatcount > MAXREPEAT) \
				 (f)->f_repeatcount = MAXREPEAT; \
			}

/* Delimiter in arguments to command line options `-s' and `-l'.  */
#define LIST_DELIMITER	':'

extern int waitdaemon (int nochdir, int noclose, int maxwait);

void cfline (const char *, struct filed *);
const char *cvthname (struct sockaddr *, socklen_t);
int decode (const char *, CODE *);
void die (int);
void doexit (int);
void domark (int);
void find_inet_port (const char *);
void fprintlog (struct filed *, const char *, int, const char *);
static int load_conffile (const char *, struct filed **);
static int load_confdir (const char *, struct filed **);
void init (int);
void logerror (const char *);
void logmsg (int, const char *, const char *, int);
void printline (const char *, const char *);
void printsys (const char *);
char *ttymsg (struct iovec *, int, char *, int);
void wallmsg (struct filed *, struct iovec *);
char **crunch_list (char **oldlist, char *list);
char *textpri (int pri);
void dbg_toggle (int);
static void dbg_printf (const char *, ...);
void trigger_restart (int);
static void add_funix (const char *path);
static int create_unix_socket (const char *path);
static void create_inet_socket (int af, int fd46[2]);

char *LocalHostName;		/* Our hostname.  */
char *LocalDomain;		/* Our local domain name.  */
char *BindAddress = NULL;	/* Binding address for INET listeners.
				 * The default is a wildcard address.  */
char *BindPort = NULL;		/* Optional non-standard port, instead
				 * of the usual 514/udp.  */
#ifdef IPPORT_SYSLOG
char portstr[8];		/* Fallback port number.  */
#endif
char addrstr[INET6_ADDRSTRLEN];	/* Common address presentation.  */
char addrname[NI_MAXHOST];	/* Common name lookup.  */
int usefamily = AF_INET;	/* Address family for INET services.
				 * Each of the values `AF_INET' and `AF_INET6'
				 * produces a single-stacked server.  */
int finet[2] = {-1, -1};	/* Internet datagram socket fd.  */
#define IU_FD_IP4	0	/* Indices for the address families.  */
#define IU_FD_IP6	1
int fklog = -1;			/* Kernel log device fd.  */
char *LogPortText = NULL;	/* Service/port for INET connections.  */
char *LogForwardPort = NULL;	/* Target port for message forwarding.  */
int Initialized;		/* True when we are initialized. */
int MarkInterval = 20 * 60;	/* Interval between marks in seconds.  */
int MarkSeq;			/* Mark sequence number.  */

int Debug;			/* True if in debug mode.  */
int AcceptRemote;		/* Receive messages that come via UDP.  */
char **StripDomains;		/* Domains to be stripped before logging.  */
char **LocalHosts;		/* Hosts to be logged by their hostname.  */
int NoDetach;			/* Don't run in background and detach
				   from ctty. */
int NoHops = 1;			/* Bounce syslog messages for other hosts.  */
int NoKLog;			/* Don't attempt to log kernel device.  */
int NoUnixAF;			/* Don't listen to unix sockets. */
int NoForward;			/* Don't forward messages.  */
time_t now;			/* Time use for mark and forward supending.  */
int force_sync;			/* GNU/Linux behaviour to sync on every line.
				   This off by default. Set to 1 to enable.  */
int set_local_time = 0;		/* Record local time, not message time.  */

const char args_doc[] = "";
const char doc[] = "Log system messages.";

/* Define keys for long options that do not have short counterparts. */
enum {
  OPT_NO_FORWARD = 256,
  OPT_NO_KLOG,
  OPT_NO_UNIXAF,
  OPT_IPANY
};

static struct argp_option argp_options[] = {
#define GRP 0
  /* Not sure about the long name. Maybe move into conffile even. */
  {NULL, 'a', "SOCKET", 0, "add unix socket to listen to (up to 19)", GRP+1},
  {NULL, 'l', "HOSTLIST", 0, "log hosts in HOSTLIST by their hostname", GRP+1},
  {NULL, 's', "DOMAINLIST", 0, "list of domains which should be stripped "
   "from the FQDN of hosts before logging their name", GRP+1},
  {"debug", 'd', NULL, 0, "print debug information (implies --no-detach)",
   GRP+1},
  {"hop", 'h', NULL, 0, "forward messages from remote hosts", GRP+1},
  {"inet", 'r', NULL, 0, "receive remote messages via internet domain socket",
   GRP+1},
  {"ipv4", '4', NULL, 0, "restrict to IPv4 transport (default)", GRP+1},
  {"ipv6", '6', NULL, 0, "restrict to IPv6 transport", GRP+1},
  {"ipany", OPT_IPANY, NULL, 0, "allow transport with IPv4 and IPv6", GRP+1},
  {"bind", 'b', "ADDR", 0, "bind listener to this address/name", GRP+1},
  {"bind-port", 'B', "PORT", 0, "bind listener to this port", GRP+1},
  {"mark", 'm', "INTVL", 0, "specify timestamp interval in minutes"
   " (0 for no timestamping)", GRP+1},
  {"no-detach", 'n', NULL, 0, "do not enter daemon mode", GRP+1},
  {"no-forward", OPT_NO_FORWARD, NULL, 0, "do not forward any messages "
   "(overrides --hop)", GRP+1},
#ifdef PATH_KLOG
  {"no-klog", OPT_NO_KLOG, NULL, 0, "do not listen to kernel log device "
   PATH_KLOG, GRP+1},
#endif
  {"no-unixaf", OPT_NO_UNIXAF, NULL, 0, "do not listen on unix domain "
   "sockets (overrides -a and -p)", GRP+1},
  {"pidfile", 'P', "FILE", 0, "override pidfile (default: "
   PATH_LOGPID ")", GRP+1},
  {"rcfile", 'f', "FILE", 0, "override configuration file (default: "
   PATH_LOGCONF ")",
   GRP+1},
  {"rcdir", 'D', "DIR", 0, "override configuration directory (default: "
   PATH_LOGCONFD ")", GRP+1},
  {"socket", 'p', "FILE", 0, "override default unix domain socket " PATH_LOG,
   GRP+1},
  {"sync", 'S', NULL, 0, "force a file sync on every line", GRP+1},
  {"local-time", 'T', NULL, 0, "set local time on received messages", GRP+1},
#undef GRP
  {NULL, 0, NULL, 0, NULL, 0}
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  char *endptr;
  int v;

  switch (key)
    {
    case 'a':
      add_funix (arg);
      break;

    case 'l':
      LocalHosts = crunch_list (LocalHosts, arg);
      break;

    case 's':
      StripDomains = crunch_list (StripDomains, arg);
      break;

    case 'd':
      Debug = 1;
      NoDetach = 1;
      break;

    case 'h':
      NoHops = 0;
      break;

    case 'r':
      AcceptRemote = 1;
      break;

    case '4':
      usefamily = AF_INET;
      break;

    case '6':
      usefamily = AF_INET6;
      break;

    case OPT_IPANY:
      usefamily = AF_UNSPEC;
      break;

    case 'b':
      BindAddress = arg;
      break;

    case 'B':
      BindPort = arg;
      break;

    case 'm':
      v = strtol (arg, &endptr, 10);
      if (*endptr)
        argp_error (state, "invalid value (`%s' near `%s')", arg, endptr);
      MarkInterval = v * 60;
      break;

    case 'n':
      NoDetach = 1;
      break;

    case OPT_NO_FORWARD:
      NoForward = 1;
      break;

    case OPT_NO_KLOG:
      NoKLog = 1;
      break;

    case OPT_NO_UNIXAF:
      NoUnixAF = 1;
      break;

    case 'P':
      PidFile = arg;
      break;

    case 'f':
      ConfFile = arg;
      break;

    case 'D':
      ConfDir = arg;
      break;

    case 'p':
      funix[0].name = arg;
      funix[0].fd = -1;
      break;

    case 'S':
      force_sync = 1;
      break;

    case 'T':
      set_local_time = 1;
      break;

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
  size_t i;
  FILE *fp;
  char *p;
  char line[MAXLINE + 1];
  char kline[MAXLINE + 1];
  int kline_len = 0;
  pid_t ppid = 0;		/* We run in debug mode and didn't fork.  */
  struct pollfd *fdarray;
  unsigned long nfds = 0;
#ifdef HAVE_SIGACTION
  struct sigaction sa;
#endif

  set_program_name (argv[0]);

  /* Initialize PATH_LOG as the first element of the unix sockets array.  */
  add_funix (PATH_LOG);

  /* Parse command line */
  iu_argp_init ("syslogd", default_program_authors);
  argp_parse (&argp, argc, argv, 0, NULL, NULL);

  /* Check desired port, if in demand at all.  */
  find_inet_port (BindPort);

  /* Daemonise, if not, set the buffering for line buffer.  */
  if (!NoDetach)
    {
      /* History: According to the GNU/Linux sysklogd ChangeLogs "Wed
         Feb 14 12:42:09 CST 1996: Dr. Wettstein Parent process of
         syslogd does not exit until child process has finished
         initialization process.  This allows rc.* startup to pause
         until syslogd facility is up and operating."

         IMO, the GNU/Linux distributors should fix there booting
         sequence.  But we still keep the approach.  */
      signal (SIGTERM, doexit);
      ppid = waitdaemon (0, 0, 30);
      if (ppid < 0)
        error (EXIT_FAILURE, errno, "could not become daemon");
    }
  else
    {
      if (Debug)
	dbg_output = 1;
      setvbuf (stdout, 0, _IOLBF, 0);
    }

  /* Get our hostname.  */
  LocalHostName = localhost ();
  if (LocalHostName == NULL)
    error (EXIT_FAILURE, errno, "can't get local host name");

  /* Get the domainname.  */
  p = strchr (LocalHostName, '.');
  if (p != NULL)
    {
      *p++ = '\0';
      LocalDomain = p;
    }
  else
    {
      struct addrinfo hints, *rp;
      int err;

      memset (&hints, 0, sizeof (hints));
      hints.ai_family = AF_UNSPEC;	/* Family is irrelevant.  */
      hints.ai_flags = AI_CANONNAME;

      /* Try to resolve the domainname by calling DNS.  */
      err = getaddrinfo (LocalHostName, NULL, &hints, &rp);
      if (err == 0)
	{
	  /* Override what we had */
	  free (LocalHostName);
	  LocalHostName = strdup (rp->ai_canonname);
	  p = strchr (LocalHostName, '.');
	  if (p != NULL)
	    {
	      *p++ = '\0';
	      LocalDomain = p;
	    }
	  freeaddrinfo (rp);
	}
      if (LocalDomain == NULL)
	LocalDomain = strdup ("");
    }

  consfile.f_type = F_CONSOLE;
  consfile.f_un.f_fname = strdup (ctty);

  signal (SIGTERM, die);
  signal (SIGINT, NoDetach ? die : SIG_IGN);
  signal (SIGQUIT, NoDetach ? die : SIG_IGN);

#ifdef HAVE_SIGACTION
  /* Register repeatable actions portably!  */
  sa.sa_flags = SA_RESTART;
  sigemptyset (&sa.sa_mask);

  sa.sa_handler = domark;
  (void) sigaction (SIGALRM, &sa, NULL);

  sa.sa_handler = NoDetach ? dbg_toggle : SIG_IGN;
  (void) sigaction (SIGUSR1, &sa, NULL);
#else /* !HAVE_SIGACTION */
  signal (SIGALRM, domark);
  signal (SIGUSR1, NoDetach ? dbg_toggle : SIG_IGN);
#endif

  alarm (TIMERINTVL);

  /* We add  3 = 1(klog) + 2(inet,inet6), even if they may stay unused.  */
  fdarray = (struct pollfd *) malloc ((nfunix + 3) * sizeof (*fdarray));
  if (fdarray == NULL)
    error (EXIT_FAILURE, errno, "can't allocate fd table");

  /* read configuration file */
  init (0);

#ifdef PATH_KLOG
  /* Initialize kernel logging and add to the list.  */
  if (!NoKLog)
    {
      fklog = open (PATH_KLOG, O_RDONLY, 0);
      if (fklog >= 0)
	{
	  fdarray[nfds].fd = fklog;
	  fdarray[nfds].events = POLLIN | POLLPRI;
	  nfds++;
	  dbg_printf ("Klog open %s\n", PATH_KLOG);
	}
      else
	dbg_printf ("Can't open %s: %s\n", PATH_KLOG, strerror (errno));
    }
#endif

  /* Initialize unix sockets.  */
  if (!NoUnixAF)
    {
      for (i = 0; i < nfunix; i++)
	{
	  funix[i].fd = create_unix_socket (funix[i].name);
	  if (funix[i].fd >= 0)
	    {
	      fdarray[nfds].fd = funix[i].fd;
	      fdarray[nfds].events = POLLIN | POLLPRI;
	      nfds++;
	      dbg_printf ("Opened UNIX socket `%s'.\n", funix[i].name);
	    }
	  else
	    dbg_printf ("Can't open %s: %s\n", funix[i].name,
			strerror (errno));
	}
    }

  /* Initialize inet socket and add it to the list.  */
  if (AcceptRemote)
    {
      create_inet_socket (usefamily, finet);
      if (finet[IU_FD_IP4] >= 0)
	{
	  /* IPv4 socket is present.  */
	  fdarray[nfds].fd = finet[IU_FD_IP4];
	  fdarray[nfds].events = POLLIN | POLLPRI;
	  nfds++;
	  dbg_printf ("Opened syslog UDP/IPv4 port.\n");
	}
      if (finet[IU_FD_IP6] >= 0)
	{
	  /* IPv6 socket is present.  */
	  fdarray[nfds].fd = finet[IU_FD_IP6];
	  fdarray[nfds].events = POLLIN | POLLPRI;
	  nfds++;
	  dbg_printf ("Opened syslog UDP/IPv6 port.\n");
	}
      if (finet[IU_FD_IP4] < 0 && finet[IU_FD_IP6] < 0)
	dbg_printf ("Can't open UDP port: %s\n", strerror (errno));
    }

  /* Tuck my process id away.  */
  fp = fopen (PidFile, "w");
  if (fp != NULL)
    {
      fprintf (fp, "%d\n", (int) getpid ());
      fclose (fp);
    }

  dbg_printf ("off & running....\n");

#ifdef HAVE_SIGACTION
  /* `sa' has been cleared already.  */
  sa.sa_handler = trigger_restart;
  (void) sigaction (SIGHUP, &sa, NULL);
#else /* !HAVE_SIGACTION */
  signal (SIGHUP, trigger_restart);
#endif

  if (NoDetach)
    {
      dbg_output = 1;
      dbg_printf ("Debugging is disabled. Send SIGUSR1 to PID=%d "
		  "to turn on debugging.\n", (int) getpid ());
      dbg_output = 0;
    }

  /* If we're doing waitdaemon(), tell the parent to exit,
     we are ready to roll.  */
  if (ppid)
    kill (ppid, SIGTERM);

  for (;;)
    {
      int nready;
      nready = poll (fdarray, nfds, -1);
      if (nready == 0)		/* ??  noop */
	continue;

      /* Sighup was dropped.  */
      if (restart)
	{
	  dbg_printf ("\nReceived SIGHUP, restarting syslogd.\n");
	  init (0);
	  restart = 0;
	  continue;
	}

      if (nready < 0)
	{
	  if (errno != EINTR)
	    logerror ("poll");
	  continue;
	}

      /*dbg_printf ("got a message (%d)\n", nready); */

      for (i = 0; i < nfds; i++)
	if (fdarray[i].revents & (POLLIN | POLLPRI))
	  {
	    int result;
	    socklen_t len;
	    if (fdarray[i].fd == -1)
	      continue;
	    else if (fdarray[i].fd == fklog)
	      {
		result = read (fdarray[i].fd, &kline[kline_len],
			       sizeof (kline) - kline_len - 1);

		if (result > 0)
		  {
		    kline_len += result;
		  }
		else if (result < 0 && errno != EINTR)
		  {
		    logerror ("klog");
		    fdarray[i].fd = fklog = -1;
		  }

		while (1)
		  {
		    char *bol, *eol;

		    kline[kline_len] = '\0';

		    for (bol = kline, eol = strchr (kline, '\n'); eol;
			 bol = eol, eol = strchr (bol, '\n'))
		      {
			*(eol++) = '\0';
			kline_len -= (eol - bol);
			printsys (bol);
		      }

		    /* This loop makes sure the daemon won't lock up
		     * on null bytes in the klog stream.  They still hurt
		     * efficiency, acting like a message separator that
		     * forces a shift-and-reiterate when the buffer was
		     * never full.
		     */
		    while (kline_len && !*bol)
		      {
			bol++;
			kline_len--;
		      }

		    if (!kline_len)
		      break;

		    if (bol != kline)
		      {
			/* shift the partial line to start of buffer, so
			 * we can re-iterate.
			 */
			memmove (kline, bol, kline_len);
		      }
		    else
		      {
			if (kline_len < MAXLINE)
			  break;

			/* The pathological case of a single message that
			 * overfills our buffer.  The best we can do is
			 * log it in pieces.
			 */
			printsys (kline);

			/* Clone priority signal if present
			 * We merely shift the kline_len pointer after
			 * it so the next chunk is written after it.
			 *
			 * strchr(kline,'>') is not used as it would allow
			 * a pathological line ending in '>' to cause an
			 * endless loop.
			 */
			if (kline[0] == '<'
			    && isdigit (kline[1]) && kline[2] == '>')
			  kline_len = 3;
			else
			  kline_len = 0;
		      }
		  }
	      }
	    else if (fdarray[i].fd == finet[IU_FD_IP4]
		     || fdarray[i].fd == finet[IU_FD_IP6])
	      {
		struct sockaddr_storage frominet;
		/*dbg_printf ("inet message\n"); */
		len = sizeof (frominet);
		memset (line, '\0', sizeof (line));
		result = recvfrom (fdarray[i].fd, line, MAXLINE, 0,
				   (struct sockaddr *) &frominet, &len);
		if (result > 0)
		  {
		    line[result] = '\0';
		    printline (cvthname ((struct sockaddr *) &frominet, len), line);
		  }
		else if (result < 0 && errno != EINTR)
		  logerror ("recvfrom inet");
	      }
	    else
	      {
		struct sockaddr_un fromunix;
		/*dbg_printf ("unix message\n"); */
		len = sizeof (fromunix);
		result = recvfrom (fdarray[i].fd, line, MAXLINE, 0,
				   (struct sockaddr *) &fromunix, &len);
		if (result > 0)
		  {
		    line[result] = '\0';
		    printline (LocalHostName, line);
		  }
		else if (result < 0 && errno != EINTR)
		  logerror ("recvfrom unix");
	      }
	  }
	else if (fdarray[i].revents & POLLNVAL)
	  {
	    logerror ("poll nval\n");
	    fdarray[i].fd = -1;
	  }
	else if (fdarray[i].revents & POLLERR)
	  logerror ("poll err\n");
	else if (fdarray[i].revents & POLLHUP)
	  logerror ("poll hup\n");
    }				/* for (;;) */
}				/* main */

#ifndef SUN_LEN
# define SUN_LEN(unp) (strlen((unp)->sun_path) + 3)
#endif

static void
add_funix (const char *name)
{
  funix = realloc (funix, (nfunix + 1) * sizeof (*funix));
  if (funix == NULL)
    error (EXIT_FAILURE, errno, "cannot allocate space for unix sockets");

  funix[nfunix].name = name;
  funix[nfunix].fd = -1;
  nfunix++;
}

static int
create_unix_socket (const char *path)
{
  int fd;
  struct sockaddr_un sunx;
  char line[MAXLINE + 1];

  if (path[0] == '\0')
    return -1;

  if (strlen (path) >= sizeof (sunx.sun_path))
    {
      snprintf (line, sizeof (line), "UNIX socket name too long: %s", path);
      logerror (line);
      return -1;
    }

  unlink (path);

  memset (&sunx, 0, sizeof (sunx));
  sunx.sun_family = AF_UNIX;
  strncpy (sunx.sun_path, path, sizeof (sunx.sun_path) - 1);
  fd = socket (AF_UNIX, SOCK_DGRAM, 0);
  if (fd < 0 || bind (fd, (struct sockaddr *) &sunx, SUN_LEN (&sunx)) < 0
      || chmod (path, 0666) < 0)
    {
      snprintf (line, sizeof (line), "cannot create %s", path);
      logerror (line);
      dbg_printf ("cannot create %s: %s\n", path, strerror (errno));
      close (fd);
      fd = -1;
    }
  return fd;
}

static void
create_inet_socket (int af, int fd46[2])
{
  int err, fd = -1;
  struct addrinfo hints, *rp, *ai;

  /* Invalidate old descriptors.  */
  fd46[IU_FD_IP4] = fd46[IU_FD_IP6] = -1;

  if (!LogPortText)
    {
      dbg_printf ("No listen port has been accepted.\n");
      return;
    }

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = af;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  err = getaddrinfo (BindAddress, LogPortText, &hints, &rp);
  if (err)
    {
      logerror ("lookup error, suspending inet service");
      return;
    }

  for (ai = rp; ai; ai = ai->ai_next)
    {
      int yes = 1;

      fd = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (fd < 0)
	continue;

      err = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (yes));
      if (err < 0)
	logerror ("failed to set SO_REUSEADDR");

      if (ai->ai_family == AF_INET6)
	{
	  /* Avoid dual stacked sockets.  Better to use distinct sockets.  */
	  (void) setsockopt (fd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof (yes));
	}

      if (bind (fd, ai->ai_addr, ai->ai_addrlen) < 0)
	{
	  close (fd);
	  fd = -1;
	  continue;
	}
      /* Register any success.  */
      if (ai->ai_family == AF_INET && fd46[IU_FD_IP4] < 0)
	fd46[IU_FD_IP4] = fd;
      else if (ai->ai_family == AF_INET6 && fd46[IU_FD_IP6] < 0)
	fd46[IU_FD_IP6] = fd;
    }
  freeaddrinfo (rp);

  if (fd46[IU_FD_IP4] < 0 && fd46[IU_FD_IP6] < 0)
    {
      logerror ("inet service, failed lookup.");
      return;
    }
  return;
}

char **
crunch_list (char **oldlist, char *list)
{
  int count, i;
  char *p, *q;

  p = list;

  /* Strip off trailing delimiters.  */
  while (p[strlen (p) - 1] == LIST_DELIMITER)
    {
      p[strlen (p) - 1] = '\0';
    }
  /* Cut off leading delimiters.  */
  while (p[0] == LIST_DELIMITER)
    {
      p++;
    }

  /* Bailout early the string is empty.  */
  if (*p == '\0')
    return oldlist;

  /* Count delimiters to calculate elements.  */
  for (count = 1, i = 0; p[i]; i++)
    if (p[i] == LIST_DELIMITER)
      count++;

  /* Count how many we add in the old list.  */
  for (i = 0; oldlist && oldlist[i]; i++)
    ;

  /* allocate enough space */
  oldlist = (char **) realloc (oldlist, (i + count + 1) * sizeof (*oldlist));
  if (oldlist == NULL)
    error (EXIT_FAILURE, errno, "can't allocate memory");

  /*
     We now can assume that the first and last
     characters are different from any delimiters,
     so we don't have to care about it anymore.  */

  /* Start from where we left last time.  */
  for (count = i; (q = strchr (p, LIST_DELIMITER)) != NULL;
       count++, p = q, p++)
    {
      oldlist[count] = (char *) malloc ((q - p + 1) * sizeof (char));
      if (oldlist[count] == NULL)
        error (EXIT_FAILURE, errno, "can't allocate memory");

      strncpy (oldlist[count], p, q - p);
      oldlist[count][q - p] = '\0';
    }

  /* take the last one */
  oldlist[count] = (char *) xmalloc ((strlen (p) + 1) * sizeof (char));
  if (oldlist[count] == NULL)
    error (EXIT_FAILURE, errno, "can't allocate memory");

  strcpy (oldlist[count], p);

  oldlist[++count] = NULL;	/* terminate the array with a NULL */

  if (Debug)
    {
      for (count = 0; oldlist[count]; count++)
	printf ("#%d: %s\n", count, oldlist[count]);
    }
  return oldlist;
}

/* Take a raw input line, decode the message, and print the message on
   the appropriate log files.  */
void
printline (const char *hname, const char *msg)
{
  int c, pri;
  const char *p;
  char *q, line[MAXLINE + 1];

  /* test for special codes */
  pri = DEFUPRI;
  p = msg;
  if (*p == '<')
    {
      pri = 0;
      while (isdigit (*++p))
	pri = 10 * pri + (*p - '0');
      if (*p == '>')
	++p;
    }

  /* This overrides large positive and overflowing negative values.  */
  if (pri & ~(LOG_FACMASK | LOG_PRIMASK))
    pri = DEFUPRI;

  /* Avoid undefined facilities.  */
  if (LOG_FAC (pri) > LOG_NFACILITIES)
    pri = DEFUPRI;

  /* Do not allow users to log kernel messages.  */
  if (LOG_FAC (pri) == (LOG_KERN >> 3))
    pri = LOG_MAKEPRI (LOG_USER, LOG_PRI (pri));

  q = line;
  while ((c = *p++) != '\0' && q < &line[sizeof (line) - 1])
    if (iscntrl (c))
      if (c == '\n')
	*q++ = ' ';
      else if (c == '\t')
	*q++ = '\t';
      else if (c >= 0177)
	*q++ = c;
      else
	{
	  *q++ = '^';
	  *q++ = c ^ 0100;
	}
    else
      *q++ = c;
  *q = '\0';

  /* This for the default behaviour on GNU/Linux syslogd who
     sync on every line.  */
  if (force_sync)
    logmsg (pri, line, hname, SYNC_FILE);
  else
    logmsg (pri, line, hname, 0);
}

/* Take a raw input line from /dev/klog, split and format similar to
   syslog().  */
void
printsys (const char *msg)
{
  int c, pri, flags;
  char *lp, *q, line[MAXLINE + 1];
  const char *p;

  strcpy (line, "vmunix: ");
  lp = line + strlen (line);
  for (p = msg; *p != '\0';)
    {
      flags = SYNC_FILE | ADDDATE;	/* Fsync after write.  */
      pri = DEFSPRI;
      if (*p == '<')
	{
	  pri = 0;
	  while (isdigit (*++p))
	    pri = 10 * pri + (*p - '0');
	  if (*p == '>')
	    ++p;
	}
      else
	{
	  /* kernel printf's come out on console */
	  flags |= IGN_CONS;
	}
      if (pri & ~(LOG_FACMASK | LOG_PRIMASK))
	pri = DEFSPRI;
      q = lp;
      while (*p != '\0' && (c = *p++) != '\n' && q < &line[MAXLINE])
	*q++ = c;
      *q = '\0';
      logmsg (pri, line, LocalHostName, flags);
    }
}

/* Decode a priority into textual information like auth.emerg.  */
char *
textpri (int pri)
{
  static char res[20];
  CODE *c_pri, *c_fac;

  for (c_fac = facilitynames; c_fac->c_name
       && !(c_fac->c_val == LOG_FAC (pri) << 3); c_fac++);
  for (c_pri = prioritynames; c_pri->c_name
       && !(c_pri->c_val == LOG_PRI (pri)); c_pri++);

  snprintf (res, sizeof (res), "%s.%s", c_fac->c_name, c_pri->c_name);

  return res;
}

/* Log a message to the appropriate log files, users, etc. based on
   the priority.  */
void
logmsg (int pri, const char *msg, const char *from, int flags)
{
  struct filed *f;
  int fac, msglen, prilev;
#ifdef HAVE_SIGACTION
  sigset_t sigs, osigs;
#else
  int omask;
#endif

  const char *timestamp;

  dbg_printf ("(logmsg): %s (%d), flags %x, from %s, msg %s\n",
	      textpri (pri), pri, flags, from, msg);

#ifdef HAVE_SIGACTION
  sigemptyset (&sigs);
  sigaddset (&sigs, SIGHUP);
  sigaddset (&sigs, SIGALRM);
  sigprocmask (SIG_BLOCK, &sigs, &osigs);
#else
  omask = sigblock (sigmask (SIGHUP) | sigmask (SIGALRM));
#endif

  /* Check to see if msg looks non-standard.  */
  msglen = strlen (msg);
  if (msglen < 16 || msg[3] != ' ' || msg[6] != ' ' ||
      msg[9] != ':' || msg[12] != ':' || msg[15] != ' ')
    flags |= ADDDATE;

  time (&now);
  if (flags & ADDDATE)
    timestamp = ctime (&now) + 4;
  else
    {
      if (set_local_time)
	timestamp = ctime (&now) + 4;
      else
	timestamp = msg;
      msg += 16;
      msglen -= 16;
    }

  /* Extract facility and priority level.  */
  if (flags & MARK)
#ifdef INTERNAL_MARK
    fac = LOG_FAC (INTERNAL_MARK);
#else
    fac = LOG_NFACILITIES;
#endif
  else
    fac = LOG_FAC (pri);
  prilev = LOG_PRI (pri);

  /* Log the message to the particular outputs. */
  if (!Initialized)
    {
      f = &consfile;
      f->f_file = open (ctty, O_WRONLY, 0);
      f->f_prevhost = strdup (LocalHostName);
      if (f->f_file >= 0)
	{
	  fprintlog (f, from, flags, msg);
	  close (f->f_file);
	}
#ifdef HAVE_SIGACTION
      sigprocmask (SIG_SETMASK, &osigs, 0);
#else
      sigsetmask (omask);
#endif
      return;
    }
  for (f = Files; f; f = f->f_next)
    {
      /* Skip messages that are incorrect priority. */
      if (!(f->f_pmask[fac] & LOG_MASK (prilev)))
	continue;

      if (f->f_type == F_CONSOLE && (flags & IGN_CONS))
	continue;

      /* Don't output marks to recently written files.  */
      if ((flags & MARK) && (now - f->f_time) < MarkInterval / 2)
	continue;

      if (f->f_progname)
	{
	  /* The usual, and desirable, formattings are:
	   *
	   *   prg: message text
	   *   prg[PIDNO]: message text
	   */

	  /* Skip on selector mismatch.  */
	  if (strncmp (msg, f->f_progname, f->f_prognlen))
	    continue;

	  /* Avoid matching on prefixes.  */
	  if (isalnum (msg[f->f_prognlen])
	      || msg[f->f_prognlen] == '-'
	      || msg[f->f_prognlen] == '_')
	    continue;
	}

      /* Suppress duplicate lines to this file.  */
      if ((flags & MARK) == 0 && msglen == f->f_prevlen && f->f_prevhost
	  && !strcmp (msg, f->f_prevline) && !strcmp (from, f->f_prevhost))
	{
	  strncpy (f->f_lasttime, timestamp, sizeof (f->f_lasttime) - 1);
	  f->f_prevcount++;
	  dbg_printf ("msg repeated %d times, %ld sec of %d\n",
		      f->f_prevcount, now - f->f_time,
		      repeatinterval[f->f_repeatcount]);
	  /* If domark would have logged this by now, flush it now (so we
	     don't hold isolated messages), but back off so we'll flush
	     less often in the future.  */
	  if (now > REPEATTIME (f))
	    {
	      fprintlog (f, from, flags, (char *) NULL);
	      BACKOFF (f);
	    }
	}
      else
	{
	  /* New line, save it.  */
	  if (f->f_prevcount)
	    fprintlog (f, from, 0, (char *) NULL);
	  f->f_repeatcount = 0;
	  strncpy (f->f_lasttime, timestamp, sizeof (f->f_lasttime) - 1);
	  free (f->f_prevhost);
	  f->f_prevhost = strdup (from);
	  if (msglen < MAXSVLINE)
	    {
	      f->f_prevlen = msglen;
	      f->f_prevpri = pri;
	      strcpy (f->f_prevline, msg);
	      fprintlog (f, from, flags, (char *) NULL);
	    }
	  else
	    {
	      f->f_prevline[0] = 0;
	      f->f_prevlen = 0;
	      fprintlog (f, from, flags, msg);
	    }
	}
    }
#ifdef HAVE_SIGACTION
  sigprocmask (SIG_SETMASK, &osigs, 0);
#else
  sigsetmask (omask);
#endif
}

void
fprintlog (struct filed *f, const char *from, int flags, const char *msg)
{
  struct iovec iov[IOVCNT];
  struct iovec *v;
  int l;
  char line[MAXLINE + 1], repbuf[80], greetings[200];
  time_t fwd_suspend;

  v = iov;
  /* Be paranoid.  */
  memset (v, 0, sizeof (struct iovec) * IOVCNT);
  if (f->f_type == F_WALL)
    {
      v->iov_base = greetings;
      snprintf (greetings, sizeof (greetings),
		"\r\n\7Message from syslogd@%s at %.24s ...\r\n",
		f->f_prevhost, ctime (&now));
      v->iov_len = strlen (greetings);
      v++;
      v->iov_base = (char *) "";
      v->iov_len = 0;
      v++;
    }
  else
    {
      v->iov_base = f->f_lasttime;
      v->iov_len = sizeof (f->f_lasttime) - 1;
      v++;
      v->iov_base = (char *) " ";
      v->iov_len = 1;
      v++;
    }
  if (f->f_prevhost)
    {
      v->iov_base = f->f_prevhost;
      v->iov_len = strlen (v->iov_base);
      v++;
    }
  v->iov_base = (char *) " ";
  v->iov_len = 1;
  v++;

  if (msg)
    {
      v->iov_base = (char *) msg;
      v->iov_len = strlen (msg);
    }
  else if (f->f_prevcount > 1)
    {
      v->iov_base = repbuf;
      snprintf (repbuf, sizeof (repbuf), "last message repeated %d times",
		f->f_prevcount);
      v->iov_len = strlen (repbuf);
    }
  else
    {
      v->iov_base = f->f_prevline;
      v->iov_len = f->f_prevlen;
    }
  v++;

  dbg_printf ("Logging to %s", TypeNames[f->f_type]);

  switch (f->f_type)
    {
    case F_UNUSED:
      f->f_time = now;
      dbg_printf ("\n");
      break;

    case F_FORW_SUSP:
      fwd_suspend = time ((time_t *) 0) - f->f_time;
      if (fwd_suspend >= INET_SUSPEND_TIME)
	{
	  dbg_printf ("\nForwarding suspension over, retrying FORW ");
	  f->f_type = F_FORW;
	  goto f_forw;
	}
      else
	{
	  dbg_printf (" %s\n", f->f_un.f_forw.f_hname);
	  dbg_printf ("Forwarding suspension not over, time left: %d.\n",
		      INET_SUSPEND_TIME - fwd_suspend);
	}
      break;

    case F_FORW_UNKN:
      dbg_printf (" %s\n", f->f_un.f_forw.f_hname);
      fwd_suspend = time ((time_t *) 0) - f->f_time;
      if (fwd_suspend >= INET_SUSPEND_TIME)
	{
	  struct addrinfo hints, *rp;
	  int err;

	  memset (&hints, 0, sizeof (hints));
	  hints.ai_family = usefamily;
#ifdef AI_ADDRCONFIG
	  if (usefamily == AF_UNSPEC)
	    hints.ai_flags |= AI_ADDRCONFIG;
#endif
	  err = getaddrinfo (f->f_un.f_forw.f_hname, LogForwardPort,
			     &hints, &rp);
	  if (err)
	    {
	      dbg_printf ("Failure: %s\n", gai_strerror (err));
	      dbg_printf ("Retries: %d\n", f->f_prevcount);
	      if (--f->f_prevcount < 0)
		{
		  f->f_type = F_UNUSED;
		  free (f->f_un.f_forw.f_hname);
		  f->f_un.f_forw.f_hname = NULL;
		}
	    }
	  else
	    {
	      dbg_printf ("%s found, resuming.\n", f->f_un.f_forw.f_hname);
	      f->f_un.f_forw.f_addrlen = rp->ai_addrlen;
	      memcpy (&f->f_un.f_forw.f_addr, rp->ai_addr, rp->ai_addrlen);
	      freeaddrinfo (rp);
	      f->f_prevcount = 0;
	      f->f_type = F_FORW;
	      goto f_forw;
	    }
	}
      else
	dbg_printf ("Forwarding suspension not over, time left: %d\n",
		    INET_SUSPEND_TIME - fwd_suspend);
      break;

    case F_FORW:
    f_forw:
      dbg_printf (" %s\n", f->f_un.f_forw.f_hname);
      if (strcasecmp (from, LocalHostName) && NoHops)
	dbg_printf ("Not forwarding remote message.\n");
      else if (NoForward)
	dbg_printf ("Not forwarding because forwarding is disabled.\n");
      else
	{
	  int temp_finet, *pfinet;	/* PFINET points to active fd.  */

	  if (f->f_un.f_forw.f_addr.ss_family == AF_INET)
	    pfinet = &finet[IU_FD_IP4];
	  else	/* AF_INET6 */
	    pfinet = &finet[IU_FD_IP6];

	  temp_finet = *pfinet;

	  if (temp_finet < 0)
	    {
	      int err;
	      struct addrinfo hints, *rp;

	      /* Forwarding needs a temporary socket.
	       * The source port is fixed!  */
	      memset (&hints, 0, sizeof (hints));
	      hints.ai_family = f->f_un.f_forw.f_addr.ss_family;
	      hints.ai_socktype = SOCK_DGRAM;
	      hints.ai_flags = AI_PASSIVE;

	      err = getaddrinfo (NULL, LogForwardPort, &hints, &rp);
	      if (err)
		{
		  dbg_printf ("Not forwarding due to lookup failure: %s.\n",
			      gai_strerror(err));
		  break;
		}
	      temp_finet = socket (rp->ai_family, rp->ai_socktype,
				   rp->ai_protocol);
	      if (temp_finet < 0)
		{
		  dbg_printf ("Not forwarding due to socket failure.\n");
		  freeaddrinfo (rp);
		  break;
		}

	      err = bind (temp_finet, rp->ai_addr, rp->ai_addrlen);
	      freeaddrinfo (rp);
	      if (err)
		{
		  dbg_printf ("Not forwarding due to bind error: %s.\n",
			      strerror (errno));
		  break;
		}
	    } /* Creation of temporary outgoing socket since "finet < 0" */

	  f->f_time = now;
	  snprintf (line, sizeof (line), "<%d>%.15s %s",
		    f->f_prevpri, (char *) iov[0].iov_base,
		    (char *) iov[4].iov_base);
	  l = strlen (line);
	  if (l > MAXLINE)
	    l = MAXLINE;
	  if (sendto (temp_finet, line, l, 0,
		      (struct sockaddr *) &f->f_un.f_forw.f_addr,
		      f->f_un.f_forw.f_addrlen) != l)
	    {
	      int e = errno;
	      dbg_printf ("INET sendto error: %d = %s.\n", e, strerror (e));
	      f->f_type = F_FORW_SUSP;
	      errno = e;
	      logerror ("sendto");
	    }

	  if (*pfinet < 0)
	    close (temp_finet);	/* Only temporary socket may be closed.  */
	}
      break;

    case F_CONSOLE:
      f->f_time = now;
      if (flags & IGN_CONS)
	{
	  dbg_printf (" (ignored)\n");
	  break;
	}

    case F_TTY:
    case F_FILE:
    case F_PIPE:
      f->f_time = now;
      dbg_printf (" %s\n", f->f_un.f_fname);
      if (f->f_type == F_TTY || f->f_type == F_CONSOLE)
	{
	  v->iov_base = (char *) "\r\n";
	  v->iov_len = 2;
	}
      else
	{
	  v->iov_base = (char *) "\n";
	  v->iov_len = 1;
	}
    again:
      if (writev (f->f_file, iov, IOVCNT) < 0)
	{
	  int e = errno;

	  /* XXX: If a named pipe is full, ignore it.  */
	  if (f->f_type == F_PIPE && e == EAGAIN)
	    break;

	  close (f->f_file);
	  /* Check for errors on TTY's due to loss of tty. */
	  if ((e == EIO || e == EBADF)
	      && (f->f_type == F_TTY || f->f_type == F_CONSOLE))
	    {
	      f->f_file = open (f->f_un.f_fname, O_WRONLY | O_APPEND, 0);
	      if (f->f_file < 0)
		{
		  f->f_type = F_UNUSED;
		  logerror (f->f_un.f_fname);
		  free (f->f_un.f_fname);
		  f->f_un.f_fname = NULL;
		}
	      else
		goto again;
	    }
	  else
	    {
	      f->f_type = F_UNUSED;
	      errno = e;
	      logerror (f->f_un.f_fname);
	      free (f->f_un.f_fname);
	      f->f_un.f_fname = NULL;
	    }
	}
      else if ((flags & SYNC_FILE) && !(f->f_flags & OMIT_SYNC))
	fsync (f->f_file);
      break;

    case F_USERS:
    case F_WALL:
      f->f_time = now;
      dbg_printf ("\n");
      v->iov_base = (char *) "\r\n";
      v->iov_len = 2;
      wallmsg (f, iov);
      break;
    }

  if (f->f_type != F_FORW_UNKN)
    f->f_prevcount = 0;
}

/* Write the specified message to either the entire world,
 * or to a list of approved users.  */
void
wallmsg (struct filed *f, struct iovec *iov)
{
  static int reenter;		/* Avoid calling ourselves.  */
  STRUCT_UTMP *utp;
#if defined UTMP_NAME_FUNCTION || !defined HAVE_GETUTXENT
  STRUCT_UTMP *utmpbuf;
  size_t utmp_count;
#endif /* UTMP_NAME_FUNCTION || !HAVE_GETUTXENT */
  int i;
  char *p;
  char line[sizeof (utp->ut_line) + 1];

  if (reenter++)
    return;

#if !defined UTMP_NAME_FUNCTION && defined HAVE_GETUTXENT
  setutxent ();

  while ((utp = getutxent ()))
#else /* UTMP_NAME_FUNCTION || !HAVE_GETUTXENT */
  if (read_utmp (UTMP_FILE, &utmp_count, &utmpbuf,
		 READ_UTMP_USER_PROCESS | READ_UTMP_CHECK_PIDS) < 0)
    {
      logerror ("opening utmp file");
      return;
    }

  for (utp = utmpbuf; utp < utmpbuf + utmp_count; utp++)
#endif /* UTMP_NAME_FUNCTION || !HAVE_GETUTXENT */
    {
      strncpy (line, utp->ut_line, sizeof (utp->ut_line));
      line[sizeof (utp->ut_line)] = '\0';
      if (f->f_type == F_WALL)
	{
	  /* Note we're using our own version of ttymsg
	     which does a double fork () to not have
	     zombies.  No need to waitpid().  */
	  p = ttymsg (iov, IOVCNT, line, TTYMSGTIME);
	  if (p != NULL)
	    {
	      errno = 0;	/* Already in message. */
	      logerror (p);
	    }
	  continue;
	}
      /* Should we send the message to this user? */
      for (i = 0; i < f->f_un.f_user.f_nusers; i++)
	if (!strncmp (f->f_un.f_user.f_unames[i], UT_USER (utp),
		      sizeof (UT_USER (utp))))
	  {
	    p = ttymsg (iov, IOVCNT, line, TTYMSGTIME);
	    if (p != NULL)
	      {
		errno = 0;	/* Already in message. */
		logerror (p);
	      }
	    break;
	  }
    }
#if defined UTMP_NAME_FUNCTION || !defined HAVE_GETUTXENT
  free (utmpbuf);
#else /* !UTMP_NAME_FUNCTION && HAVE_GETUTXENT */
  endutxent ();
#endif
  reenter = 0;
}

/* Return a printable representation of a host address.  */
const char *
cvthname (struct sockaddr *f, socklen_t len)
{
  int err;
  char *p;

  err = getnameinfo (f, len, addrstr, sizeof (addrstr),
		     NULL, 0, NI_NUMERICHOST);
  if (err)
    {
      dbg_printf ("Malformed from address: %s.\n",
		  gai_strerror (err));
      return "???";
    }

  dbg_printf ("cvthname(%s)\n", addrstr);

  err = getnameinfo (f, len, addrname, sizeof (addrname),
		     NULL, 0, NI_NAMEREQD);
  if (err)
    {
      dbg_printf ("Host name for your address (%s) unknown.\n", addrstr);
      return addrstr;
    }

  p = strchr (addrname, '.');
  if (p != NULL)
    {
      if (strcasecmp (p + 1, LocalDomain) == 0)
	*p = '\0';
      else
	{
	  int count;

	  if (StripDomains)
	    {
	      count = 0;
	      while (StripDomains[count])
		{
		  if (strcasecmp (p + 1, StripDomains[count]) == 0)
		    {
		      *p = '\0';
		      return addrname;
		    }
		  count++;
		}
	    }
	  if (LocalHosts)
	    {
	      count = 0;
	      while (LocalHosts[count])
		{
		  if (strcasecmp (addrname, LocalHosts[count]) == 0)
		    {
		      *p = '\0';
		      return addrname;
		    }
		  count++;
		}
	    }
	}
    }
  return addrname;
}

void
domark (int signo _GL_UNUSED_PARAMETER)
{
  struct filed *f;

  now = time ((time_t *) NULL);
  if (MarkInterval > 0)
    {
      MarkSeq += TIMERINTVL;
      if (MarkSeq >= MarkInterval)
	{
	  logmsg (LOG_INFO, "-- MARK --", LocalHostName, ADDDATE | MARK);
	  MarkSeq = 0;
	}
    }

  for (f = Files; f; f = f->f_next)
    {
      if (f->f_prevcount && now >= REPEATTIME (f))
	{
	  dbg_printf ("flush %s: repeated %d times, %d sec.\n",
		      TypeNames[f->f_type], f->f_prevcount,
		      repeatinterval[f->f_repeatcount]);
	  fprintlog (f, LocalHostName, 0, (char *) NULL);
	  BACKOFF (f);
	}
    }

#ifndef HAVE_SIGACTION
  signal (SIGALRM, domark);
#endif
  alarm (TIMERINTVL);
}

/* Print syslogd errors some place.  */
void
logerror (const char *type)
{
  char buf[100];

  if (errno)
    snprintf (buf, sizeof (buf), "syslogd: %s: %s", type, strerror (errno));
  else
    snprintf (buf, sizeof (buf), "syslogd: %s", type);
  errno = 0;
  dbg_printf ("%s\n", buf);
  logmsg (LOG_SYSLOG | LOG_ERR, buf, LocalHostName, ADDDATE);
}

void
doexit (int signo _GL_UNUSED_PARAMETER)
{
  _exit (EXIT_SUCCESS);
}

void
die (int signo)
{
  struct filed *f;
  int was_initialized = Initialized;
  char buf[100];
  size_t i;

  Initialized = 0;		/* Don't log SIGCHLDs. */
  for (f = Files; f != NULL; f = f->f_next)
    {
      /* Flush any pending output.  */
      if (f->f_prevcount)
	fprintlog (f, LocalHostName, 0, (char *) NULL);
    }
  Initialized = was_initialized;
  if (signo)
    {
      dbg_printf ("%s: exiting on signal %d\n",
                  program_invocation_name, signo);
      snprintf (buf, sizeof (buf), "exiting on signal %d", signo);
      errno = 0;
      logerror (buf);
    }

  if (fklog >= 0)
    close (fklog);

  for (i = 0; i < nfunix; i++)
    if (funix[i].fd >= 0)
      {
	close (funix[i].fd);
	if (funix[i].name)
	  unlink (funix[i].name);
      }

  if (finet[IU_FD_IP4] >= 0)
    close (finet[IU_FD_IP4]);
  if (finet[IU_FD_IP6] >= 0)
    close (finet[IU_FD_IP6]);

  exit (EXIT_SUCCESS);
}

/*
 * Return zero on error.
 */
static int
load_conffile (const char *filename, struct filed **nextp)
{
  FILE *cf;
  struct filed *f;
#ifndef LINE_MAX
# define LINE_MAX 2048
#endif
  size_t line_max = LINE_MAX;
  char *cbuf;
  char *cline;
  int cont_line = 0;

  /* Beware: Do not assume *nextp to be NULL.  */

  /* Open the configuration file.  */
  cf = fopen (filename, "r");
  if (cf == NULL)
    {
      dbg_printf ("cannot open %s\n", filename);

      /* Add emergency logging if everything else was missing.  */
      if (*nextp == NULL)
	{
	  /* Send LOG_ERR to the system console.  */
	  f = (struct filed *) calloc (1, sizeof (*f));
	  cfline ("*.ERR\t" PATH_CONSOLE, f);		/* Erases *f!  */

	  /* Below that, send LOG_EMERG to all users.  */
	  f->f_next = (struct filed *) calloc (1, sizeof (*f));
	  cfline ("*.PANIC\t*", f->f_next);	/* Erases *(f->f_next)!  */

	  *nextp = f;	/* Return this minimal table to the caller.  */
	}

      Initialized = 1;
      return 1;
    }

  /* Allocate a buffer for line parsing.  */
  cbuf = malloc (line_max);
  if (cbuf == NULL)
    {
      /* There is no graceful recovery here.  */
      dbg_printf ("cannot allocate space for configuration\n");
      fclose (cf);
      return 0;
    }
  cline = cbuf;

  /* Reset selecting program.  */
  free (selector);
  selector = NULL;

  /* Line parsing :
     - skip comments,
     - strip off trailing spaces,
     - skip empty lines,
     - glob leading spaces,
     - readjust buffer if line is too big,
     - deal with continuation lines, last char is '\' .  */
  while (fgets (cline, line_max - (cline - cbuf), cf) != NULL)
    {
      char *p;
      size_t len = strlen (cline);

      /* If this is a continuation line, skip leading whitespace for
         compatibility with sysklogd.  Note that this requires
         whitespace before the backslash in the previous line if you
         want to separate the selector from the action.  */
      if (cont_line)
	{
	  char *start = cline;
	  while (*start == ' ' || *start == '\t')
	    start++;
	  len = len - (start - cline);
	  memmove (cline, start, len + 1);
	  cont_line = 0;
	}

      /* No newline, so the line is too big for the buffer.  Readjust.  */
      if (strchr (cline, '\n') == NULL)
	{
	  size_t offset = cline - cbuf;
	  char *tmp;
	  tmp = realloc (cbuf, line_max * 2);
	  if (tmp == NULL)
	    {
	      /* Sigh ...  */
	      dbg_printf ("cannot allocate space configuration\n");
	      fclose (cf);
	      free (cbuf);
	      return 0;
	    }
	  else
	    cbuf = tmp;
	  line_max *= 2;
	  cline = cbuf + offset + len - 1;
	  continue;
	}
      else
	cline = cbuf;

      /* Glob the leading spaces.  */
      for (p = cline; isspace (*p); ++p)
	;

      /* Record program selector.
       *
       * Acceptable formats are typically:
       *
       *   !name
       *   #! name
       *   ! *
       *
       * The latter is clearing the previous setting.
       */
      if (*p == '!' || (*p == '#' && *(p + 1) == '!'))
	{
	  if (*++p == '!')
	    ++p;
	  while (isspace (*p))
	    ++p;
	  if (*p == '\0')
	    continue;

	  /* Reset previous setting.  */
	  free (selector);
	  selector = NULL;

	  if (*p != '*')
	    {
	      char *sep;

	      /* BSD systems allow multiple selectors
	       * separated by commata.  Strip away any
	       * additional names since at this time
	       * we only support a single name.
	       */
	      sep = strchr (p, ',');
	      if (sep)
		*sep = '\0';

	      /* Remove trailing whitespace.  */
	      sep = strpbrk (p, " \t\n\r");
	      if (sep)
		*sep = '\0';

	      selector = strdup (p);
	    }
	  continue;
	}

      /* Skip comments and empty line.  */
      if (*p == '\0' || *p == '#')
	continue;

      memmove (cline, p, strlen (p) + 1);

      /* Cut the trailing spaces.  */
      for (p = strchr (cline, '\0'); isspace (*--p);)
	;

      /* if '\', indicates continuation on the next line.  */
      if (*p == '\\')
	{
	  *p = '\0';
	  cline = p;
	  cont_line = 1;
	  continue;
	}

      *++p = '\0';

      /* Send the line for more parsing.
       * Then generate the new entry,
       * inserting it at the head of
       * the already existing table.
       */
      f = (struct filed *) calloc (1, sizeof (*f));
      cfline (cbuf, f);			/* Erases *f!  */
      f->f_next = *nextp;
      *nextp = f;
    }

  /* Close the configuration file.  */
  fclose (cf);
  free (cbuf);

  return 1;
}

/*
 * Return zero on error.
 */
static int
load_confdir (const char *dirname, struct filed **nextp)
{
  int rc = 0, found = 0;
  struct dirent *dent;
  DIR *dir;

  dir = opendir (dirname);
  if (dir == NULL)
    {
      dbg_printf ("cannot open %s\n", dirname);
      return 1;		/* Acceptable deviation.  */
    }

  while ((dent = readdir (dir)) != NULL)
    {
      struct stat st;
      char *file;

      if (asprintf (&file, "%s/%s", dirname, dent->d_name) < 0)
	{
	  dbg_printf ("cannot allocate space for configuration filename\n");
	  return 0;
	}

      if (stat (file, &st) != 0)
	{
	  dbg_printf ("cannot stat file configuration file\n");
	  continue;
	}


      if (S_ISREG(st.st_mode))
	{
	  found++;
	  rc += load_conffile (file, nextp);
	}

      free (file);
    }

  closedir (dir);

  /* An empty directory is acceptable.
   */
  return (found ? rc : 1);
}

/* INIT -- Initialize syslogd from configuration table.  */
void
init (int signo _GL_UNUSED_PARAMETER)
{
  int rc, ret;
  struct filed *f, *next, **nextp;

  dbg_printf ("init\n");

  /* Close all open log files.  */
  Initialized = 0;
  for (f = Files; f != NULL; f = next)
    {
      int j;

      /* Flush any pending output.  */
      if (f->f_prevcount)
	fprintlog (f, LocalHostName, 0, (char *) NULL);

      switch (f->f_type)
	{
	case F_FILE:
	case F_TTY:
	case F_CONSOLE:
	case F_PIPE:
	  free (f->f_un.f_fname);
	  close (f->f_file);
	  break;
	case F_FORW:
	case F_FORW_SUSP:
	case F_FORW_UNKN:
	  free (f->f_un.f_forw.f_hname);
	  break;
	case F_USERS:
	  for (j = 0; j < f->f_un.f_user.f_nusers; ++j)
	    free (f->f_un.f_user.f_unames[j]);
	  free (f->f_un.f_user.f_unames);
	  break;
	}
      free (f->f_progname);
      free (f->f_prevhost);
      next = f->f_next;
      free (f);
    }

  Files = NULL;		/* Empty the table.  */
  nextp = &Files;
  facilities_seen = 0;

  rc = load_conffile (ConfFile, nextp);

  ret = load_confdir (ConfDir, nextp);
  if (!ret)
    rc = 0;		/* Some allocation errors were found.  */

  Initialized = 1;

  if (Debug)
    {
      for (f = Files; f; f = f->f_next)
	{
	  int i;
	  for (i = 0; i <= LOG_NFACILITIES; i++)
	    if (f->f_pmask[i] == 0)
	      dbg_printf (" X ");
	    else
	      dbg_printf ("%2x ", f->f_pmask[i]);
	  dbg_printf ("%s: ", TypeNames[f->f_type]);
	  switch (f->f_type)
	    {
	    case F_FILE:
	    case F_TTY:
	    case F_CONSOLE:
	    case F_PIPE:
	      dbg_printf ("%s", f->f_un.f_fname);
	      break;

	    case F_FORW:
	    case F_FORW_SUSP:
	    case F_FORW_UNKN:
	      dbg_printf ("%s", f->f_un.f_forw.f_hname);
	      break;

	    case F_USERS:
	      for (i = 0; i < f->f_un.f_user.f_nusers; i++)
		dbg_printf ("%s, ", f->f_un.f_user.f_unames[i]);
	      break;
	    }
	  dbg_printf ("\n");
	}
    }

  if (AcceptRemote)
    logmsg (LOG_SYSLOG | LOG_INFO, "syslogd (" PACKAGE_NAME
	    " " PACKAGE_VERSION "): restart (remote reception)",
	    LocalHostName, ADDDATE);
  else
    logmsg (LOG_SYSLOG | LOG_INFO, "syslogd (" PACKAGE_NAME
	    " " PACKAGE_VERSION "): restart", LocalHostName, ADDDATE);

  if (!rc)
    logmsg (LOG_SYSLOG | LOG_ERR, "syslogd: Incomplete configuration.",
	    LocalHostName, ADDDATE);

  dbg_printf ("syslogd: restarted\n");
}

/* Crack a configuration file line.  */
void
cfline (const char *line, struct filed *f)
{
  struct addrinfo hints, *rp;
  int i, pri, negate_pri, excl_pri, err;
  unsigned int pri_set, pri_clear;
  char *bp;
  const char *p, *q;
  char buf[MAXLINE], ebuf[200];

  dbg_printf ("cfline(%s)%s%s\n", line,
	      selector ? " tagged " : "",
	      selector ? selector : "");

  errno = 0;	/* keep strerror() stuff out of logerror messages */

  /* Clear out file entry.  */
  memset (f, 0, sizeof (*f));
  for (i = 0; i <= LOG_NFACILITIES; i++)
    {
      f->f_pmask[i] = 0;
      f->f_flags = 0;
    }

  /* Scan through the list of selectors.  */
  for (p = line; *p && *p != '\t' && *p != ' ';)
    {

      /* Find the end of this facility name list.  */
      for (q = p; *q && *q != '\t' && *q++ != '.';)
	continue;

      /* Collect priority name.  */
      for (bp = buf; *q && !strchr ("\t ,;", *q);)
	*bp++ = *q++;
      *bp = '\0';

      /* Skip cruft.  */
      while (*q && strchr (",;", *q))
	q++;

      bp = buf;
      negate_pri = excl_pri = 0;

      while (*bp == '!' || *bp == '=')
	switch (*bp++)
	  {
	  case '!':
	    negate_pri = 1;
	    break;

	  case '=':
	    excl_pri = 1;
	    break;
	  }

      /* Decode priority name and set up bit masks.  */
      if (*bp == '*')
	{
	  pri_clear = 0;
	  pri_set = LOG_UPTO (LOG_PRIMASK);
	}
      else
	{
	  pri = decode (bp, prioritynames);
	  if (pri < 0 || (pri > LOG_PRIMASK && pri != INTERNAL_NOPRI))
	    {
	      snprintf (ebuf, sizeof (ebuf),
			"unknown priority name \"%s\"", bp);
	      logerror (ebuf);
	      return;
	    }
	  if (pri == INTERNAL_NOPRI)
	    {
	      pri_clear = 255;
	      pri_set = 0;
	    }
	  else
	    {
	      pri_clear = 0;
	      pri_set = excl_pri ? LOG_MASK (pri) : LOG_UPTO (pri);
	    }
	}
      if (negate_pri)
	{
	  unsigned int exchange = pri_set;
	  pri_set = pri_clear;
	  pri_clear = exchange;
	}

      /* Scan facilities.  */
      while (*p && !strchr ("\t .;", *p))
	{
	  for (bp = buf; *p && !strchr ("\t ,;.", *p);)
	    *bp++ = *p++;
	  *bp = '\0';
	  if (*buf == '*')
	    for (i = 0; i <= LOG_NFACILITIES; i++)
	      {
		/* make "**" act as a wildcard only for facilities not
		 * specified elsewhere
		 */
		if (buf[1] == '*' && ((1 << i) & facilities_seen))
		  continue;

		f->f_pmask[i] &= ~pri_clear;
		f->f_pmask[i] |= pri_set;
	      }
	  else
	    {
	      i = decode (buf, facilitynames);

	      if (i < 0 || i > (LOG_NFACILITIES << 3))
		{
		  snprintf (ebuf, sizeof (ebuf),
			    "unknown facility name \"%s\"", buf);
		  logerror (ebuf);
		  return;
		}

	      f->f_pmask[LOG_FAC (i)] &= ~pri_clear;
	      f->f_pmask[LOG_FAC (i)] |= pri_set;

	      facilities_seen |= (1 << LOG_FAC (i));
	    }
	  while (*p == ',' || *p == ' ')
	    p++;
	}
      p = q;
    }

  /* Skip to action part.  */
  while (*p == '\t' || *p == ' ')
    p++;

  if (*p == '-')
    {
      f->f_flags |= OMIT_SYNC;
      p++;
    }

  if (!strlen(p))
    {
      /* Invalidate an entry with empty action field.  */
      f->f_type = F_UNUSED;
      logerror ("empty action field");
      return;
    }

  switch (*p)
    {
    case '@':
      f->f_un.f_forw.f_hname = strdup (++p);
      memset (&hints, 0, sizeof (hints));
      hints.ai_family = usefamily;
      hints.ai_socktype = SOCK_DGRAM;
#ifdef AI_ADDRCONFIG
      if (usefamily == AF_UNSPEC)
	hints.ai_flags |= AI_ADDRCONFIG;
#endif

      f->f_un.f_forw.f_addrlen = 0;	/* Invalidate address.  */
      memset (&f->f_un.f_forw.f_addr, 0, sizeof (f->f_un.f_forw.f_addr));

      err = getaddrinfo (p, LogForwardPort, &hints, &rp);
      if (err)
	{
	  switch (err)
	    {
	      case EAI_AGAIN:	/* Known kinds of temporary error.  */
	      case EAI_MEMORY:
		f->f_type = F_FORW_UNKN;
		f->f_prevcount = INET_RETRY_MAX;
		break;

	      case EAI_NONAME:	/* The most probable causes for failure.  */
#if defined EAI_NODATA && (EAI_NODATA != EAI_NONAME)	/* FreeBSD complains.  */
	      case EAI_NODATA:
#endif
#ifdef EAI_ADDRFAMILY
	      case EAI_ADDRFAMILY:
#endif
	      default:		/* Catch system exceptions.  */
		f->f_type = F_UNUSED;
	    }

	  f->f_time = time ((time_t *) 0);
	}
      else
	{
	  f->f_type = F_FORW;
	  f->f_un.f_forw.f_addrlen = rp->ai_addrlen;
	  memcpy (&f->f_un.f_forw.f_addr, rp->ai_addr, rp->ai_addrlen);
	  freeaddrinfo (rp);
	}
      break;

    case '|':
      f->f_un.f_fname = strdup (p);
      f->f_file = open (++p, O_RDWR | O_NONBLOCK);
      if (f->f_file < 0)
	{
	  f->f_type = F_UNUSED;
	  logerror (p);
	  free (f->f_un.f_fname);
	  f->f_un.f_fname = NULL;
	  break;
	}
      if (strcmp (p, ctty) == 0)
	f->f_type = F_CONSOLE;
      else if (isatty (f->f_file))
	f->f_type = F_TTY;
      else
	f->f_type = F_PIPE;
      break;

    case '/':
      f->f_un.f_fname = strdup (p);
      f->f_file = open (p, O_WRONLY | O_APPEND | O_CREAT, 0644);
      if (f->f_file < 0)
	{
	  f->f_type = F_UNUSED;
	  logerror (p);
	  free (f->f_un.f_fname);
	  f->f_un.f_fname = NULL;
	  break;
	}
      if (strcmp (p, ctty) == 0)
	f->f_type = F_CONSOLE;
      else if (isatty (f->f_file))
	f->f_type = F_TTY;
      else
	f->f_type = F_FILE;
      break;

    case '*':
      f->f_type = F_WALL;
      break;

    default:
      f->f_un.f_user.f_nusers = 1;
      for (q = p; *q; q++)
	if (*q == ',')
	  f->f_un.f_user.f_nusers++;
      f->f_un.f_user.f_unames =
	(char **) malloc (f->f_un.f_user.f_nusers * sizeof (char *));
      for (i = 0; *p; i++)
	{
	  for (q = p; *q && *q != ',';)
	    q++;
	  f->f_un.f_user.f_unames[i] = malloc (q - p + 1);
	  if (f->f_un.f_user.f_unames[i])
	    {
	      strncpy (f->f_un.f_user.f_unames[i], p, q - p);
	      f->f_un.f_user.f_unames[i][q - p] = '\0';
	    }
	  while (*q == ',' || *q == ' ')
	    q++;
	  p = q;
	}
      f->f_type = F_USERS;
      break;
    }

    /* Set program selector.  */
    if (selector)
      {
	f->f_progname = strdup (selector);
	f->f_prognlen = strlen (selector);
      }
    else
      f->f_progname = NULL;
}

/* Decode a symbolic name to a numeric value.  */
int
decode (const char *name, CODE * codetab)
{
  CODE *c;

  if (isdigit (*name))
    return atoi (name);

  for (c = codetab; c->c_name; c++)
    if (!strcasecmp (name, c->c_name))
      return c->c_val;

  return -1;
}

void
dbg_toggle (int signo _GL_UNUSED_PARAMETER)
{
  int dbg_save = dbg_output;

  dbg_output = 1;
  dbg_printf ("Switching dbg_output to %s.\n",
	      dbg_save == 0 ? "true" : "false");
  dbg_output = (dbg_save == 0) ? 1 : 0;

#ifndef HAVE_SIGACTION
  signal (SIGUSR1, dbg_toggle);
#endif
}

/* Ansi2knr will always change ... to va_list va_dcl */
static void
dbg_printf (const char *fmt, ...)
{
  va_list ap;

  if (!(NoDetach && dbg_output))
    return;

  va_start (ap, fmt);
  vfprintf (stdout, fmt, ap);
  va_end (ap);

  fflush (stdout);
}

/* The following function is resposible for handling a SIGHUP signal.
   Since we are now doing mallocs/free as part of init we had better
   not being doing this during a signal handler.  Instead we simply
   set a flag variable which will tell the main loop to go through a
   restart.  */
void
trigger_restart (int signo _GL_UNUSED_PARAMETER)
{
  restart = 1;
#ifndef HAVE_SIGACTION
  signal (SIGHUP, trigger_restart);
#endif
}

/* Override default port with a non-NULL argument.
 * Otherwise identify the default syslog/udp with
 * proper fallback to avoid resolve issues.  */
void
find_inet_port (const char *port)
{
  int err;
  struct addrinfo hints, *ai;

  /* Fall back to numerical description.  */
#ifdef IPPORT_SYSLOG
  snprintf (portstr, sizeof (portstr), "%u", IPPORT_SYSLOG);
  LogForwardPort = portstr;
#else
  LogForwardPort = "514";
#endif

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  err = getaddrinfo (NULL, "syslog", &hints, &ai);
  if (err == 0)
    {
      LogForwardPort = "syslog";	/* Symbolic name is usable.  */
      freeaddrinfo (ai);
    }

  LogPortText = (char *) port;

  if (!LogPortText)
    {
      LogPortText = LogForwardPort;
      return;
    }

  /* Is the port specified on command line really usable?  */
  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  err = getaddrinfo (NULL, LogPortText, &hints, &ai);
  if (err != 0)
    {
      /* Not usable, disable listener.
       * It is too early to report failure at this time.  */
      LogPortText = NULL;
    }
  else
    freeaddrinfo (ai);

  return;
}
