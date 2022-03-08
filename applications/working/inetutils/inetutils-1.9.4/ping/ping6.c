/*
  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012,
  2013, 2014, 2015 Free Software Foundation, Inc.

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

#ifdef __sun
#  define _XPG4_2	1	/* OpenSolaris: msg_control */
#endif /* __sun */

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <signal.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <argp.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif
#ifdef HAVE_IDNA_H
# include <idna.h>
#endif

#include <unused-parameter.h>
#include <xalloc.h>
#include "ping6.h"
#include "libinetutils.h"

static PING *ping;
bool is_root = false;
unsigned char *data_buffer;
unsigned char *patptr;
int one = 1;
int pattern_len = MAXPATTERN;
size_t data_length = PING_DATALEN;
size_t count = DEFAULT_PING_COUNT;
size_t interval;
int socket_type;
int timeout = -1;
int hoplimit = 0;
unsigned int options;
static unsigned long preload = 0;
#ifdef IPV6_TCLASS
int tclass = -1;	/* Kernel sets default: -1, RFC 3542.  */
#endif
#ifdef IPV6_FLOWINFO
int flowinfo;
#endif

static int ping_echo (char *hostname);
static void ping_reset (PING * p);
static int send_echo (PING * ping);

const char args_doc[] = "HOST ...";
const char doc[] = "Send ICMP ECHO_REQUEST packets to network hosts."
                   "\vOptions marked with (root only) are available only to "
                   "superuser.";
const char *program_authors[] = {
	"Jeroen Dekkers",
	NULL
};

enum {
  ARG_HOPLIMIT = 256,
};

static struct argp_option argp_options[] = {
#define GRP 0
  {NULL, 0, NULL, 0, "Options valid for all request types:", GRP},
  {"count", 'c', "NUMBER", 0, "stop after sending NUMBER packets", GRP+1},
  {"debug", 'd', NULL, 0, "set the SO_DEBUG option", GRP+1},
#ifdef IPV6_FLOWINFO
  {"flowinfo", 'F', "N", 0, "set N as flow identifier", GRP+1},
#endif
  {"hoplimit", ARG_HOPLIMIT, "N", 0, "specify N as hop-limit", GRP+1},
  {"interval", 'i', "NUMBER", 0, "wait NUMBER seconds between sending each "
   "packet", GRP+1},
  {"numeric", 'n', NULL, 0, "do not resolve host addresses", GRP+1},
  {"ignore-routing", 'r', NULL, 0, "send directly to a host on an attached "
   "network", GRP+1},
#ifdef IPV6_TCLASS
  {"tos", 'T', "N", 0, "set traffic class to N", GRP+1},
#endif
  {"timeout", 'w', "N", 0, "stop after N seconds", GRP+1},
  {"ttl", ARG_HOPLIMIT, "N", 0, "synonym for --hoplimit", GRP+1},
  {"verbose", 'v', NULL, 0, "verbose output", GRP+1},
#undef GRP
#define GRP 10
  {NULL, 0, NULL, 0, "Options valid for --echo requests:", GRP},
  {"flood", 'f', NULL, 0, "flood ping (root only)", GRP+1},
  {"preload", 'l', "NUMBER", 0, "send NUMBER packets as fast as possible "
   "before falling into normal mode of behavior (root only)", GRP+1},
  {"pattern", 'p', "PATTERN", 0, "fill ICMP packet with given pattern (hex)",
   GRP+1},
  {"quiet", 'q', NULL, 0, "quiet output", GRP+1},
  {"size", 's', "NUMBER", 0, "send NUMBER data octets", GRP+1},
#undef GRP
  {NULL, 0, NULL, 0, NULL, 0}
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  char *endptr;
  static unsigned char pattern[MAXPATTERN];

