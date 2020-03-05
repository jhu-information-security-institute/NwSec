/*
  Copyright (C) 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pwd.h>

#include <argp.h>
#include <libinetutils.h>
#include <progname.h>
#include <ctype.h>
#include <error.h>
#include <xalloc.h>
#include <inttostr.h>
#include <unused-parameter.h>

#define SYSLOG_NAMES
#include <syslog.h>
#ifndef HAVE_SYSLOG_INTERNAL
# include "logprio.h"
#endif

#define MAKE_PRI(fac,pri) (((fac) & LOG_FACMASK) | ((pri) & LOG_PRIMASK))

static char *tag = NULL;
static int logflags = 0;
static int pri = MAKE_PRI (LOG_USER, LOG_NOTICE);  /* Cf. parse_level */
/* Only one of `host' and `unixsock' will be non-NULL
 * once option parsing has been completed. */
static char *host = PATH_LOG;
static char *unixsock = NULL;
static char *source;
static char *pidstr;

#if HAVE_DECL_GETADDRINFO
# if HAVE_IPV6
static int host_family = AF_UNSPEC;
# else
/* Fall back to only IPv4.  */
static int host_family = AF_INET;
# endif /* !HAVE_IPV6 */
#endif /* HAVE_DECL_GETADDRINFO */



int
decode (char *name, CODE *codetab, const char *what)
{
  CODE *cp;

  if (isdigit (*name))
    {
      char *p;
      unsigned long n = strtoul (name, &p, 0);

      /* For portability reasons, a numerical facility is entered
       * as a decimal integer, shifted left by three binary bits.
       * Any overflow check must adapt to this fact.
       * For the purpose of remote logging from a local system,
       * tests based on LOG_NFACILITIES are insufficient, as a
       * remote system may well distinguish more facilities than
       * the local system does!
       */
      if (*p || n > LOG_FACMASK)	/* Includes range errors.  */
	error (EXIT_FAILURE, 0, "invalid %s number: %s", what, name);

      return n;
    }

  for (cp = codetab; cp->c_name; cp++)
    {
      if (strcasecmp (name, cp->c_name) == 0)
	return cp->c_val;
    }
  error (EXIT_FAILURE, 0, "unknown %s name: %s", what, name);
  return -1; /* to pacify gcc */
}

int
parse_level (char *str)
{
  char *p;
  int fac, prio = LOG_NOTICE;	/* Default priority!  */

  p = strchr (str, '.');
  if (p)
    *p++ = 0;

  fac = decode (str, facilitynames, "facility");
  if (p)
    prio = decode (p, prioritynames, "priority");

  return MAKE_PRI (fac, prio);
}


union logger_sockaddr
  {
    struct sockaddr sa;
    struct sockaddr_in sinet;
#if HAVE_IPV6
    struct sockaddr_in6 sinet6;
#endif
    struct sockaddr_un sunix;
};

int fd;

