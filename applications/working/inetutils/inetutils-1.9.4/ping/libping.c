/*
  Copyright (C) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

#include <netinet/in.h>
/*#include <netinet/ip_icmp.h> -- deliberately not including this */
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_IDNA_H
# include <idna.h>
#endif

#include "ping.h"

static size_t _ping_packetsize (PING * p);

size_t
_ping_packetsize (PING * p)
{
  if (p->ping_type == ICMP_TIMESTAMP || p->ping_type == ICMP_TIMESTAMPREPLY)
    return ICMP_TSLEN;

  if (p->ping_type == ICMP_ADDRESS || p->ping_type == ICMP_ADDRESSREPLY)
    return ICMP_MASKLEN;

  return PING_HEADER_LEN + p->ping_datalen;
}

PING *
ping_init (int type, int ident)
{
  int fd;
  struct protoent *proto;
  PING *p;

  /* Initialize raw ICMP socket */
  proto = getprotobyname ("icmp");
  if (!proto)
    {
      fprintf (stderr, "ping: unknown protocol icmp.\n");
      return NULL;
    }

  fd = socket (AF_INET, SOCK_RAW, proto->p_proto);
  if (fd < 0)
    {
      if (errno == EPERM || errno == EACCES)
	fprintf (stderr, "ping: Lacking privilege for raw socket.\n");
      return NULL;
    }

  /* Allocate PING structure and initialize it to default values */
  p = malloc (sizeof (*p));
  if (!p)
    {
      close (fd);
      return p;
    }

  memset (p, 0, sizeof (*p));

  p->ping_fd = fd;
  p->ping_type = type;
  p->ping_count = 0;
  p->ping_interval = PING_DEFAULT_INTERVAL;
  p->ping_datalen = sizeof (icmphdr_t);
  /* Make sure we use only 16 bits in this field, id for icmp is a unsigned short.  */
  p->ping_ident = ident & 0xFFFF;
  p->ping_cktab_size = PING_CKTABSIZE;
  gettimeofday (&p->ping_start_time, NULL);
  return p;
}

void
ping_reset (PING * p)
{
  p->ping_num_xmit = 0;
  p->ping_num_recv = 0;
  p->ping_num_rept = 0;
}

void
ping_set_type (PING * p, int type)
{
  p->ping_type = type;
}