  switch (key)
    {
    case 'c':
      count = ping_cvt_number (arg, 0, 0);
      break;

    case 'd':
      socket_type |= SO_DEBUG;
      break;

    case 'f':
      if (!is_root)
        error (EXIT_FAILURE, 0, "flooding needs root privilege");

      options |= OPT_FLOOD;
      setbuf (stdout, (char *) NULL);
      break;

#ifdef IPV6_FLOWINFO
    case 'F':
      options |= OPT_FLOWINFO;
      flowinfo = ping_cvt_number (arg, 0, 0) & IPV6_FLOWINFO_FLOWLABEL;
      break;
#endif

    case 'i':
      options |= OPT_INTERVAL;
      interval = ping_cvt_number (arg, 0, 0);
      break;

    case 'l':
      if (!is_root)
        error (EXIT_FAILURE, 0, "preloading needs root privilege");

      preload = strtoul (arg, &endptr, 0);
      if (*endptr || preload > INT_MAX)
        error (EXIT_FAILURE, 0, "preload size too large");

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

    case 'r':
      socket_type |= SO_DONTROUTE;
      break;

    case 's':
      data_length = ping_cvt_number (arg, PING_MAX_DATALEN, 1);
      break;

#ifdef IPV6_TCLASS
    case 'T':
      options |= OPT_TCLASS;
      tclass = ping_cvt_number (arg, 0, 0);
      break;
#endif

    case 'v':
      options |= OPT_VERBOSE;
      break;

    case 'w':
      timeout = ping_cvt_number (arg, INT_MAX, 0);
      break;

    case ARG_HOPLIMIT:
      hoplimit = ping_cvt_number (arg, 255, 0);
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
  int status = 0;

  set_program_name (argv[0]);

# ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
# endif

  if (getuid () == 0)
    is_root = true;

  /* Parse command line */
  iu_argp_init ("ping6", program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  ping = ping_init (0, getpid ());
  if (ping == NULL)
    /* ping_init() prints our error message.  */
    exit (EXIT_FAILURE);

  setsockopt (ping->ping_fd, SOL_SOCKET, SO_BROADCAST, (char *) &one, sizeof (one));

  /* Reset root privileges */
  setuid (getuid ());

  /* Force line buffering regardless of output device.  */
  setvbuf (stdout, NULL, _IOLBF, 0);

  argc -= index;
  argv += index;

  if (count != 0)
    ping_set_count (ping, count);

  if (socket_type != 0)
    ping_set_sockopt (ping, socket_type, &one, sizeof (one));

  if (options & OPT_INTERVAL)
    ping_set_interval (ping, interval);

  if (hoplimit > 0)
    if (setsockopt (ping->ping_fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
		    &hoplimit, sizeof (hoplimit)) < 0)
      error (0, errno, "setsockopt(IPV6_HOPLIMIT)");

#ifdef IPV6_TCLASS
  if (options & OPT_TCLASS)
    if (setsockopt (ping->ping_fd, IPPROTO_IPV6, IPV6_TCLASS,
		    &tclass, sizeof (tclass)) < 0)
      error (EXIT_FAILURE, errno, "setsockopt(IPV6_TCLASS)");
#endif

#ifdef IPV6_FLOWINFO
  if (options & OPT_FLOWINFO)
    if (setsockopt (ping->ping_fd, IPPROTO_IPV6, IPV6_FLOWINFO,
		    &flowinfo, sizeof (flowinfo)) < 0)
      error (EXIT_FAILURE, errno, "setsockopt(IPV6_FLOWINFO)");
#endif

  init_data_buffer (patptr, pattern_len);

  while (argc--)
    {
      status |= ping_echo (*argv++);
      ping_reset (ping);
    }

  return status;
}

static volatile int stop = 0;

static void
sig_int (int signal _GL_UNUSED_PARAMETER)
{
  stop = 1;
}

static int
ping_run (PING * ping, int (*finish) ())
{
  fd_set fdset;
  int fdmax;
  struct timeval resp_time;
  struct timeval last, intvl, now;
  struct timeval *t = NULL;
  int finishing = 0;
  size_t nresp = 0;
  unsigned long i;

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
		{
		  putchar ('.');
		}

	      if (ping_timeout_p (&ping->ping_start_time, timeout))
		break;
	    }
	  else if (finishing)
	    break;
	  else
	    {
	      finishing = 1;

	      intvl.tv_sec = MAXWAIT;
	    }
	  gettimeofday (&last, NULL);
	}
    }

  ping_unset_data (ping);

  if (finish)
    return (*finish) ();
  return 0;
}

