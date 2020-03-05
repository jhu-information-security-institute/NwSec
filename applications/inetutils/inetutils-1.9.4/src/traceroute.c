/*
  Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015
  Free Software Foundation, Inc.

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

/* Written by Elian Gidoni.  */

#include <config.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
/* #include <netinet/ip_icmp.h> -- Deliberately not including this
   since the definitions in use are being pulled in by libicmp. */
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
#include <error.h>
#include <progname.h>
#include <limits.h>
#include <assert.h>
#include <argp.h>
#include <unused-parameter.h>
#include <icmp.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif
#ifdef HAVE_IDNA_H
# include <idna.h>
#endif

#include "xalloc.h"
#include "libinetutils.h"

#define TRACE_UDP_PORT 33434
#define TRACE_TTL 1

enum trace_type
{
  TRACE_UDP,			/* UDP datagrams.  */
  TRACE_ICMP,			/* ICMP echo requests.  */
  TRACE_1393			/* RFC 1393 requests. */
};

typedef struct trace
{
  int icmpfd, udpfd;
  enum trace_type type;
  struct sockaddr_in to, from;
  int ttl;
  struct timeval tsent;
} trace_t;

void trace_init (trace_t * t, const struct sockaddr_in to,
		 const enum trace_type type);
void trace_ip_opts (struct sockaddr_in *to);
void trace_inc_ttl (trace_t * t);
void trace_inc_port (trace_t * t);
void trace_port (trace_t * t, const unsigned short port);
int trace_read (trace_t * t, int * type, int * code);
int trace_write (trace_t * t);
int trace_udp_sock (trace_t * t);
int trace_icmp_sock (trace_t * t);

#define TIME_INTERVAL 3

void do_try (trace_t * trace, const int hop,
	     const int max_hops, const int max_tries);

char *get_hostname (struct in_addr *addr);

int stop = 0;
int pid;
int seqno;	/* Most recent sequence number.  */
static char *hostname = NULL;
char addrstr[INET6_ADDRSTRLEN];
struct sockaddr_in dest;

#ifdef IP_OPTIONS
size_t len_ip_opts = 0;
char ip_opts[MAX_IPOPTLEN];
#endif /* IP_OPTIONS */

/* Cause for destination unreachable reply,
 * encoded as a single character.
 */
const char unreach_sign[NR_ICMP_UNREACH + 2] = "NHPPFS**U**TTXXX";

static enum trace_type opt_type = TRACE_UDP;
int opt_port = TRACE_UDP_PORT;
int opt_max_hops = 64;
static int opt_max_tries = 3;
int opt_resolve_hostnames = 0;
int opt_tos = -1;	/* Triggers with non-negative values.  */
int opt_ttl = TRACE_TTL;
int opt_wait = TIME_INTERVAL;
#ifdef IP_OPTIONS
char *opt_gateways = NULL;
#endif

const char args_doc[] = "HOST";
const char doc[] = "Print the route packets trace to network host.";
const char *program_authors[] = {
	"Elian Gidoni",
	NULL
};

/* Define keys for long options that do not have short counterparts. */
enum {
  OPT_RESOLVE = 256
};

static struct argp_option argp_options[] = {
#define GRP 0
  {"first-hop", 'f', "NUM", 0, "set initial hop distance, i.e., time-to-live",
   GRP+1},
#ifdef IP_OPTIONS
  {"gateways", 'g', "GATES", 0, "list of gateways for loose source routing",
   GRP+1},
#endif
  {"icmp", 'I', NULL, 0, "use ICMP ECHO as probe", GRP+1},
  {"max-hop", 'm', "NUM", 0, "set maximal hop count (default: 64)", GRP+1},
  {"port", 'p', "PORT", 0, "use destination PORT port (default: 33434)",
   GRP+1},
  {"resolve-hostnames", OPT_RESOLVE, NULL, 0, "resolve hostnames", GRP+1},
  {"tos", 't', "NUM", 0, "set type of service (TOS) to NUM", GRP+1},
  {"tries", 'q', "NUM", 0, "send NUM probe packets per hop (default: 3)",
   GRP+1},
  {"type", 'M', "METHOD", 0, "use METHOD (`icmp' or `udp') for traceroute "
   "operations, defaulting to `udp'", GRP+1},
  {"wait", 'w', "NUM", 0, "wait NUM seconds for response (default: 3)",
   GRP+1},
#undef GRP
  {NULL, 0, NULL, 0, NULL, 0}
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  char *p;
  static bool host_is_given = false;