int
ping_xmit (PING * p)
{
  int i, buflen;

  if (_ping_setbuf (p, USE_IPV6))
    return -1;

  buflen = _ping_packetsize (p);

  /* Mark sequence number as sent */
  _PING_CLR (p, p->ping_num_xmit);

  /* Encode ICMP header */
  switch (p->ping_type)
    {
    case ICMP_ECHO:
      icmp_echo_encode (p->ping_buffer, buflen, p->ping_ident,
			p->ping_num_xmit);
      break;

    case ICMP_TIMESTAMP:
      icmp_timestamp_encode (p->ping_buffer, buflen, p->ping_ident,
			     p->ping_num_xmit);
      break;

    case ICMP_ADDRESS:
      icmp_address_encode (p->ping_buffer, buflen, p->ping_ident,
			   p->ping_num_xmit);
      break;

    default:
      icmp_generic_encode (p->ping_buffer, buflen, p->ping_type,
			   p->ping_ident, p->ping_num_xmit);
      break;
    }

  i = sendto (p->ping_fd, (char *) p->ping_buffer, buflen, 0,
	      (struct sockaddr *) &p->ping_dest.ping_sockaddr, sizeof (struct sockaddr_in));
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
my_echo_reply (PING * p, icmphdr_t * icmp)
{
  struct ip *orig_ip = &icmp->icmp_ip;
  icmphdr_t *orig_icmp = (icmphdr_t *) (orig_ip + 1);

  return (orig_ip->ip_dst.s_addr == p->ping_dest.ping_sockaddr.sin_addr.s_addr
	  && orig_ip->ip_p == IPPROTO_ICMP
	  && orig_icmp->icmp_type == ICMP_ECHO
	  && ntohs (orig_icmp->icmp_id) == p->ping_ident);
}

int
ping_recv (PING * p)
{
  socklen_t fromlen = sizeof (p->ping_from.ping_sockaddr);
  int n, rc;
  icmphdr_t *icmp;
  struct ip *ip;
  int dupflag;

  n = recvfrom (p->ping_fd,
		(char *) p->ping_buffer, _PING_BUFLEN (p, USE_IPV6), 0,
		(struct sockaddr *) &p->ping_from.ping_sockaddr, &fromlen);
  if (n < 0)
    return -1;

  rc = icmp_generic_decode (p->ping_buffer, n, &ip, &icmp);
  if (rc < 0)
    {
      /*FIXME: conditional */
      fprintf (stderr, "packet too short (%d bytes) from %s\n", n,
	       inet_ntoa (p->ping_from.ping_sockaddr.sin_addr));
      return -1;
    }

  switch (icmp->icmp_type)
    {
    case ICMP_ECHOREPLY:
    case ICMP_TIMESTAMPREPLY:
    case ICMP_ADDRESSREPLY:
      /*    case ICMP_ROUTERADV: */

      if (ntohs (icmp->icmp_id) != p->ping_ident)
	return -1;

      if (rc)
	fprintf (stderr, "checksum mismatch from %s\n",
		 inet_ntoa (p->ping_from.ping_sockaddr.sin_addr));

      p->ping_num_recv++;
      if (_PING_TST (p, ntohs (icmp->icmp_seq)))
	{
	  p->ping_num_rept++;
	  p->ping_num_recv--;
	  dupflag = 1;
	}
      else
	{
	  _PING_SET (p, ntohs (icmp->icmp_seq));
	  dupflag = 0;
	}

      if (p->ping_event.handler)
	(*p->ping_event.handler) (dupflag ? PEV_DUPLICATE : PEV_RESPONSE,
				  p->ping_closure,
				  &p->ping_dest.ping_sockaddr,
				  &p->ping_from.ping_sockaddr, ip, icmp, n);
      break;

    case ICMP_ECHO:
    case ICMP_TIMESTAMP:
    case ICMP_ADDRESS:
      return -1;

    default:
      if (!my_echo_reply (p, icmp))
	return -1;

      if (p->ping_event.handler)
	(*p->ping_event.handler) (PEV_NOECHO,
				  p->ping_closure,
				  &p->ping_dest.ping_sockaddr,
				  &p->ping_from.ping_sockaddr, ip, icmp, n);
    }
  return 0;
}

void
ping_set_event_handler (PING * ping, ping_efp pf, void *closure)
{
  ping->ping_event.handler = pf;
  ping->ping_closure = closure;
}

void
ping_set_packetsize (PING * ping, size_t size)
{
  ping->ping_datalen = size;
}

int
ping_set_dest (PING * ping, char *host)
{
#if HAVE_DECL_GETADDRINFO
  int rc;
  struct addrinfo hints, *res;
  char *p;

# ifdef HAVE_IDN
  rc = idna_to_ascii_lz (host, &p, 0);	/* P is allocated.  */
  if (rc)
    return 1;
# else /* !HAVE_IDN */
  p = host;
# endif

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET;
  hints.ai_flags = AI_CANONNAME;
# ifdef AI_IDN
  hints.ai_flags |= AI_IDN;
# endif
# ifdef AI_CANONIDN
  hints.ai_flags |= AI_CANONIDN;
# endif

  rc = getaddrinfo (p, NULL, &hints, &res);

  if (rc)
    return 1;

  memcpy (&ping->ping_dest.ping_sockaddr, res->ai_addr, res->ai_addrlen);
  if (res->ai_canonname)
    ping->ping_hostname = strdup (res->ai_canonname);
  else
    ping->ping_hostname = strdup (p);

# ifdef HAVE_IDN
  free (p);
# endif
  freeaddrinfo (res);

  return 0;
#else /* !HAVE_DECL_GETADDRINFO */

  struct sockaddr_in *s_in = &ping->ping_dest.ping_sockaddr;
  s_in->sin_family = AF_INET;
# ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  s_in->sin_len = sizeof (*s_in);
# endif
  if (inet_aton (host, &s_in->sin_addr))
    ping->ping_hostname = strdup (host);
  else
    {
      struct hostent *hp;
# ifdef HAVE_IDN
      char *p;
      int rc;

      rc = idna_to_ascii_lz (host, &p, 0);
      if (rc)
	return 1;
      hp = gethostbyname (p);
      free (p);
# else /* !HAVE_IDN */
      hp = gethostbyname (host);
# endif
      if (!hp)
	return 1;

      s_in->sin_family = hp->h_addrtype;
      if (hp->h_length > (int) sizeof (s_in->sin_addr))
	hp->h_length = sizeof (s_in->sin_addr);

      memcpy (&s_in->sin_addr, hp->h_addr, hp->h_length);
      ping->ping_hostname = strdup (hp->h_name);
    }
  return 0;
#endif /* !HAVE_DECL_GETADDRINFO */
}