static int
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

static int
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
	printf ("-- somebody's printing up packets!");
      else
	printf ("%d%% packet loss",
		(int) (((ping->ping_num_xmit - ping->ping_num_recv) * 100) /
		       ping->ping_num_xmit));

    }
  printf ("\n");
  return 0;
}

static int print_echo (int dup, int hops, struct ping_stat *stat,
		       struct sockaddr_in6 *dest, struct sockaddr_in6 *from,
		       struct icmp6_hdr *icmp6, int datalen);
static void print_icmp_error (struct sockaddr_in6 *from,
			      struct icmp6_hdr *icmp6, int len);

static int echo_finish (void);

static int
ping_echo (char *hostname)
{
  int err;
  char buffer[256];
  struct ping_stat ping_stat;
  int status;

  if (options & OPT_FLOOD && options & OPT_INTERVAL)
    error (EXIT_FAILURE, 0, "-f and -i incompatible options");

  memset (&ping_stat, 0, sizeof (ping_stat));
  ping_stat.tmin = 999999999.0;

  ping->ping_datalen = data_length;
  ping->ping_closure = &ping_stat;

  if (ping_set_dest (ping, hostname))
    error (EXIT_FAILURE, 0, "unknown host %s", hostname);

  err = getnameinfo ((struct sockaddr *) &ping->ping_dest.ping_sockaddr6,
		     sizeof (ping->ping_dest.ping_sockaddr6), buffer,
		     sizeof (buffer), NULL, 0, NI_NUMERICHOST);
  if (err)
    {
      const char *errmsg;

      if (err == EAI_SYSTEM)
	errmsg = strerror (errno);
      else
	errmsg = gai_strerror (err);

      error (EXIT_FAILURE, 0, "getnameinfo: %s", errmsg);
    }

  printf ("PING %s (%s): %zu data bytes",
	  ping->ping_hostname, buffer, data_length);
  if (options & OPT_VERBOSE)
    printf (", id 0x%04x = %u", ping->ping_ident, ping->ping_ident);

  printf ("\n");

  status = ping_run (ping, echo_finish);
  free (ping->ping_hostname);
  return status;
}

static void
ping_reset (PING * p)
{
  p->ping_num_xmit = 0;
  p->ping_num_recv = 0;
  p->ping_num_rept = 0;
}

static int
print_echo (int dupflag, int hops, struct ping_stat *ping_stat,
	    struct sockaddr_in6 *dest _GL_UNUSED_PARAMETER,
	    struct sockaddr_in6 *from,
	    struct icmp6_hdr *icmp6, int datalen)
{
  int err;
  char buf[256];
  struct timeval tv;
  int timing = 0;
  double triptime = 0.0;

  gettimeofday (&tv, NULL);

  /* Do timing */
  if (PING_TIMING (datalen - sizeof (struct icmp6_hdr)))
    {
      struct timeval tv1, *tp;

      timing++;
      tp = (struct timeval *) (icmp6 + 1);

      /* Avoid unaligned data: */
      memcpy (&tv1, tp, sizeof (tv1));
      tvsub (&tv, &tv1);

      triptime = ((double) tv.tv_sec) * 1000.0 +
	((double) tv.tv_usec) / 1000.0;
      ping_stat->tsum += triptime;
      ping_stat->tsumsq += triptime * triptime;
      if (triptime < ping_stat->tmin)
	ping_stat->tmin = triptime;
      if (triptime > ping_stat->tmax)
	ping_stat->tmax = triptime;
    }

