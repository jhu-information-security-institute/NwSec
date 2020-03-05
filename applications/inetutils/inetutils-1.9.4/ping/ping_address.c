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

/* NOTE: most of existing routers simply discard ICMP_ADDRESS requests. */
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
#include <unused-parameter.h>

#include <ping.h>
#include <ping_impl.h>

static int recv_address (int code, void *closure,
			 struct sockaddr_in *dest, struct sockaddr_in *from,
			 struct ip *ip, icmphdr_t * icmp, int datalen);
static void print_address (int dupflag, void *closure,
			   struct sockaddr_in *dest, struct sockaddr_in *from,
			   struct ip *ip, icmphdr_t * icmp, int datalen);
static int address_finish (void);

int
ping_address (char *hostname)
{
  ping_set_type (ping, ICMP_ADDRESS);
  ping_set_event_handler (ping, recv_address, NULL);
  ping_set_packetsize (ping, ICMP_MASKLEN);
  ping_set_count (ping, 1);

  if (ping_set_dest (ping, hostname))
    error (EXIT_FAILURE, 0, "unknown host");

  printf ("PING %s (%s): sending address mask request\n",
	  ping->ping_hostname, inet_ntoa (ping->ping_dest.ping_sockaddr.sin_addr));

  return ping_run (ping, address_finish);
}


int
recv_address (int code, void *closure,
	      struct sockaddr_in *dest, struct sockaddr_in *from,
	      struct ip *ip, icmphdr_t * icmp, int datalen)
{
  switch (code)
    {
    case PEV_RESPONSE:
    case PEV_DUPLICATE:
      print_address (code == PEV_DUPLICATE,
		     closure, dest, from, ip, icmp, datalen);
      break;
    case PEV_NOECHO:;
      print_icmp_header (from, ip, icmp, datalen);
    }
  return 0;
}

void
print_address (int dupflag, void *closure _GL_UNUSED_PARAMETER,
	       struct sockaddr_in *dest _GL_UNUSED_PARAMETER,
	       struct sockaddr_in *from,
	       struct ip *ip _GL_UNUSED_PARAMETER,
	       icmphdr_t * icmp, int datalen)
{
  struct in_addr addr;

  printf ("%d bytes from %s: icmp_seq=%u", datalen,
	  inet_ntoa (*(struct in_addr *) &from->sin_addr.s_addr),
	  ntohs (icmp->icmp_seq));
  if (dupflag)
    printf (" (DUP!)");
  printf ("\n");
  addr.s_addr = icmp->icmp_mask;
  printf ("icmp_mask = %s", inet_ntoa (addr));
  printf ("\n");
  return;
}


int
address_finish (void)
{
  return 0;
}
