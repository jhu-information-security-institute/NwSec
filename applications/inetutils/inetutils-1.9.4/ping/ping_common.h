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

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/icmp6.h>
#include <icmp.h>
#include <error.h>
#include <progname.h>

#include <stdbool.h>

#define MAXWAIT         10	/* Max seconds to wait for response.  */
#define MAXPATTERN      16	/* Maximal length of pattern.  */

#define OPT_FLOOD       0x001
#define OPT_INTERVAL    0x002
#define OPT_NUMERIC     0x004
#define OPT_QUIET       0x008
#define OPT_RROUTE      0x010
#define OPT_VERBOSE     0x020
#define OPT_IPTIMESTAMP 0x040
#define OPT_FLOWINFO    0x080
#define OPT_TCLASS      0x100

#define SOPT_TSONLY     0x001
#define SOPT_TSADDR     0x002
#define SOPT_TSPRESPEC  0x004

struct ping_stat
{
  double tmin;                  /* minimum round trip time */
  double tmax;                  /* maximum round trip time */
  double tsum;                  /* sum of all times, for doing average */
  double tsumsq;                /* sum of all times squared, for std. dev. */
};

#define PEV_RESPONSE 0
#define PEV_DUPLICATE 1
#define PEV_NOECHO  2

#define PING_CKTABSIZE 128

/* The rationale for not exiting after a sending N packets is that we
   want to follow the traditional behaviour of ping.  */
#define DEFAULT_PING_COUNT 0

#define PING_HEADER_LEN (USE_IPV6 ? sizeof (struct icmp6_hdr) : ICMP_MINLEN)
#define PING_TIMING(s)  ((s) >= sizeof (struct timeval))
#define PING_DATALEN    (64 - PING_HEADER_LEN)  /* default data length */

#define PING_DEFAULT_INTERVAL 1000      /* Milliseconds */
#define PING_PRECISION 1000     /* Millisecond precision */

#define PING_SET_INTERVAL(t,i) do {\
  (t).tv_sec = (i)/PING_PRECISION;\
  (t).tv_usec = ((i)%PING_PRECISION)*(1000000/PING_PRECISION) ;\
} while (0)

/* FIXME: Adjust IPv6 case for options and their consumption.  */
#define _PING_BUFLEN(p, u) ((u)? ((p)->ping_datalen + sizeof (struct icmp6_hdr)) : \
				   (MAXIPLEN + (p)->ping_datalen + ICMP_TSLEN))

typedef int (*ping_efp6) (int code, void *closure, struct sockaddr_in6 * dest,
			  struct sockaddr_in6 * from, struct icmp6_hdr * icmp,
			  int datalen);

typedef int (*ping_efp) (int code,
			 void *closure,
			 struct sockaddr_in * dest,
			 struct sockaddr_in * from,
			 struct ip * ip, icmphdr_t * icmp, int datalen);

union event {
  ping_efp6 handler6;
  ping_efp handler;
};

union ping_address {
  struct sockaddr_in ping_sockaddr;
  struct sockaddr_in6 ping_sockaddr6;
};

typedef struct ping_data PING;

struct ping_data
{
  int ping_fd;                 /* Raw socket descriptor */
  int ping_type;               /* Type of packets to send */
  size_t ping_count;           /* Number of packets to send */
  struct timeval ping_start_time; /* Start time */
  size_t ping_interval;        /* Number of seconds to wait between sending pkts */
  union ping_address ping_dest;/* whom to ping */
  char *ping_hostname;         /* Printable hostname */
  size_t ping_datalen;         /* Length of data */
  int ping_ident;              /* Our identifier */
  union event ping_event;      /* User-defined handler */
  void *ping_closure;          /* User-defined data */

  /* Runtime info */
  int ping_cktab_size;
  char *ping_cktab;

  unsigned char *ping_buffer;         /* I/O buffer */
  union ping_address ping_from;
  size_t ping_num_xmit;        /* Number of packets transmitted */
  size_t ping_num_recv;        /* Number of packets received */
  size_t ping_num_rept;        /* Number of duplicates received */
};

#define _C_BIT(p,bit)   (p)->ping_cktab[(bit)>>3]	/* byte in ck array */
#define _C_MASK(bit)    (1 << ((bit) & 0x07))
#define _C_IND(p,bit)   ((bit) % (8 * (p)->ping_cktab_size))

#define _PING_SET(p,bit)						\
  do									\
    {									\
      int n = _C_IND (p,bit);						\
      _C_BIT (p,n) |= _C_MASK (n);					\
    }									\
  while (0)

#define _PING_CLR(p,bit)						\
  do									\
    {									\
      int n = _C_IND (p,bit);						\
      _C_BIT (p,n) &= ~_C_MASK (n);					\
    }									\
  while (0)

#define _PING_TST(p,bit)					\
  (_C_BIT (p, _C_IND (p,bit)) & _C_MASK  (_C_IND (p,bit)))


void tvsub (struct timeval *out, struct timeval *in);
double nabs (double a);
double nsqrt (double a, double prec);

size_t ping_cvt_number (const char *optarg, size_t maxval, int allow_zero);
int is_normed_time (n_time t);
const char * ping_cvt_time (char *buf, size_t buflen, n_time t);

void init_data_buffer (unsigned char *pat, size_t len);

void decode_pattern (const char *text, int *pattern_len,
		     unsigned char *pattern_data);
int _ping_setbuf (PING * p, bool use_ipv6);
int ping_set_data (PING *p, void *data, size_t off, size_t len, bool use_ipv6);
void ping_set_count (PING * ping, size_t count);
void ping_set_sockopt (PING * ping, int opt, void *val, int valsize);
void ping_set_interval (PING * ping, size_t interval);
void ping_unset_data (PING * p);
int ping_timeout_p (struct timeval *start_time, int timeout);

char * ipaddr2str (struct sockaddr *from, socklen_t fromlen);
char * sinaddr2str (struct in_addr ina);