  if (options & OPT_QUIET)
    return 0;
  if (options & OPT_FLOOD)
    {
      putchar ('\b');
      return 0;
    }

  err = getnameinfo ((struct sockaddr *) from, sizeof (*from),
		     buf, sizeof (buf), NULL, 0,
		     (options & OPT_NUMERIC) ? NI_NUMERICHOST
#ifdef NI_IDN
		     : NI_IDN
#else
		     : 0
#endif
		     );
  if (err)
    {
      const char *errmsg;

      if (err == EAI_SYSTEM)
	errmsg = strerror (errno);
      else
	errmsg = gai_strerror (err);

      fprintf (stderr, "ping: getnameinfo: %s\n", errmsg);

      strcpy (buf, "unknown");
    }

  printf ("%d bytes from %s: icmp_seq=%u", datalen, buf,
	  ntohs (icmp6->icmp6_seq));
  if (hops >= 0)
    printf (" ttl=%d", hops);
  if (timing)
    printf (" time=%.3f ms", triptime);
  if (dupflag)
    printf (" (DUP!)");

  putchar ('\n');

  return 0;
}

#define NITEMS(a) sizeof(a)/sizeof((a)[0])

struct icmp_code_descr
{
  int code;
  char *diag;
};

static struct icmp_code_descr icmp_dest_unreach_desc[] = {
  {ICMP6_DST_UNREACH_NOROUTE, "No route to destination"},
  {ICMP6_DST_UNREACH_ADMIN, "Destination administratively prohibited"},
  {ICMP6_DST_UNREACH_BEYONDSCOPE, "Beyond scope of source address"},
  {ICMP6_DST_UNREACH_ADDR, "Address unreachable"},
  {ICMP6_DST_UNREACH_NOPORT, "Port unreachable"},
};

static void
print_dst_unreach (struct icmp6_hdr *icmp6)
{
  struct icmp_code_descr *p;

  printf ("Destination unreachable: ");
  for (p = icmp_dest_unreach_desc;
       p < icmp_dest_unreach_desc + NITEMS (icmp_dest_unreach_desc); p++)
    {
      if (p->code == icmp6->icmp6_code)
	{
	  puts (p->diag);
	  return;
	}
    }

  printf ("Unknown code %d\n", icmp6->icmp6_code);
}

static void
print_packet_too_big (struct icmp6_hdr *icmp6)
{
  printf ("Packet too big, mtu=%d\n", icmp6->icmp6_mtu);
}

static struct icmp_code_descr icmp_time_exceeded_desc[] = {
  {ICMP6_TIME_EXCEED_TRANSIT, "Hop limit exceeded"},
  {ICMP6_TIME_EXCEED_REASSEMBLY, "Fragment reassembly timeout"},
};

static void
print_time_exceeded (struct icmp6_hdr *icmp6)
{
  struct icmp_code_descr *p;

  printf ("Time exceeded: ");
  for (p = icmp_time_exceeded_desc;
       p < icmp_time_exceeded_desc + NITEMS (icmp_time_exceeded_desc); p++)
    {
      if (p->code == icmp6->icmp6_code)
	{
	  puts (p->diag);
	  return;
	}
    }

  printf ("Unknown code %d\n", icmp6->icmp6_code);
}

static struct icmp_code_descr icmp_param_prob_desc[] = {
  {ICMP6_PARAMPROB_HEADER, "Erroneous header field"},
  {ICMP6_PARAMPROB_NEXTHEADER, "Unrecognized Next Header type"},
  {ICMP6_PARAMPROB_OPTION, "Unrecognized IPv6 option"},
};

