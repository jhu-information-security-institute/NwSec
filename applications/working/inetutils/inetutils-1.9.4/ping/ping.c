/*
  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
  2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <signal.h>

#include <netinet/in.h>

/*#include <netinet/ip_icmp.h>  -- deliberately not including this */
#ifdef HAVE_NETINET_IP_VAR_H
# include <netinet/ip_var.h>
#endif

#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include <argp.h>
#include <unused-parameter.h>
#include <ping.h>
#include "ping_impl.h"
#include "libinetutils.h"

extern int ping_echo (char *hostname);
extern int ping_timestamp (char *hostname);
extern int ping_address (char *hostname);
extern int ping_router (char *hostname);

PING *ping;
bool is_root = false;
unsigned char *data_buffer;
unsigned char *patptr;
int pattern_len = MAXPATTERN;
int socket_type;
size_t count = DEFAULT_PING_COUNT;
size_t interval;
size_t data_length = PING_DATALEN;
unsigned options;
unsigned int suboptions;
unsigned long preload = 0;
int tos = -1;		/* Triggers with non-negative values.  */
int ttl = 0;
int timeout = -1;
int linger = MAXWAIT;
int (*ping_type) (char *hostname) = ping_echo;

int (*decode_type (const char *arg)) (char *hostname);
static int decode_ip_timestamp (char *arg);
static int send_echo (PING * ping);

#define MIN_USER_INTERVAL (200000/PING_PRECISION)

const char args_doc[] = "HOST ...";
const char doc[] = "Send ICMP ECHO_REQUEST packets to network hosts."
                   "\vOptions marked with (root only) are available only to "
                   "superuser.";
const char *program_authors[] = {
	"Sergey Poznyakoff",
	NULL
};

/* Define keys for long options that do not have short counterparts. */
enum {
  ARG_ECHO = 256,
  ARG_ADDRESS,
  ARG_TIMESTAMP,
  ARG_ROUTERDISCOVERY,
  ARG_TTL,
  ARG_IPTIMESTAMP,
};

static struct argp_option argp_options[] = {
#define GRP 0
  {NULL, 0, NULL, 0, "Options controlling ICMP request types:", GRP},
  {"address", ARG_ADDRESS, NULL, 0, "send ICMP_ADDRESS packets (root only)",
   GRP+1},
  {"echo", ARG_ECHO, NULL, 0, "send ICMP_ECHO packets (default)", GRP+1},
  {"mask", ARG_ADDRESS, NULL, 0, "same as --address", GRP+1},
  {"timestamp", ARG_TIMESTAMP, NULL, 0, "send ICMP_TIMESTAMP packets", GRP+1},
  {"type", 't', "TYPE", 0, "send TYPE packets", GRP+1},
  /* This option is not yet fully implemented, so mark it as hidden. */
  {"router", ARG_ROUTERDISCOVERY, NULL, OPTION_HIDDEN, "send "
   "ICMP_ROUTERDISCOVERY packets (root only)", GRP+1},
#undef GRP
#define GRP 10
  {NULL, 0, NULL, 0, "Options valid for all request types:", GRP},
  {"count", 'c', "NUMBER", 0, "stop after sending NUMBER packets", GRP+1},
  {"debug", 'd', NULL, 0, "set the SO_DEBUG option", GRP+1},
  {"interval", 'i', "NUMBER", 0, "wait NUMBER seconds between sending each "
   "packet", GRP+1},
  {"numeric", 'n', NULL, 0, "do not resolve host addresses", GRP+1},
  {"ignore-routing", 'r', NULL, 0, "send directly to a host on an attached "
   "network", GRP+1},
  {"tos", 'T', "NUM", 0, "set type of service (TOS) to NUM", GRP+1},
  {"ttl", ARG_TTL, "N", 0, "specify N as time-to-live", GRP+1},
  {"verbose", 'v', NULL, 0, "verbose output", GRP+1},
  {"timeout", 'w', "N", 0, "stop after N seconds", GRP+1},
  {"linger", 'W', "N", 0, "number of seconds to wait for response", GRP+1},
#undef GRP
#define GRP 20
  {NULL, 0, NULL, 0, "Options valid for --echo requests:", GRP},
  {"flood", 'f', NULL, 0, "flood ping (root only)", GRP+1},
  {"preload", 'l', "NUMBER", 0, "send NUMBER packets as fast as possible "
   "before falling into normal mode of behavior (root only)", GRP+1},
  {"pattern", 'p', "PATTERN", 0, "fill ICMP packet with given pattern (hex)",
   GRP+1},
  {"quiet", 'q', NULL, 0, "quiet output", GRP+1},
  {"route", 'R', NULL, 0, "record route", GRP+1},
  {"ip-timestamp", ARG_IPTIMESTAMP, "FLAG", 0, "IP timestamp of type FLAG, "
   "which is one of \"tsonly\" and \"tsaddr\"", GRP+1},
  {"size", 's', "NUMBER", 0, "send NUMBER data octets", GRP+1},
#undef GRP
  {NULL, 0, NULL, 0, NULL, 0}
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  char *endptr;
  static unsigned char pattern[MAXPATTERN];
  double v;