  switch (key)
    {
    case 'f':
      opt_ttl = strtol (arg, &p, 0);
      if (*p || opt_ttl <= 0 || opt_ttl > 255)
        error (EXIT_FAILURE, 0, "impossible distance `%s'", arg);
      break;

#ifdef IP_OPTIONS
    case 'g':
      if (opt_gateways)
	{
	  size_t len = 0;

	  len = strlen (opt_gateways);
	  opt_gateways = xrealloc (opt_gateways, len + strlen (arg) + 3);

	  /* Append new gateways to old list,
	   * separating the two parts by a comma.
	   */
	  opt_gateways[len] = ',';
	  opt_gateways[len + 1] = '\0';
	  strcat (opt_gateways, arg);
	}
      else
	opt_gateways = xstrdup (arg);
      break;
#endif /* IP_OPTIONS */

    case 'I':
      opt_type = TRACE_ICMP;
      break;

    case 'm':
      opt_max_hops = strtol (arg, &p, 0);
      if (*p || opt_max_hops <= 0 || opt_max_hops > 255)
	error (EXIT_FAILURE, 0, "invalid hops value `%s'", arg);
      break;

    case 'p':
      opt_port = strtol (arg, &p, 0);
      if (*p || opt_port <= 0 || opt_port > 65536)
        error (EXIT_FAILURE, 0, "invalid port number `%s'", arg);
      break;

    case 'q':
      opt_max_tries = (int) strtol (arg, &p, 10);
      if (*p)
        argp_error (state, "invalid value (`%s' near `%s')", arg, p);
      if (opt_max_tries < 1 || opt_max_tries > 10)
        error (EXIT_FAILURE, 0, "number of tries should be between 1 and 10");
      break;

    case 't':
      opt_tos = strtol (arg, &p, 0);
      if (*p || opt_tos < 0 || opt_tos > 255)
	error (EXIT_FAILURE, 0, "invalid TOS value `%s'", arg);
      break;

    case 'M':
      if (strcmp (arg, "icmp") == 0)
        opt_type = TRACE_ICMP;
      else if (strcmp (arg, "udp") == 0)
        opt_type = TRACE_UDP;
      else
        argp_error (state, "invalid method");
      break;

    case 'w':
      opt_wait = strtol (arg, &p, 0);
      if (*p || opt_wait < 0 || opt_wait > 60)
	error (EXIT_FAILURE, 0, "ridiculous waiting time `%s'", arg);
      break;

    case OPT_RESOLVE:
      opt_resolve_hostnames = 1;
      break;

    case ARGP_KEY_ARG:
      host_is_given = true;
      hostname = xstrdup(arg);
      break;

    case ARGP_KEY_SUCCESS:
      if (!host_is_given)
        argp_error (state, "missing host operand");
      break;

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
  int hop, rc;
  char *rhost;
  struct addrinfo hints, *res;
  trace_t trace;

  set_program_name (argv[0]);

#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif

  pid = getpid() & 0xffff;

  /* Parse command line */
  iu_argp_init ("traceroute", program_authors);
  argp_parse (&argp, argc, argv, 0, NULL, NULL);

  if ((hostname == NULL) || (*hostname == '\0'))
    error (EXIT_FAILURE, 0, "unknown host");

  /* Hostname lookup first for better information */
  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET;
  hints.ai_flags = AI_CANONNAME;
#ifdef AI_IDN
  hints.ai_flags |= AI_IDN;
# ifdef AI_CANONIDN
  hints.ai_flags |= AI_CANONIDN;
# endif
#endif

#ifdef HAVE_IDN
  rc = idna_to_ascii_lz (hostname, &rhost, 0);
  free (hostname);

  if (rc)
    error (EXIT_FAILURE, 0, "unknown host");
#else /* !HAVE_IDN */
  rhost = hostname;
#endif

  rc = getaddrinfo (rhost, NULL, &hints, &res);

  if (rc)
    error (EXIT_FAILURE, 0, "unknown host");

  memcpy (&dest, res->ai_addr, res->ai_addrlen);
  dest.sin_port = htons (opt_port);

  getnameinfo (res->ai_addr, res->ai_addrlen, addrstr, sizeof (addrstr),
	       NULL, 0, NI_NUMERICHOST);

  printf ("traceroute to %s (%s), %d hops max\n",
	  res->ai_canonname ? res->ai_canonname : rhost,
	  addrstr, opt_max_hops);

  free (rhost);
  freeaddrinfo (res);

  trace_ip_opts (&dest);

  trace_init (&trace, dest, opt_type);

  hop = 1;
  seqno = -1;	/* One less than first usable packet number 0.  */

  while (!stop)
    {
      if (hop > opt_max_hops)
	exit (EXIT_FAILURE);
      do_try (&trace, hop, opt_max_hops, opt_max_tries);
      trace_inc_ttl (&trace);
      trace_inc_port (&trace);
      hop++;
    }

  exit (EXIT_SUCCESS);
}

void
do_try (trace_t * trace, const int hop,
	const int max_hops _GL_UNUSED_PARAMETER,
	const int max_tries)
{
  fd_set readset;
  int ret, tries, readonly = 0;
  struct timeval now, time;
  double triptime = 0.0;
  uint32_t prev_addr = 0;

  printf (" %2d  ", hop);

  for (tries = 0; tries < max_tries; tries++)
    {
      int save_errno;
      int fd = trace_icmp_sock (trace);

      FD_ZERO (&readset);
      FD_SET (fd, &readset);

      memset (&time, 0, sizeof (time));		/* 64-bit issue.  */
      time.tv_sec = opt_wait;
      time.tv_usec = 0;

      if (!readonly)
	trace_write (trace);

      errno = 0;
      ret = select (fd + 1, &readset, NULL, NULL, &time);
      save_errno = errno;

      gettimeofday (&now, NULL);

      now.tv_usec -= trace->tsent.tv_usec;
      now.tv_sec -= trace->tsent.tv_sec;
      if (now.tv_usec < 0)
	{
	  --now.tv_sec;
	  now.tv_usec += 1000000;
	}

      if (ret < 0)
	{
	  switch (save_errno)
	    {
	    case EINTR:
	      /* was interrupted */
	      break;
	    default:
	      error (EXIT_FAILURE, errno, "select failed");
	      break;
	    }
	}
      else if (ret == 0)
	{
	  /* time expired */
	  printf (" * ");
	  fflush (stdout);
	}
      else
	{
	  if (FD_ISSET (fd, &readset))
	    {
	      int rc, type, code;

	      triptime = ((double) now.tv_sec) * 1000.0 +
		((double) now.tv_usec) / 1000.0;

	      rc = trace_read (trace, &type, &code);

	      if (rc < 0)
		{
		  /* FIXME: printf ("Some error ocurred\n"); */
		  tries--;
		  readonly = 1;
		  continue;
		}
	      else
		{
		  if (tries == 0 || prev_addr != trace->from.sin_addr.s_addr)
		    {
		      printf (" %s ", inet_ntoa (trace->from.sin_addr));
		      if (opt_resolve_hostnames)
			printf ("(%s) ",
			    get_hostname (&trace->from.sin_addr));
		    }
		  printf (" %.3fms ", triptime);

		  /* Additional messages.  */
		  if (rc > 0 && type == ICMP_DEST_UNREACH)
		    printf ("!%c ", unreach_sign[code & 0x0f]);

		}
	      prev_addr = trace->from.sin_addr.s_addr;
	    }
	}
      readonly = 0;
      fflush (stdout);
    }
  printf ("\n");
}

char *
get_hostname (struct in_addr *addr)
{
  struct hostent *info =
	gethostbyaddr ((char *) addr, sizeof (*addr), AF_INET);
  if (info != NULL)
    return info->h_name;

  return inet_ntoa (*addr);
}

void
trace_init (trace_t * t, const struct sockaddr_in to,
	    const enum trace_type type)
{
  int fd;
  const int *ttlp;

  assert (t);
  ttlp = &t->ttl;

  t->type = type;
  t->to = to;
  t->ttl = opt_ttl;

  if (t->type == TRACE_UDP)
    {
      t->udpfd = socket (PF_INET, SOCK_DGRAM, 0);
      if (t->udpfd < 0)
        error (EXIT_FAILURE, errno, "socket");

      if (setsockopt (t->udpfd, IPPROTO_IP, IP_TTL, ttlp,
		      sizeof (*ttlp)) < 0)
        error (EXIT_FAILURE, errno, "setsockopt");
    }

  if (t->type == TRACE_ICMP || t->type == TRACE_UDP)
    {
      struct protoent *protocol = getprotobyname ("icmp");
      if (protocol)
	{
	  t->icmpfd = socket (PF_INET, SOCK_RAW, protocol->p_proto);
	  if (t->icmpfd < 0)
	    error (EXIT_FAILURE, errno, "socket");

	  if (setsockopt (t->icmpfd, IPPROTO_IP, IP_TTL,
			  ttlp, sizeof (*ttlp)) < 0)
	    error (EXIT_FAILURE, errno, "setsockopt");
	}
      else
	{
	  /* FIXME: Should we error out? */
	  error (EXIT_FAILURE, 0, "can't find supplied protocol 'icmp'");
	}

      /* free (protocol); ??? */
      /* FIXME: ... */
    }
  else
    {
      /* FIXME: type according to RFC 1393 */
    }

  fd = (t->type == TRACE_UDP ? t->udpfd : t->icmpfd);

  if (opt_tos >= 0)
    if (setsockopt (fd, IPPROTO_IP, IP_TOS,
		    &opt_tos, sizeof (opt_tos)) < 0)
      error (0, errno, "setsockopt(IP_TOS)");

#ifdef IP_OPTIONS
  if (len_ip_opts)
    if (setsockopt (fd, IPPROTO_IP, IP_OPTIONS,
		    &ip_opts, len_ip_opts) < 0)
      error (0, errno, "setsockopt(IPOPT_LSRR)");
#endif /* IP_OPTIONS */
}

void
trace_port (trace_t * t, const unsigned short int port)
{
  assert (t);
  if (port < IPPORT_RESERVED)
    t->to.sin_port = TRACE_UDP_PORT;
  else
    t->to.sin_port = port;
}

/* Returned packet may contain, according to specifications:
 *
 *   IP-header + IP-options		(new IP-header)
 *     + ICMP-header + old-IP-header	(ICMP message)
 *     + old-IP-options + old-ICMP-header
 */

#define CAPTURE_LEN (MAXIPLEN + MAXICMPLEN)

int
trace_read (trace_t * t, int * type, int * code)
{
  int len, rc = 0;
  unsigned char data[CAPTURE_LEN];
  struct ip *ip;
  icmphdr_t *ic;
  socklen_t siz;

  assert (t);

  siz = sizeof (t->from);

  len = recvfrom (t->icmpfd, (char *) data, sizeof (data), 0,
		  (struct sockaddr *) &t->from, &siz);
  if (len < 0)
    error (EXIT_FAILURE, errno, "recvfrom");

  icmp_generic_decode (data, sizeof (data), &ip, &ic);

  /* Pass type and code of incoming packet.  */
  *type = ic->icmp_type;
  *code = ic->icmp_code;

  switch (t->type)
    {
    case TRACE_UDP:
      {
	unsigned short *port;
	if ((ic->icmp_type != ICMP_TIME_EXCEEDED
	     && ic->icmp_type != ICMP_DEST_UNREACH))
	  return -1;

	/* check whether it's for us */
        port = (unsigned short *) ((void *) &ic->icmp_ip +
			(ic->icmp_ip.ip_hl << 2) + sizeof (in_port_t));
	if (*port != t->to.sin_port)	/* Network byte order!  */
	  return -1;

	if (ic->icmp_type == ICMP_DEST_UNREACH)
	  /* FIXME: Ugly hack. */
	  stop = 1;

	/* Only ICMP_PORT_UNREACH is an expected reply,
	 * all other denials produce additional information.
	 */
	if (ic->icmp_type == ICMP_DEST_UNREACH
	    && ic->icmp_code != ICMP_PORT_UNREACH)
	  rc = 1;
      }
      break;

    case TRACE_ICMP:
      if (! (ic->icmp_type == ICMP_TIME_EXCEEDED
	     || ic->icmp_type == ICMP_ECHOREPLY
	     || ic->icmp_type == ICMP_DEST_UNREACH) )
	return -1;

      if (ic->icmp_type == ICMP_ECHOREPLY
	  && (ntohs (ic->icmp_seq) != seqno
	      || ntohs (ic->icmp_id) != pid))
	return -1;

      if (ic->icmp_type == ICMP_TIME_EXCEEDED
	  || ic->icmp_type == ICMP_DEST_UNREACH)
	{
	  unsigned short seq, ident;
	  struct ip *old_ip;
	  icmphdr_t *old_icmp;

	  old_ip = (struct ip *) &ic->icmp_ip;
	  old_icmp = (icmphdr_t *) ((void *) old_ip + (old_ip->ip_hl <<2));
	  seq = ntohs (old_icmp->icmp_seq);
	  ident = ntohs (old_icmp->icmp_id);

	  /* An expired packet tests identity and sequence number,
	   * whereas an undeliverable packet only checks identity.
	   */
	  if (ident != pid
	      || (ic->icmp_type == ICMP_TIME_EXCEEDED
		  && seq != seqno))
	    return -1;
	}

      if (ip->ip_src.s_addr == dest.sin_addr.s_addr
	  || ic->icmp_type == ICMP_DEST_UNREACH)
	/* FIXME: Ugly hack. */
	stop = 1;

      if (ic->icmp_type == ICMP_DEST_UNREACH)
	rc = 1;
      break;

      /* FIXME: Type according to RFC 1393. */

    default:
      break;
    }

  return rc;
}

int
trace_write (trace_t * t)
{
  int len;

  assert (t);

  switch (t->type)
    {
    case TRACE_UDP:
      {
	char data[] = "SUPERMAN";

	len = sendto (t->udpfd, (char *) data, sizeof (data),
		      0, (struct sockaddr *) &t->to, sizeof (t->to));
	if (len < 0)
	  {
	    switch (errno)
	      {
	      case ECONNRESET:
		break;
	      default:
		error (EXIT_FAILURE, errno, "sendto");
	      }
	  }

	if (gettimeofday (&t->tsent, NULL) < 0)
	  error (EXIT_FAILURE, errno, "gettimeofday");
      }
      break;

    case TRACE_ICMP:
      {
	icmphdr_t hdr;

	/* The sequence number is updated to a valid value!  */
	if (icmp_echo_encode ((unsigned char *) &hdr, sizeof (hdr),
			      pid, ++seqno))
	  return -1;

	len = sendto (t->icmpfd, (char *) &hdr, sizeof (hdr),
		      0, (struct sockaddr *) &t->to, sizeof (t->to));
	if (len < 0)
	  {
	    switch (errno)
	      {
	      case ECONNRESET:
		break;
	      default:
		error (EXIT_FAILURE, errno, "sendto");
	      }
	  }

	if (gettimeofday (&t->tsent, NULL) < 0)
	  error (EXIT_FAILURE, errno, "gettimeofday");
      }
      break;

      /* FIXME: type according to RFC 1393 */

    default:
      break;
    }

  return 0;
}

int
trace_udp_sock (trace_t * t)
{
  return (t != NULL ? t->udpfd : -1);
}

int
trace_icmp_sock (trace_t * t)
{
  return (t != NULL ? t->icmpfd : -1);
}

void
trace_inc_ttl (trace_t * t)
{
  int fd;
  const int *ttlp;

  assert (t);

  ttlp = &t->ttl;
  t->ttl++;
  fd = (t->type == TRACE_UDP ? t->udpfd : t->icmpfd);
  if (setsockopt (fd, IPPROTO_IP, IP_TTL, ttlp, sizeof (*ttlp)) < 0)
    error (EXIT_FAILURE, errno, "setsockopt");
}

void
trace_inc_port (trace_t * t)
{
  assert (t);
  if (t->type == TRACE_UDP)
    t->to.sin_port = htons (ntohs (t->to.sin_port) + 1);
}

void
trace_ip_opts (struct sockaddr_in *to)
{
#ifdef IP_OPTIONS
  if (opt_gateways && *opt_gateways)
    {
      char *gateway, *optbase;
      struct addrinfo hints, *res;

      memset (&hints, 0, sizeof (hints));
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_DGRAM;

      memset (&ip_opts, 0, sizeof (ip_opts));
      optbase = ip_opts;

      /* Set up any desired options.  Keep
       * `optbase' updated, pointing to the
       * part presently under construction.
       */

      /* 1. Loose source routing.  */
      gateway = opt_gateways;
      optbase[IPOPT_OPTVAL] = IPOPT_LSRR;
      optbase[IPOPT_OLEN] = IPOPT_MINOFF - 1;	/* No payload yet.  */
      optbase[IPOPT_OFFSET] = IPOPT_MINOFF;	/* Empty payload.  */

      /* Traverse the gateway list, inserting
       * addresses in the stated order.  Take
       * care not to overflow available space.
       */
      while (gateway && *gateway
	     && optbase[IPOPT_OFFSET]
		< (int) (MAX_IPOPTLEN - sizeof (struct in_addr)))
	{
	  int rc;
	  char *p;

	  p = strpbrk (gateway, " ,;:");
	  if (p)
	    *p++ = '\0';

	  rc = getaddrinfo (gateway, NULL, &hints, &res);
	  if (rc)
	    error (EXIT_FAILURE, errno, "gateway `%s' %s",
		   gateway, gai_strerror(rc));

	  /* Put target into next unused slot.  */
	  memcpy (optbase + optbase[IPOPT_OLEN],
		  &((struct sockaddr_in *) res->ai_addr)->sin_addr,
		  sizeof (struct in_addr));

	  freeaddrinfo (res);

	  /* Option gained in length.  */
	  optbase[IPOPT_OLEN] += sizeof (struct in_addr);

	  gateway = p;
	}

      if (gateway && *gateway)
	error (EXIT_FAILURE, 0, "too many gateways specified");

      /* Append the final destination.  */
      memcpy (optbase + optbase[IPOPT_OLEN],
	      &to->sin_addr, sizeof (to->sin_addr));
      optbase[IPOPT_OLEN] += sizeof (to->sin_addr);

      /* 2. There is an implicit IPOPT_EOL after
       * IPOPT_LSRR, ensured by the call to memset().
       * Use it!
       */
      len_ip_opts = optbase[IPOPT_OLEN] + 1;
    }
#endif /* IP_OPTIONS */
}