static void
print_param_prob (struct icmp6_hdr *icmp6)
{
  struct icmp_code_descr *p;

  printf ("Parameter problem: ");
  for (p = icmp_param_prob_desc;
       p < icmp_param_prob_desc + NITEMS (icmp_param_prob_desc); p++)
    {
      if (p->code == icmp6->icmp6_code)
	{
	  puts (p->diag);
	  return;
	}
    }

  printf ("Unknown code %d\n", icmp6->icmp6_code);
}

void
print_ip_data (struct icmp6_hdr *icmp6)
{
  size_t j;
  struct ip6_hdr *ip = (struct ip6_hdr *) ((char *) icmp6 + sizeof (*icmp6));
  char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];

  (void) inet_ntop (AF_INET6, &ip->ip6_dst, dst, sizeof (dst));
  (void) inet_ntop (AF_INET6, &ip->ip6_src, src, sizeof (src));

  printf ("IP Header Dump:\n ");
  for (j = 0; j < sizeof (*ip) - sizeof (ip->ip6_src) - sizeof (ip->ip6_dst); ++j)
    printf ("%02x%s", *((unsigned char *) ip + j),
	    (j % 2) ? " " : "");	/* Group bytes two by two.  */
  printf ("(src) (dst)\n");

  printf ("Vr TC Flow Plen Nxt Hop Src\t\t  Dst\n");
  printf (" %1x %02x %04x %4hu %3hhu %3hhu %s %s\n",
	  ntohl (ip->ip6_flow) >> 28,
	  (ntohl (ip->ip6_flow) & 0x0fffffff) >> 20,
	  ntohl (ip->ip6_flow) & 0x0fffff,
	  ntohs (ip->ip6_plen), ip->ip6_nxt, ip->ip6_hlim,
	  src, dst);

  switch (ip->ip6_nxt)
    {
    case IPPROTO_ICMPV6:
      {
	struct icmp6_hdr *hdr =
	  (struct icmp6_hdr *) ((unsigned char *) ip + sizeof (*ip));

	printf ("ICMP: type %hhu, code %hhu, size %hu",
		hdr->icmp6_type, hdr->icmp6_code, ntohs (ip->ip6_plen));
	switch (hdr->icmp6_type)
	  {
	  case ICMP6_ECHO_REQUEST:
	  case ICMP6_ECHO_REPLY:
	    printf (", id 0x%04x, seq 0x%04x",
		    ntohs (hdr->icmp6_id), ntohs (hdr->icmp6_seq));
	    break;
	  default:
	    break;
	  }
      }
      break;
    default:
      break;
    }

  printf ("\n");
}

static struct icmp_diag
{
  int type;
  void (*func) (struct icmp6_hdr *);
} icmp_diag[] = {
  {ICMP6_DST_UNREACH, print_dst_unreach},
  {ICMP6_PACKET_TOO_BIG, print_packet_too_big},
  {ICMP6_TIME_EXCEEDED, print_time_exceeded},
  {ICMP6_PARAM_PROB, print_param_prob},
};

static void
print_icmp_error (struct sockaddr_in6 *from, struct icmp6_hdr *icmp6, int len)
{
  char *s;
  struct icmp_diag *p;

  s = ipaddr2str ((struct sockaddr *) from, sizeof (*from));
  printf ("%d bytes from %s: ", len, s);
  free (s);

  for (p = icmp_diag; p < icmp_diag + NITEMS (icmp_diag); p++)
    {
      if (p->type == icmp6->icmp6_type)
	{
	  p->func (icmp6);
	  if (options & OPT_VERBOSE)
	    print_ip_data (icmp6);

	  return;
	}
    }

  /* This should never happen because of the ICMP6_FILTER set in
     ping_init(). */
  printf ("Unknown ICMP type: %d\n", icmp6->icmp6_type);
}