static void
open_socket (void)
{
  union logger_sockaddr sockaddr;
  socklen_t socklen;
  int family;
#if HAVE_DECL_GETADDRINFO
  int ret;
#endif

  /* A UNIX socket name can be specified in two ways.
   * Zero length of `unixsock' is handled automatically.  */
  if ((host != NULL && strchr (host, '/')) || unixsock != NULL)
    {
      size_t len;

      /* Copy `unixsock' to `host' if necessary.
       * There is no need to differentiate them.  */
      if (unixsock)
	host = unixsock;
      len = strlen (host);
      if (len >= sizeof sockaddr.sunix.sun_path)
	error (EXIT_FAILURE, 0, "UNIX socket name too long");
      strcpy (sockaddr.sunix.sun_path, host);
      sockaddr.sunix.sun_family = AF_UNIX;
      family = PF_UNIX;
      socklen = sizeof (sockaddr.sunix);
    }
  else
    {
#if HAVE_DECL_GETADDRINFO
      struct addrinfo hints, *ai, *res;
#else
      struct hostent *hp;
      struct servent *sp;
      unsigned short port;
#endif /* !HAVE_DECL_GETADDRINFO */
      char *p;

#if HAVE_IPV6
      /* Bare, numeric IPv6 addresses must be contained
       * in brackets in order that an appended port not
       * be read by mistake.  */
      if (*host == '[')
	{
	  ++host;
	  p = strchr (host, ']');
	  if (p)
	    {
	      *p++ = '\0';
	      if (*p == ':')
		++p;
	      else
		p = NULL;
	    }
	}
      else
        {
	  /* When no bracket was detected, then seek the
	   * right-most colon character in order to correctly
	   * parse IPv6 addresses.  */
	  p = strrchr (host, ':');
	  if (p)
	    *p++ = 0;
	}
#else /* !HAVE_IPV6 */
      p = strchr (host, ':');
      if (p)
	*p++ = 0;
#endif /* !HAVE_IPV6 */

      if (!p)
	p = "syslog";

#if HAVE_DECL_GETADDRINFO
      memset (&hints, 0, sizeof (hints));
      hints.ai_socktype = SOCK_DGRAM;

      /* This falls back to AF_INET if compilation
       * was made with !HAVE_IPV6.  */
      hints.ai_family = host_family;

      /* The complete handshake is attempted within
       * a single while-loop, since the answers from
       * getaddrinfo() need to be checked in detail.  */
      ret = getaddrinfo (host, p, &hints, &res);
      if (ret < 0)
	error (EXIT_FAILURE, 0, "%s:%s, %s", host, p, gai_strerror(ret));

      for (ai = res; ai; ai = ai->ai_next)
        {
	  fd = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	  if (fd < 0)
	    continue;

	  if (source)
	    {
	      /* Resolver uses the same address family
	       * as the already resolved target host.
	       */
	      int ret;
	      struct addrinfo tips, *a;

	      memset (&tips, 0, sizeof (tips));
	      tips.ai_family = ai->ai_family;

	      ret = getaddrinfo(source, NULL, &tips, &a);
	      if (ret)
		{
		  close (fd);
		  continue;
		}

	      if (bind (fd, a->ai_addr, a->ai_addrlen))
		{
		  freeaddrinfo (a);
		  close (fd);
		  continue;
		}

	      freeaddrinfo (a);
	    }

	  if (connect (fd, ai->ai_addr, ai->ai_addrlen))
	    {
	      close (fd);
	      continue;
	    }

	  /* Socket standing, bound and connected.  */
	  break;
	}

      if (res)
	freeaddrinfo (res);

      if (ai == NULL)
	error (EXIT_FAILURE, EADDRNOTAVAIL, "%s:%s", host, p);

      /* Existing socket can be returned now.
       * This handles AF_INET and AF_INET6 in case
       * HAVE_DECL_GETADDRINFO is true.  */
      return;

#else /* !HAVE_DECL_GETADDRINFO */

      sockaddr.sinet.sin_family = AF_INET;
      family = PF_INET;

      hp = gethostbyname (host);
      if (hp)
	sockaddr.sinet.sin_addr.s_addr = *(unsigned long*) hp->h_addr_list[0];
      else if (inet_aton (host, (struct in_addr *) &sockaddr.sinet.sin_addr)
	       != 1)
	error (EXIT_FAILURE, 0, "unknown host name");

      if (isdigit (*p))
	{
	  char *end;
	  unsigned long n = strtoul (p, &end, 10);
	  if (*end || (port = n) != n)
	    error (EXIT_FAILURE, 0, "%s: invalid port number", p);
	  port = htons (port);
	}
      else if ((sp = getservbyname (p, "udp")) != NULL)
	port = sp->s_port;
      else
	error (EXIT_FAILURE, 0, "%s: unknown service name", p);

      sockaddr.sinet.sin_port = port;
#endif /* !HAVE_DECL_GETADDRINFO */

      socklen = sizeof (sockaddr.sinet);
    }

  /* Execution arrives here for AF_UNIX and for
   * situations with !HAVE_DECL_GETADDRINFO.  */

  fd = socket (family, SOCK_DGRAM, 0);
  if (fd < 0)
    error (EXIT_FAILURE, errno, "cannot create socket");

  if (family == PF_INET)
    {
      struct sockaddr_in s;
      s.sin_family = AF_INET;

      if (source)
	{
	  if (inet_aton (source, (struct in_addr *) &s.sin_addr) != 1)
	    error (EXIT_FAILURE, 0, "invalid source address");
	}
      else
	s.sin_addr.s_addr = INADDR_ANY;
      s.sin_port = 0;

      if (bind(fd, (struct sockaddr*) &s, sizeof(s)) < 0)
	error (EXIT_FAILURE, errno, "cannot bind to source address");
    }

  if (connect (fd, &sockaddr.sa, socklen))
    error (EXIT_FAILURE, errno, "cannot connect");
}


