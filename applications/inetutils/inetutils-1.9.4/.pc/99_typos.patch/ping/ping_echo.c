/*
  Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
  2010, 2011, 2012, 2013, 2014, 2015 Free Software Foundation, Inc.

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

#include <netinet/in.h>
#include <arpa/inet.h>

/*#include <netinet/ip_icmp.h>  -- deliberately not including this */
#ifdef HAVE_NETINET_IP_VAR_H
# include <netinet/ip_var.h>
#endif

#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unused-parameter.h>

#include <ping.h>
#include "ping_impl.h"

#define NROUTES		9	/* number of record route slots */
#ifndef MAX_IPOPTLEN
# define MAX_IPOPTLEN 40
#endif

#include <xalloc.h>

static int handler (int code, void *closure,
		    struct sockaddr_in *dest, struct sockaddr_in *from,
		    struct ip *ip, icmphdr_t * icmp, int datalen);
int print_echo (int dup, struct ping_stat *stat,
		struct sockaddr_in *dest, struct sockaddr_in *from,
		struct ip *ip, icmphdr_t * icmp, int datalen);
static int echo_finish (void);

void print_icmp_header (struct sockaddr_in *from,
			struct ip *ip, icmphdr_t * icmp, int len);
static void print_ip_opt (struct ip *ip, int hlen);

int
ping_echo (char *hostname)
{
#ifdef IP_OPTIONS
  char rspace[MAX_IPOPTLEN];	/* Maximal IP option space.  */
#endif
  struct ping_stat ping_stat;
  int status;

  if (options & OPT_FLOOD && options & OPT_INTERVAL)
    error (EXIT_FAILURE, 0, "-f and -i incompatible options");

  memset (&ping_stat, 0, sizeof (ping_stat));
  ping_stat.tmin = 999999999.0;

  ping_set_type (ping, ICMP_ECHO);
  ping_set_packetsize (ping, data_length);
  ping_set_event_handler (ping, handler, &ping_stat);

  if (ping_set_dest (ping, hostname))
    error (EXIT_FAILURE, 0, "unknown host");

  if (options & OPT_RROUTE)
    {
#ifdef IP_OPTIONS
      memset (rspace, 0, sizeof (rspace));
      rspace[IPOPT_OPTVAL] = IPOPT_RR;
      rspace[IPOPT_OLEN] = sizeof (rspace) - 1;
      rspace[IPOPT_OFFSET] = IPOPT_MINOFF;
      if (setsockopt (ping->ping_fd, IPPROTO_IP,
		      IP_OPTIONS, rspace, sizeof (rspace)) < 0)
        error (EXIT_FAILURE, errno, "setsockopt");
#else
      error (EXIT_FAILURE, 0, "record route not available in this "
             "implementation.");
#endif /* IP_OPTIONS */
    }
  else if (options & OPT_IPTIMESTAMP)
    {
      int type;

      if (suboptions & SOPT_TSPRESPEC)
#ifdef IPOPT_TS_PRESPEC_RFC791
	type = IPOPT_TS_PRESPEC_RFC791;
#else
	type = IPOPT_TS_PRESPEC;
#endif
      else if (suboptions & SOPT_TSADDR)
	type = IPOPT_TS_TSANDADDR;
      else
        type = IPOPT_TS_TSONLY;

#ifdef IP_OPTIONS
      memset (rspace, 0, sizeof (rspace));
      rspace[IPOPT_OPTVAL] = IPOPT_TS;
      rspace[IPOPT_OLEN] = sizeof (rspace);
      if (type != IPOPT_TS_TSONLY)
	rspace[IPOPT_OLEN] -= sizeof (n_time);	/* Exsessive part.  */
      rspace[IPOPT_OFFSET] = IPOPT_MINOFF + 1;

# ifdef IPOPT_POS_OV_FLG
      rspace[IPOPT_POS_OV_FLG] = type;
# else
      rspace[3] = type;
# endif /* !IPOPT_POS_OV_FLG */

      if (setsockopt (ping->ping_fd, IPPROTO_IP,
		      IP_OPTIONS, rspace, rspace[IPOPT_OLEN]) < 0)
        error (EXIT_FAILURE, errno, "setsockopt");
#else /* !IP_OPTIONS */
      error (EXIT_FAILURE, 0, "IP timestamp not available in this "
             "implementation.");
#endif /* IP_OPTIONS */
    }

  printf ("PING %s (%s): %zu data bytes",
	  ping->ping_hostname,
	  inet_ntoa (ping->ping_dest.ping_sockaddr.sin_addr), data_length);
  if (options & OPT_VERBOSE)
    printf (", id 0x%04x = %u", ping->ping_ident, ping->ping_ident);

  printf ("\n");

  status = ping_run (ping, echo_finish);
  free (ping->ping_hostname);
  return status;
}