static int
echo_finish (void)
{
  ping_finish ();
  if (ping->ping_num_recv && PING_TIMING (data_length))
    {
      struct ping_stat *ping_stat = (struct ping_stat *) ping->ping_closure;
      double total = ping->ping_num_recv + ping->ping_num_rept;
      double avg = ping_stat->tsum / total;
      double vari = ping_stat->tsumsq / total - avg * avg;

      printf ("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
	      ping_stat->tmin, avg, ping_stat->tmax, nsqrt (vari, 0.0005));
    }
  return (ping->ping_num_recv == 0);
}

static PING *
ping_init (int type _GL_UNUSED_PARAMETER, int ident)
{
  int fd, err;
  const int on = 1;
  PING *p;
  struct icmp6_filter filter;

  /* Initialize raw ICMPv6 socket */
  fd = socket (PF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
  if (fd < 0)
    {
      if (errno == EPERM || errno == EACCES)
        error (EXIT_FAILURE, errno, "raw socket");

      return NULL;
    }

  /* Tell which ICMPs we are interested in.  */
  ICMP6_FILTER_SETBLOCKALL (&filter);
  ICMP6_FILTER_SETPASS (ICMP6_ECHO_REPLY, &filter);
  ICMP6_FILTER_SETPASS (ICMP6_DST_UNREACH, &filter);
  ICMP6_FILTER_SETPASS (ICMP6_PACKET_TOO_BIG, &filter);
  ICMP6_FILTER_SETPASS (ICMP6_TIME_EXCEEDED, &filter);
  ICMP6_FILTER_SETPASS (ICMP6_PARAM_PROB, &filter);

  err =
    setsockopt (fd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, sizeof (filter));
  if (err)
    {
      close (fd);
      return NULL;
    }

  err = setsockopt (fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on, sizeof (on));
  if (err)
    {
      close (fd);
      return NULL;
    }

  /* Allocate PING structure and initialize it to default values */
  p = malloc (sizeof (*p));
  if (!p)
    {
      close (fd);
      return NULL;
    }

  memset (p, 0, sizeof (*p));

  p->ping_fd = fd;
  p->ping_count = DEFAULT_PING_COUNT;
  p->ping_interval = PING_DEFAULT_INTERVAL;
  p->ping_datalen = sizeof (struct timeval);
  /* Make sure we use only 16 bits in this field, id for icmp is a unsigned short.  */
  p->ping_ident = ident & 0xFFFF;
  p->ping_cktab_size = PING_CKTABSIZE;
  gettimeofday (&p->ping_start_time, NULL);
  return p;
}

static int
ping_xmit (PING * p)
{
  int i, buflen;
  struct icmp6_hdr *icmp6;

  if (_ping_setbuf (p, USE_IPV6))
    return -1;

  buflen = p->ping_datalen + sizeof (struct icmp6_hdr);

  /* Mark sequence number as sent */
  _PING_CLR (p, p->ping_num_xmit);

  icmp6 = (struct icmp6_hdr *) p->ping_buffer;
  icmp6->icmp6_type = ICMP6_ECHO_REQUEST;
  icmp6->icmp6_code = 0;
  /* The checksum will be calculated by the TCP/IP stack.  */
  icmp6->icmp6_cksum = 0;
  icmp6->icmp6_id = htons (p->ping_ident);
  icmp6->icmp6_seq = htons (p->ping_num_xmit);

  i = sendto (p->ping_fd, (char *) p->ping_buffer, buflen, 0,
	      (struct sockaddr *) &p->ping_dest.ping_sockaddr6,
	      sizeof (p->ping_dest.ping_sockaddr6));
  if (i < 0)
    return -1;
  else
    {
      p->ping_num_xmit++;
      if (i != buflen)
	printf ("ping: wrote %s %d chars, ret=%d\n",
		p->ping_hostname, buflen, i);
    }

  return 0;
}

static int
my_echo_reply (PING * p, struct icmp6_hdr *icmp6)
{
  struct ip6_hdr *orig_ip = (struct ip6_hdr *) (icmp6 + 1);
  struct icmp6_hdr *orig_icmp = (struct icmp6_hdr *) (orig_ip + 1);

  return IN6_ARE_ADDR_EQUAL (&orig_ip->ip6_dst, &ping->ping_dest.ping_sockaddr6.sin6_addr)
    && orig_ip->ip6_nxt == IPPROTO_ICMPV6
    && orig_icmp->icmp6_type == ICMP6_ECHO_REQUEST
    && orig_icmp->icmp6_id == htons (p->ping_ident);
}

static int
ping_recv (PING * p)
{
  int dupflag, n;
  int hops = -1;
  struct msghdr msg;
  struct iovec iov;
  struct icmp6_hdr *icmp6;
  struct cmsghdr *cmsg;
  char cmsg_data[1024];

  iov.iov_base = p->ping_buffer;
  iov.iov_len = _PING_BUFLEN (p, USE_IPV6);
  msg.msg_name = &p->ping_from.ping_sockaddr6;
  msg.msg_namelen = sizeof (p->ping_from.ping_sockaddr6);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_data;
  msg.msg_controllen = sizeof (cmsg_data);
  msg.msg_flags = 0;

  n = recvmsg (p->ping_fd, &msg, 0);
  if (n < 0)
    return -1;

  for (cmsg = CMSG_FIRSTHDR (&msg); cmsg; cmsg = CMSG_NXTHDR (&msg, cmsg))
    {
      if (cmsg->cmsg_level == IPPROTO_IPV6
	  && cmsg->cmsg_type == IPV6_HOPLIMIT)
	{
	  hops = *(int *) CMSG_DATA (cmsg);
	  break;
	}
    }

  icmp6 = (struct icmp6_hdr *) p->ping_buffer;
  if (icmp6->icmp6_type == ICMP6_ECHO_REPLY)
    {
      /* We got an echo reply.  */
      if (ntohs (icmp6->icmp6_id) != p->ping_ident)
	return -1;		/* It's not a response to us.  */

      if (_PING_TST (p, ntohs (icmp6->icmp6_seq)))
	{
	  /* We already got the reply for this echo request.  */
	  p->ping_num_rept++;
	  dupflag = 1;
	}
      else
	{
	  _PING_SET (p, ntohs (icmp6->icmp6_seq));
	  p->ping_num_recv++;
	  dupflag = 0;
	}

      print_echo (dupflag, hops, p->ping_closure, &p->ping_dest.ping_sockaddr6,
		  &p->ping_from.ping_sockaddr6, icmp6, n);

    }
  else
    {
      /* We got an error reply.  */
      if (!my_echo_reply (p, icmp6))
	return -1;		/* It's not for us.  */

      print_icmp_error (&p->ping_from.ping_sockaddr6, icmp6, n);
    }

  return 0;
}

static int
ping_set_dest (PING * ping, char *host)
{
  int err;
  struct addrinfo *result, hints;
  char *rhost;

#ifdef HAVE_IDN
  err = idna_to_ascii_lz (host, &rhost, 0);
  if (err)
    return 1;
#else /* !HAVE_IDN */
  rhost = host;
#endif

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET6;
  hints.ai_flags = AI_CANONNAME;
#ifdef AI_IDN
  hints.ai_flags |= AI_IDN;
#endif
#ifdef AI_CANONIDN
  hints.ai_flags |= AI_CANONIDN;
#endif

  err = getaddrinfo (rhost, NULL, &hints, &result);
  if (err)
    return 1;

  memcpy (&ping->ping_dest.ping_sockaddr6, result->ai_addr, result->ai_addrlen);

  if (result->ai_canonname)
    ping->ping_hostname = strdup (result->ai_canonname);
  else
    ping->ping_hostname = strdup (rhost);

#if HAVE_IDN
  free (rhost);
#endif
  freeaddrinfo (result);

  if (!ping->ping_hostname)
    return 1;

  return 0;
}