  switch (key)
    {
    case 'c':
      count = ping_cvt_number (arg, 0, 1);
      break;

    case 'd':
      socket_type |= SO_DEBUG;
      break;

    case 'i':
      v = strtod (arg, &endptr);
      if (*endptr)
        argp_error (state, "invalid value (`%s' near `%s')", arg, endptr);
      options |= OPT_INTERVAL;
      interval = v * PING_PRECISION;
      if (!is_root && interval < MIN_USER_INTERVAL)
        error (EXIT_FAILURE, 0, "option value too small: %s", arg);
      break;

    case 'r':
      socket_type |= SO_DONTROUTE;
      break;

    case 's':
      data_length = ping_cvt_number (arg, PING_MAX_DATALEN, 1);
      break;

    case 'n':
      options |= OPT_NUMERIC;
      break;

    case 'p':
      decode_pattern (arg, &pattern_len, pattern);
      patptr = pattern;
      break;

    case 'q':
      options |= OPT_QUIET;
      break;

    case 'T':
      tos = ping_cvt_number (arg, 255, 1);
      break;

    case 'w':
      timeout = ping_cvt_number (arg, INT_MAX, 0);
      break;

    case 'R':
      options |= OPT_RROUTE;
      break;

    case 'W':
      linger = ping_cvt_number (arg, INT_MAX, 0);
      break;

    case 'v':
      options |= OPT_VERBOSE;
      break;

    case 'l':
      preload = strtoul (arg, &endptr, 0);
      if (*endptr || preload > INT_MAX)
        error (EXIT_FAILURE, 0, "invalid preload value (%s)", arg);
      break;

    case 'f':
      options |= OPT_FLOOD;
      break;

    case 't':
      ping_type = decode_type (arg);
      break;

    case ARG_ECHO:
      ping_type = decode_type ("echo");
      break;

    case ARG_TIMESTAMP:
      ping_type = decode_type ("timestamp");
      break;

    case ARG_ADDRESS:
      ping_type = decode_type ("address");
      break;

    case ARG_ROUTERDISCOVERY:
      ping_type = decode_type ("router");
      break;

    case ARG_TTL:
      ttl = ping_cvt_number (arg, 255, 0);
      break;

    case ARG_IPTIMESTAMP:
      options |= OPT_IPTIMESTAMP;
      suboptions |= decode_ip_timestamp (arg);
      break;

    case ARGP_KEY_NO_ARGS:
      argp_error (state, "missing host operand");

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp =
  {argp_options, parse_opt, args_doc, doc, NULL, NULL, NULL};

int
main (int argc, char **argv)
{
  int index;
  int one = 1;
  int status = 0;

  set_program_name (argv[0]);

# ifdef HAVE_SETLOCALE
  setlocale(LC_ALL, "");
# endif

  if (getuid () == 0)
    is_root = true;

  /* Parse command line */
  iu_argp_init ("ping", program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  ping = ping_init (ICMP_ECHO, getpid ());
  if (ping == NULL)
    /* ping_init() prints our error message.  */
    exit (EXIT_FAILURE);

  ping_set_sockopt (ping, SO_BROADCAST, (char *) &one, sizeof (one));

  /* Reset root privileges */
  setuid (getuid ());

  /* Force line buffering regardless of output device.  */
  setvbuf (stdout, NULL, _IOLBF, 0);

  argv += index;
  argc -= index;

  if (count != 0)
    ping_set_count (ping, count);

  if (socket_type != 0)
    ping_set_sockopt (ping, socket_type, &one, sizeof (one));

  if (options & OPT_INTERVAL)
    ping_set_interval (ping, interval);

  if (ttl > 0)
    if (setsockopt (ping->ping_fd, IPPROTO_IP, IP_TTL,
		    &ttl, sizeof (ttl)) < 0)
      error (0, errno, "setsockopt(IP_TTL)");

  if (tos >= 0)
    if (setsockopt (ping->ping_fd, IPPROTO_IP, IP_TOS,
		    &tos, sizeof (tos)) < 0)
      error (0, errno, "setsockopt(IP_TOS)");

  init_data_buffer (patptr, pattern_len);

  while (argc--)
    {
      status |= (*(ping_type)) (*argv++);
      ping_reset (ping);
    }

  free (ping);
  free (data_buffer);
  return status;
}

int (*decode_type (const char *arg)) (char *hostname)
{
  int (*ping_type) (char *hostname);

  if (strcasecmp (arg, "echo") == 0)
    ping_type = ping_echo;
  else if (strcasecmp (arg, "timestamp") == 0)
    ping_type = ping_timestamp;
  else if (strcasecmp (arg, "address") == 0)
    ping_type = ping_address;
  else if (strcasecmp (arg, "mask") == 0)
    ping_type = ping_address;
#if 0
  else if (strcasecmp (arg, "router") == 0)
    ping_type = ping_router;
#endif
  else
    error (EXIT_FAILURE, 0, "unsupported packet type: %s", arg);

 return ping_type;
}

int
decode_ip_timestamp (char *arg)
{
  int sopt = 0;

  if (strcasecmp (arg, "tsonly") == 0)
    sopt = SOPT_TSONLY;
  else if (strcasecmp (arg, "tsaddr") == 0)
    sopt = SOPT_TSADDR;
#if 0	/* Not yet implemented.  */
  else if (strcasecmp (arg, "prespec") == 0)
    sopt = SOPT_TSPRESPEC;
#endif
  else
    error (EXIT_FAILURE, 0, "unsupported timestamp type: %s", arg);

  return sopt;
}

int volatile stop = 0;

void
sig_int (int signal _GL_UNUSED_PARAMETER)
{
  stop = 1;
}

int
ping_run (PING * ping, int (*finish) ())
{
  fd_set fdset;
  int fdmax;
  struct timeval resp_time;
  struct timeval last, intvl, now;
  struct timeval *t = NULL;
  int finishing = 0;
  size_t nresp = 0;
  size_t i;

  signal (SIGINT, sig_int);

  fdmax = ping->ping_fd + 1;

  /* Some systems use `struct timeval' of size 16.  As these are
   * not initialising `timeval' properly by assignment alone, let
   * us play safely here.  gettimeofday() is always sufficient.
   */
  memset (&resp_time, 0, sizeof (resp_time));
  memset (&intvl, 0, sizeof (intvl));
  memset (&now, 0, sizeof (now));

  for (i = 0; i < preload; i++)
    send_echo (ping);

  if (options & OPT_FLOOD)
    {
      intvl.tv_sec = 0;
      intvl.tv_usec = 10000;
    }
  else
    PING_SET_INTERVAL (intvl, ping->ping_interval);

  gettimeofday (&last, NULL);
  send_echo (ping);

  while (!stop)
    {
      int n;

      FD_ZERO (&fdset);
      FD_SET (ping->ping_fd, &fdset);
      gettimeofday (&now, NULL);
      resp_time.tv_sec = last.tv_sec + intvl.tv_sec - now.tv_sec;
      resp_time.tv_usec = last.tv_usec + intvl.tv_usec - now.tv_usec;

      while (resp_time.tv_usec < 0)
	{
	  resp_time.tv_usec += 1000000;
	  resp_time.tv_sec--;
	}
      while (resp_time.tv_usec >= 1000000)
	{
	  resp_time.tv_usec -= 1000000;
	  resp_time.tv_sec++;
	}

      if (resp_time.tv_sec < 0)
	resp_time.tv_sec = resp_time.tv_usec = 0;

      n = select (fdmax, &fdset, NULL, NULL, &resp_time);
      if (n < 0)
	{
	  if (errno != EINTR)
	    error (EXIT_FAILURE, errno, "select failed");
	  continue;
	}
      else if (n == 1)
	{
	  if (ping_recv (ping) == 0)
	    nresp++;
	  if (t == 0)
	    {
	      gettimeofday (&now, NULL);
	      t = &now;
	    }

	  if (ping_timeout_p (&ping->ping_start_time, timeout))
	    break;

	  if (ping->ping_count && nresp >= ping->ping_count)
	    break;
	}
      else
	{
	  if (!ping->ping_count || ping->ping_num_xmit < ping->ping_count)
	    {
	      send_echo (ping);
	      if (!(options & OPT_QUIET) && options & OPT_FLOOD)
		putchar ('.');

	      if (ping_timeout_p (&ping->ping_start_time, timeout))
		break;
	    }
	  else if (finishing)
	    break;
	  else
	    {
	      finishing = 1;

	      intvl.tv_sec = linger;
	    }
	  gettimeofday (&last, NULL);
	}
    }

  ping_unset_data (ping);

  if (finish)
    return (*finish) ();
  return 0;
}

int
send_echo (PING * ping)
{
  size_t off = 0;
  int rc;

  if (PING_TIMING (data_length))
    {
      struct timeval tv;
      gettimeofday (&tv, NULL);
      ping_set_data (ping, &tv, 0, sizeof (tv), USE_IPV6);
      off += sizeof (tv);
    }
  if (data_buffer)
    ping_set_data (ping, data_buffer, off,
		   data_length > off ? data_length - off : data_length,
		   USE_IPV6);

  rc = ping_xmit (ping);
  if (rc < 0)
    error (EXIT_FAILURE, errno, "sending packet");

  return rc;
}

int
ping_finish (void)
{
  fflush (stdout);
  printf ("--- %s ping statistics ---\n", ping->ping_hostname);
  printf ("%zu packets transmitted, ", ping->ping_num_xmit);
  printf ("%zu packets received, ", ping->ping_num_recv);
  if (ping->ping_num_rept)
    printf ("+%zu duplicates, ", ping->ping_num_rept);
  if (ping->ping_num_xmit)
    {
      if (ping->ping_num_recv > ping->ping_num_xmit)
	printf ("-- somebody is printing forged packets!");
      else
	printf ("%d%% packet loss",
		(int) (((ping->ping_num_xmit - ping->ping_num_recv) * 100) /
		       ping->ping_num_xmit));

    }
  printf ("\n");
  return 0;
}
