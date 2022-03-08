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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
/*#include <netinet/ip_icmp.h> -- deliberately not including this */
#include <arpa/inet.h>
#include <icmp.h>

int
icmp_generic_encode (unsigned char * buffer, size_t bufsize, int type, int ident,
		     int seqno)
{
  icmphdr_t *icmp;

  if (bufsize < ICMP_MINLEN)
    return -1;
  icmp = (icmphdr_t *) buffer;
  icmp->icmp_type = type;
  icmp->icmp_code = 0;
  icmp->icmp_cksum = 0;
  icmp->icmp_seq = htons (seqno);
  icmp->icmp_id = htons (ident);

  icmp->icmp_cksum = icmp_cksum (buffer, bufsize);
  return 0;
}

int
icmp_generic_decode (unsigned char * buffer, size_t bufsize,
		     struct ip **ipp, icmphdr_t ** icmpp)
{
  size_t hlen;
  unsigned short cksum;
  struct ip *ip;
  icmphdr_t *icmp;

  /* IP header */
  ip = (struct ip *) buffer;
  hlen = ip->ip_hl << 2;
  if (bufsize < hlen + ICMP_MINLEN)
    return -1;

  /* ICMP header */
  icmp = (icmphdr_t *) (buffer + hlen);

  /* Prepare return values */
  *ipp = ip;
  *icmpp = icmp;

  /* Recompute checksum */
  cksum = icmp->icmp_cksum;
  icmp->icmp_cksum = 0;
  icmp->icmp_cksum = icmp_cksum ((unsigned char *) icmp, bufsize - hlen);
  if (icmp->icmp_cksum != cksum)
    return 1;
  return 0;
}

int
icmp_echo_encode (unsigned char * buffer, size_t bufsize, int ident, int seqno)
{
  return icmp_generic_encode (buffer, bufsize, ICMP_ECHO, ident, seqno);
}

int
icmp_echo_decode (unsigned char * buffer, size_t bufsize,
		  struct ip **ipp, icmphdr_t ** icmpp)
{
  return icmp_generic_decode (buffer, bufsize, ipp, icmpp);
}