int
handler (int code, void *closure,
	 struct sockaddr_in *dest, struct sockaddr_in *from,
	 struct ip *ip, icmphdr_t * icmp, int datalen)
{
  switch (code)
    {
    case PEV_RESPONSE:
    case PEV_DUPLICATE:
      print_echo (code == PEV_DUPLICATE,
		  (struct ping_stat *) closure, dest, from, ip, icmp,
		  datalen);
      break;
    case PEV_NOECHO:;
      print_icmp_header (from, ip, icmp, datalen);
    }
  return 0;
}

int
print_echo (int dupflag, struct ping_stat *ping_stat,
	    struct sockaddr_in *dest _GL_UNUSED_PARAMETER,
	    struct sockaddr_in *from,
	    struct ip *ip, icmphdr_t * icmp, int datalen)
{
  int hlen;
  struct timeval tv;
  int timing = 0;
  double triptime = 0.0;

  gettimeofday (&tv, NULL);

  /* Length of IP header */
  hlen = ip->ip_hl << 2;

  /* Length of ICMP header+payload */
  datalen -= hlen;

  /* Do timing */
  if (PING_TIMING (datalen - PING_HEADER_LEN))
    {
      struct timeval tv1, *tp;

      timing++;
      tp = (struct timeval *) icmp->icmp_data;

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

  printf ("%d bytes from %s: icmp_seq=%u", datalen,
	  inet_ntoa (*(struct in_addr *) &from->sin_addr.s_addr),
	  ntohs (icmp->icmp_seq));
  printf (" ttl=%d", ip->ip_ttl);
  if (timing)
    printf (" time=%.3f ms", triptime);
  if (dupflag)
    printf (" (DUP!)");

  print_ip_opt (ip, hlen);
  printf ("\n");

  return 0;
}

#define NITEMS(a) sizeof(a)/sizeof((a)[0])

struct icmp_diag
{
  int type;
  char *text;
  void (*fun) (icmphdr_t *, void *data);
  void *data;
};

struct icmp_code_descr
{
  int type;
  int code;
  char *diag;
} icmp_code_descr[] =
  {
    {ICMP_DEST_UNREACH, ICMP_NET_UNREACH, "Destination Net Unreachable"},
    {ICMP_DEST_UNREACH, ICMP_HOST_UNREACH, "Destination Host Unreachable"},
    {ICMP_DEST_UNREACH, ICMP_PROT_UNREACH, "Destination Protocol Unreachable"},
    {ICMP_DEST_UNREACH, ICMP_PORT_UNREACH, "Destination Port Unreachable"},
    {ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED, "Fragmentation needed and DF set"},
    {ICMP_DEST_UNREACH, ICMP_SR_FAILED, "Source Route Failed"},
    {ICMP_DEST_UNREACH, ICMP_NET_UNKNOWN, "Network Unknown"},
    {ICMP_DEST_UNREACH, ICMP_HOST_UNKNOWN, "Host Unknown"},
    {ICMP_DEST_UNREACH, ICMP_HOST_ISOLATED, "Host Isolated"},
    {ICMP_DEST_UNREACH, ICMP_NET_UNR_TOS, "Destination Network Unreachable At This TOS"},
    {ICMP_DEST_UNREACH, ICMP_HOST_UNR_TOS, "Destination Host Unreachable At This TOS"},
#ifdef ICMP_PKT_FILTERED
    {ICMP_DEST_UNREACH, ICMP_PKT_FILTERED, "Packet Filtered"},
#endif
#ifdef ICMP_PREC_VIOLATION
    {ICMP_DEST_UNREACH, ICMP_PREC_VIOLATION, "Precedence Violation"},
#endif
#ifdef ICMP_PREC_CUTOFF
    {ICMP_DEST_UNREACH, ICMP_PREC_CUTOFF, "Precedence Cutoff"},
#endif
    {ICMP_REDIRECT, ICMP_REDIR_NET, "Redirect Network"},
    {ICMP_REDIRECT, ICMP_REDIR_HOST, "Redirect Host"},
    {ICMP_REDIRECT, ICMP_REDIR_NETTOS, "Redirect Type of Service and Network"},
    {ICMP_REDIRECT, ICMP_REDIR_HOSTTOS, "Redirect Type of Service and Host"},
    {ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, "Time to live exceeded"},
    {ICMP_TIME_EXCEEDED, ICMP_EXC_FRAGTIME, "Frag reassembly time exceeded"}
  };

static void
print_icmp_code (int type, int code, char *prefix)
{
  struct icmp_code_descr *p;

  for (p = icmp_code_descr; p < icmp_code_descr + NITEMS (icmp_code_descr);
       p++)
    if (p->type == type && p->code == code)
      {
	printf ("%s\n", p->diag);
	return;
      }

  printf ("%s, Unknown Code: %d\n", prefix, code);
}

static void
print_ip_header (struct ip *ip)
{
  size_t hlen;
  unsigned char *cp;

  hlen = ip->ip_hl << 2;
  cp = (unsigned char *) ip + sizeof (*ip);	/* point to options */

  if (options & OPT_VERBOSE)
    {
      size_t j;

      printf ("IP Hdr Dump:\n ");
      for (j = 0; j < sizeof (*ip); ++j)
	printf ("%02x%s", *((unsigned char *) ip + j),
		(j % 2) ? " " : "");	/* Group bytes two by two.  */
      printf ("\n");
    }

  printf
    ("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src\tDst\tData\n");
  printf (" %1x  %1x  %02x", ip->ip_v, ip->ip_hl, ip->ip_tos);
  /*
   * The member `ip_len' is not portably reported in any byte order.
   * Use a simple heuristic to print a reasonable value.
   */
  printf (" %04x %04x",
	  (ip->ip_len > 0x2000) ? ntohs (ip->ip_len) : ip->ip_len,
	  ntohs (ip->ip_id));
  printf ("   %1x %04x", (ntohs (ip->ip_off) & 0xe000) >> 13,
	  ntohs (ip->ip_off) & 0x1fff);
  printf ("  %02x  %02x %04x", ip->ip_ttl, ip->ip_p, ntohs (ip->ip_sum));
  printf (" %s ", inet_ntoa (*((struct in_addr *) &ip->ip_src)));
  printf (" %s ", inet_ntoa (*((struct in_addr *) &ip->ip_dst)));
  while (hlen-- > sizeof (*ip))
    printf ("%02x", *cp++);

  printf ("\n");
}

void
print_ip_data (icmphdr_t * icmp, void *data _GL_UNUSED_PARAMETER)
{
  int hlen;
  unsigned char *cp;
  struct ip *ip = &icmp->icmp_ip;

  print_ip_header (ip);

  hlen = ip->ip_hl << 2;
  cp = (unsigned char *) ip + hlen;

  if (ip->ip_p == IPPROTO_TCP)
    printf ("TCP: from port %u, to port %u (decimal)\n",
	    (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
  else if (ip->ip_p == IPPROTO_UDP)
    printf ("UDP: from port %u, to port %u (decimal)\n",
	    (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
  else if (ip->ip_p == IPPROTO_ICMP)
    {
      int type = *cp;
      int code = *(cp + 1);

      printf ("ICMP: type %u, code %u, size %u", type, code,
	      ntohs (ip->ip_len) - hlen);
      if (type == ICMP_ECHOREPLY || type == ICMP_ECHO)
	printf (", id 0x%04x, seq 0x%04x", *(cp + 4) * 256 + *(cp + 5),
		*(cp + 6) * 256 + *(cp + 7));
      printf ("\n");
    }
}

static void
print_icmp (icmphdr_t * icmp, void *data)
{
  print_icmp_code (icmp->icmp_type, icmp->icmp_code, data);
  if (options & OPT_VERBOSE)
    print_ip_data (icmp, NULL);
}

static void
print_parameterprob (icmphdr_t * icmp, void *data)
{
  printf ("Parameter problem: IP address = %s\n",
	  inet_ntoa (icmp->icmp_gwaddr));
  print_ip_data (icmp, data);
}

struct icmp_diag icmp_diag[] = {
  {ICMP_ECHOREPLY, "Echo Reply", NULL, NULL},
  {ICMP_DEST_UNREACH, NULL, print_icmp, "Dest Unreachable"},
  {ICMP_SOURCE_QUENCH, "Source Quench", print_ip_data, NULL},
  {ICMP_REDIRECT, NULL, print_icmp, "Redirect"},
  {ICMP_ECHO, "Echo Request", NULL, NULL},
  {ICMP_ROUTERADV, "Router Advertisment", NULL, NULL},
  {ICMP_ROUTERDISCOVERY, "Router Discovery", NULL, NULL},
  {ICMP_TIME_EXCEEDED, NULL, print_icmp, "Time exceeded"},
  {ICMP_PARAMETERPROB, NULL, print_parameterprob, NULL},
  {ICMP_TIMESTAMP, "Timestamp", NULL, NULL},
  {ICMP_TIMESTAMPREPLY, "Timestamp Reply", NULL, NULL},
  {ICMP_INFO_REQUEST, "Information Request", NULL, NULL},
  {ICMP_INFO_REPLY, "Information Reply", NULL, NULL},
  {ICMP_ADDRESS, "Address Mask Request", NULL, NULL},
  {ICMP_ADDRESSREPLY, "Address Mask Reply", NULL, NULL},
};

void
print_icmp_header (struct sockaddr_in *from,
		   struct ip *ip, icmphdr_t * icmp, int len)
{
  int hlen;
  struct ip *orig_ip;
  char *s;
  struct icmp_diag *p;

  /* Length of the IP header */
  hlen = ip->ip_hl << 2;
  /* Original IP header */
  orig_ip = &icmp->icmp_ip;

  if (!(options & OPT_VERBOSE
	|| orig_ip->ip_dst.s_addr == ping->ping_dest.ping_sockaddr.sin_addr.s_addr))
    return;

  s = ipaddr2str ((struct sockaddr *) from, sizeof (*from));
  printf ("%d bytes from %s: ", len - hlen, s);
  free (s);

  for (p = icmp_diag; p < icmp_diag + NITEMS (icmp_diag); p++)
    {
      if (p->type == icmp->icmp_type)
	{
	  if (p->text)
	    printf ("%s\n", p->text);
	  if (p->fun)
	    p->fun (icmp, p->data);
	  return;
	}
    }
  printf ("Bad ICMP type: %d\n", icmp->icmp_type);
}

void
print_ip_opt (struct ip *ip, int hlen)
{
  unsigned char *cp;
  int i, j, k, l;
  static int old_rrlen;
  static char old_rr[MAX_IPOPTLEN];

  cp = (unsigned char *) (ip + 1);

  for (; hlen > (int) sizeof (struct ip); --hlen, ++cp)
    switch (*cp)
      {
      case IPOPT_EOL:
	hlen = 0;
	break;

      case IPOPT_LSRR:
      case IPOPT_SSRR:
	printf ("\n%cSRR: ", (*cp == IPOPT_LSRR) ? 'L' : 'S');
	hlen -= 2;
	j = *++cp;
	++cp;
	if (j > IPOPT_MINOFF)
	  for (;;)
	    {
	      /* Fetch in network byte order, calculate as host.  */
	      l = *++cp;
	      l = (l << 8) + *++cp;
	      l = (l << 8) + *++cp;
	      l = (l << 8) + *++cp;
	      if (l == 0)
		{
		  printf ("\t0.0.0.0");
		}
	      else
		{
		  struct in_addr ina;
		  char *s;

		  ina.s_addr = htonl (l);
		  printf ("\t%s", s = sinaddr2str (ina));
		  free (s);
		}
	      hlen -= 4;
	      j -= 4;
	      if (j <= IPOPT_MINOFF)
		break;
	      putchar ('\n');
	    }
	break;

      case IPOPT_RR:
	j = *++cp;
	i = *++cp;
	hlen -= 2;
	if (i > j)
	  i = j;
	i -= IPOPT_MINOFF;
	if (i <= 0)
	  break;
	if (i == old_rrlen
	    && cp == (unsigned char *) (ip + 1) + 2
	    && !memcmp ((char *) cp, old_rr, i) && !(options & OPT_FLOOD))
	  {
	    printf ("\t (same route)");
	    i = ((i + 3) / 4) * 4;
	    hlen -= i;
	    cp += i;
	    break;
	  }
	if (i < MAX_IPOPTLEN)
	  {
	    old_rrlen = i;
	    memmove (old_rr, cp, i);
	  }
	else
	  old_rrlen = 0;

	printf ("\nRR: ");
	j = 0;
	for (;;)
	  {
	    /* Fetch in network byte order, calculate as host.  */
	    l = *++cp;
	    l = (l << 8) + *++cp;
	    l = (l << 8) + *++cp;
	    l = (l << 8) + *++cp;
	    if (l == 0)
	      {
		printf ("\t0.0.0.0");
	      }
	    else
	      {
		struct in_addr ina;
		char *s;

		ina.s_addr = htonl (l);
		printf ("\t%s", s = sinaddr2str (ina));
		free (s);
	      }
	    hlen -= 4;
	    i -= 4;
	    j += 4;
	    if (i <= 0)
	      break;
	    if (j >= MAX_IPOPTLEN)
	      {
		printf ("\t (truncated route)");
		break;
	      }
	    putchar ('\n');
	  }
	break;

      case IPOPT_TS:
	j = *++cp;	/* len */
	i = *++cp;	/* ptr */
	hlen -= 2;
	if (i > j)
	  i = j;

	/* Check minimal sizing.  */
	if (j <= (int) (IPOPT_MINOFF + sizeof (n_time)))
	  break;

	k = *++cp;	/* OV, FL */
	++cp;		/* Points at first content.  */
	hlen -= 2;

	printf ("\nTS:");
	j = 5;		/* First possible slot.  */
	for (;;)
	  {
	    char timestr[16];

	    if ((k & 0x0f) != IPOPT_TS_TSONLY
		&& ((j / 4) % 2 == 1))	/* find 5, 13, 21, 29 */
	      {
		/* IP addresses */
		struct in_addr ina;
		char *s;

		ina.s_addr = *((in_addr_t *) cp);
		printf ("\t%s", s = sinaddr2str (ina));
		free (s);

		hlen -= sizeof (in_addr_t);
		cp += sizeof (in_addr_t);
		j += sizeof (in_addr_t);
	      }
	    else
	      {
		/* Timestamps */
		printf ("\t%s ms",
			ping_cvt_time (timestr, sizeof (timestr),
					ntohl (*(n_time *) cp)));
		if (options & OPT_VERBOSE)
		  printf (" = 0x%08x", ntohl (*(n_time *) cp));

		hlen -= sizeof (n_time);
		cp += sizeof (n_time);
		j += sizeof (n_time);

		putchar ('\n');
	      }

	    if (j >= i)
	      break;
	  }

	if (k & 0xf0)
	  printf ("\t(%u overflowing hosts)", k >> 4);
	break;

      case IPOPT_NOP:
	printf ("\nNOP");
	break;

      default:
	printf ("\nunknown option %x", *cp);
	break;
      }
}

int
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