static void
send_to_syslog (const char *msg)
{
  char *pbuf;
  time_t now = time (NULL);
  size_t len;
  ssize_t rc;

  if (logflags & LOG_PID)
    rc = asprintf (&pbuf, "<%d>%.15s %s[%s]: %s",
		   pri, ctime (&now) + 4, tag, pidstr, msg);
  else
    rc = asprintf (&pbuf, "<%d>%.15s %s: %s",
		   pri, ctime (&now) + 4, tag, msg);
  if (rc == -1)
    error (EXIT_FAILURE, errno, "cannot format message");
  len = strlen (pbuf);

#ifdef LOG_PERROR
  if (logflags & LOG_PERROR)
    {
      struct iovec iov[2], *ioptr;
      size_t msglen = strlen (msg);

      ioptr = iov;
      ioptr->iov_base = (char*) msg;
      ioptr->iov_len = msglen;

      if (msg[msglen - 1] != '\n')
	{
	  /* provide a newline */
	  ioptr++;
	  ioptr->iov_base = (char *) "\n";
	  ioptr->iov_len = 1;
	}
      writev (fileno (stderr), iov, ioptr - iov + 1);
    }
#endif /* LOG_PERROR */

  rc = send (fd, pbuf, len, 0);
  free (pbuf);
  if (rc == -1)
    error (0, errno, "send failed");
  else if (rc != (ssize_t) len)
    error (0, errno, "sent less bytes than expected (%lu vs. %lu)",
	   (unsigned long) rc, (unsigned long) len);
}


const char args_doc[] = "[MESSAGE]";
const char doc[] = "Send messages to syslog";

static struct argp_option argp_options[] = {
#define GRP 10
  {"ipv4", '4', NULL, 0, "use IPv4 for logging to host", GRP },
  {"ipv6", '6', NULL, 0, "use IPv6 with a host target", GRP },
  { "host", 'h', "HOST", 0,
    "log to HOST instead of to the default " PATH_LOG, GRP },
  { "unix", 'u', "SOCK", 0,
    "log to UNIX socket SOCK instead of " PATH_LOG, GRP },
  { "source", 'S', "IP", 0,
    "set source IP address", GRP },
  { "id", 'i', "PID", OPTION_ARG_OPTIONAL,
    "log the process id with every line", GRP },
#ifdef LOG_PERROR
  { "stderr", 's', NULL, 0, "copy the message to stderr", GRP },
#endif
  { "file", 'f', "FILE", 0, "log the content of FILE", GRP },
  { "priority", 'p', "PRI", 0, "log with priority PRI", GRP },
  { "tag", 't', "TAG", 0, "prepend every line with TAG", GRP },
#undef GRP
  {NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state _GL_UNUSED_PARAMETER)
{
  switch (key)
    {
    case '4':
      host_family = AF_INET;
      break;

    case '6':
#if HAVE_IPV6
      host_family = AF_INET6;
      break;

#else /* !HAVE_IPV6 */
      /* Print a warning but continue with IPv4.  */
      error (0, 0, "warning: Falling back to IPv4, "
		"since IPv6 is disabled");
      /* AF_INET is set by default in this case.  */
      break;
#endif /* !HAVE_IPV6 */

    case 'h':
      host = arg;
      unixsock = NULL;	/* Erase any previous `-u'.  */
      break;

    case 'u':
      unixsock = arg;
      host = NULL;	/* Erase previous `-h'.  */
      break;

    case 'S':
      source = arg;
      break;

    case 'i':
      logflags |= LOG_PID;
      if (arg)
	pidstr = arg;
      else
	{
	  char buf[INT_BUFSIZE_BOUND (uintmax_t)];
	  arg = umaxtostr (getpid (), buf);
	  pidstr = xstrdup (arg);
	}
      break;

#ifdef LOG_PERROR
    case 's':
      logflags |= LOG_PERROR;
      break;
#endif /* LOG_PERROR */

    case 'f':
      if (strcmp (arg, "-") && freopen (arg, "r", stdin) == NULL)
        error (EXIT_FAILURE, errno, "%s", arg);
      break;

    case 'p':
      pri = parse_level (arg);
      break;

    case 't':
      tag = arg;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp =
  {argp_options, parse_opt, args_doc, doc, NULL, NULL, NULL};

const char *program_authors[] = {
  "Sergey Poznyakoff",
  NULL
};

int
main (int argc, char *argv[])
{
  int index;
  char *buf = NULL;

  set_program_name (argv[0]);
  iu_argp_init ("logger", program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  argc -= index;
  argv += index;

  if (!tag)
    {
      tag = getenv ("USER");
      if (!tag)
	{
	  struct passwd *pw = getpwuid (getuid ());
	  if (pw)
	    tag = xstrdup (pw->pw_name);
	  else
	    tag = xstrdup ("none");
	}
    }

  open_socket ();

  if (argc > 0)
    {
      int i;
      size_t len = 0;
      char *p;

      for (i = 0; i < argc; i++)
	len += strlen (argv[i]) + 1;

      buf = xmalloc (len);
      for (i = 0, p = buf; i < argc; i++)
	{
	  len = strlen (argv[i]);
	  memcpy (p, argv[i], len);
	  p += len;
	  *p++ = ' ';
	}
      p[-1] = 0;

      send_to_syslog (buf);
    }
  else
    {
      size_t size = 0;

      while (getline (&buf, &size, stdin) > 0)
	send_to_syslog (buf);
    }
  free (buf);
  exit (EXIT_SUCCESS);
}
