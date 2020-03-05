/* flags.c -- network interface flag handling
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

/* Written by Marcus Brinkmann.  */

#include <config.h>

#include <stdio.h>

#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdlib.h>
#include "ifconfig.h"
#include "xalloc.h"

/* Conversion table for interface flag names.
   The mask must be a power of 2.  */
struct if_flag
{
  const char *name;
  int mask;
  int rev;
} if_flags[] =
  {
    /* Available on all systems which derive the network interface from
       BSD. Verified for GNU, Linux 2.4, FreeBSD, Solaris 2.7, HP-UX
       10.20 and OSF 4.0g.  */

#ifdef IFF_UP			/* Interface is up.  */
    {"UP", IFF_UP, 0},
#endif
#ifdef IFF_BROADCAST		/* Broadcast address is valid.  */
    {"BROADCAST", IFF_BROADCAST, 0},
#endif
#ifdef IFF_DEBUG		/* Debugging is turned on.  */
    {"DEBUG", IFF_DEBUG, 0},
#endif
#ifdef IFF_LOOPBACK		/* Is a loopback net.  */
    {"LOOPBACK", IFF_LOOPBACK, 0},
#endif
#ifdef IFF_POINTOPOINT		/* Interface is a point-to-point link.  */
    {"POINTOPOINT", IFF_POINTOPOINT, 0},
#endif
#ifdef IFF_RUNNING		/* Resources allocated.  */
    {"RUNNING", IFF_RUNNING, 0},
#endif
#ifdef IFF_NOARP		/* No address resolution protocol.  */
    {"NOARP", IFF_NOARP, 0},
    {"ARP", IFF_NOARP, 1},
#endif
/* Keep IFF_PPROMISC prior to IFF_PROMISC
 * for precedence in parsing.
 */
#ifdef IFF_PPROMISC		/* User accessible promiscuous mode.  */
    {"PROMISC", IFF_PPROMISC, 0},
#endif
#ifdef IFF_PROMISC		/* Receive all packets.  */
    {"PROMISC", IFF_PROMISC, 0},
#endif
#ifdef IFF_ALLMULTI		/* Receive all multicast packets.  */
    {"ALLMULTI", IFF_ALLMULTI, 0},
#endif
#ifdef IFF_MULTICAST		/* Supports multicast.  */
    {"MULTICAST", IFF_MULTICAST, 0},
#endif
    /* Usually available on all systems which derive the network
       interface from BSD (see above), but with exceptions noted.  */
#ifdef IFF_NOTRAILERS		/* Avoid use of trailers.  */
    /* Obsoleted on FreeBSD systems.  */
    {"NOTRAILERS", IFF_NOTRAILERS, 0},
    {"TRAILERS", IFF_NOTRAILERS, 1},
#endif
    /* Available on GNU and Linux systems.  */
#ifdef IFF_MASTER		/* Master of a load balancer.  */
    {"MASTER", IFF_MASTER, 0},
#endif
#ifdef IFF_SLAVE		/* Slave of a load balancer.  */
    {"SLAVE", IFF_SLAVE, 0},
#endif
#ifdef IFF_PORTSEL		/* Can set media type.  */
    {"PORTSEL", IFF_PORTSEL, 0},
#endif
#ifdef IFF_AUTOMEDIA		/* Auto media select is active.  */
    {"AUTOMEDIA", IFF_AUTOMEDIA, 0},
#endif
    /* Available on Linux 2.4 systems (not glibc <= 2.2.1).  */
#ifdef IFF_DYNAMIC		/* Dialup service with hanging addresses.  */
    {"DYNAMIC", IFF_DYNAMIC, 0},
#endif
    /* Available on FreeBSD and OSF 4.0g systems.  */
#ifdef IFF_OACTIVE		/* Transmission is in progress.  */
    {"OACTIVE", IFF_OACTIVE, 0},
#endif
#ifdef IFF_SIMPLEX		/* Can't hear own transmissions.  */
    {"SIMPLEX", IFF_SIMPLEX, 0},
#endif
    /* Available on FreeBSD systems.  */
#ifdef IFF_LINK0		/* Per link layer defined bit.  */
    {"LINK0", IFF_LINK0, 0},
#endif
#ifdef IFF_LINK1		/* Per link layer defined bit.  */
    {"LINK1", IFF_LINK1, 0},
#endif
#if defined IFF_LINK2 && defined IFF_ALTPHYS
# if IFF_LINK2 == IFF_ALTPHYS
    /* IFF_ALTPHYS == IFF_LINK2 on FreeBSD.  This entry is used as a
       fallback for if_flagtoname conversion, if no relevant EXPECT_
       macro is specified to figure out which one is meant.  */
    {"LINK2/ALTPHYS", IFF_LINK2, 0},
# endif
#endif
#ifdef IFF_LINK2		/* Per link layer defined bit.  */
    {"LINK2", IFF_LINK2, 0},
#endif
#ifdef IFF_ALTPHYS		/* Use alternate physical connection.  */
    {"ALTPHYS", IFF_ALTPHYS, 0},
#endif
    /* Available on Solaris 2.7 systems.  */
#ifdef IFF_INTELLIGENT		/* Protocol code on board.  */
    {"INTELLIGENT", IFF_INTELLIGENT, 0},
#endif
#ifdef IFF_MULTI_BCAST		/* Multicast using broadcast address.  */
    {"MULTI_BCAST", IFF_MULTI_BCAST, 0},
#endif
#ifdef IFF_UNNUMBERED		/* Address is not unique.  */
    {"UNNUMBERED", IFF_UNNUMBERED, 0},
#endif
#ifdef IFF_DHCPRUNNING		/* Interface is under control of DHCP.  */
    {"DHCPRUNNING", IFF_DHCPRUNNING, 0},
#endif
#ifdef IFF_PRIVATE		/* Do not advertise.  */
    {"PRIVATE", IFF_PRIVATE, 0},
#endif
    /* Available on HP-UX 10.20 systems.  */
#ifdef IFF_NOTRAILERS		/* Avoid use of trailers.  */
    {"NOTRAILERS", IFF_NOTRAILERS, 0},
    {"TRAILERS", IFF_NOTRAILERS, 1},
#endif
#ifdef IFF_LOCALSUBNETS		/* Subnets of this net are local.  */
    {"LOCALSUBNETS", IFF_LOCALSUBNETS, 0},
#endif
#ifdef IFF_CKO			/* Interface supports header checksum.  */
    {"CKO", IFF_CKO, 0},
#endif
#ifdef IFF_NOACC		/* No data access on outbound.  */
    {"NOACC", IFF_NOACC, 0},
    {"ACC", IFF_NOACC, 1},
#endif
#ifdef IFF_NOSR8025		/* No source route 802.5.  */
    {"NOSR8025", IFF_NOSR8025, 0},
    {"SR8025", IFF_NOSR8025, 1},
#endif
#ifdef IFF_CKO_ETC		/* Interface supports trailer checksum.  */
    {"CKO_ETC", IFF_CKO_ETC, 0},
#endif
#ifdef IFF_AR_SR8025		/* All routes broadcast for ARP 8025.  */
    {"AR_SR8025", IFF_AR_SR8025, 0},
#endif
#ifdef IFF_ALT_SR8025		/* Alternating no rif, rif for ARP on.  */
    {"ALT_SR8025", IFF_ALT_SR8025, 0},
#endif
    /* Defined on OSF 4.0g systems.  */
#ifdef IFF_PFCOPYALL		/* PFILT gets packets to this host.  */
    {"PFCOPYALL", IFF_PFCOPYALL, 0},
#endif
#ifdef IFF_UIOMOVE		/* DART.  */
    {"UIOMOVE", IFF_UIOMOVE, 0},
#endif
#ifdef IFF_PKTOK		/* DART.  */
    {"PKTOK", IFF_PKTOK, 0},
#endif
#ifdef IFF_SOCKBUF		/* DART.  */
    {"SOCKBUF", IFF_SOCKBUF, 0},
#endif
#ifdef IFF_VAR_MTU		/* Interface supports variable MTUs.  */
    {"VAR_MTU", IFF_VAR_MTU, 0},
#endif
#ifdef IFF_NOCHECKSUM		/* No checksums needed (reliable media).  */
    {"NOCHECKSUM", IFF_NOCHECKSUM, 0},
    {"CHECKSUM", IFF_NOCHECKSUM, 1},
#endif
#ifdef IFF_MULTINET		/* Multiple networks on interface.  */
    {"MULTINET", IFF_MULTINET, 0},
#endif
#ifdef IFF_VMIFNET		/* Used to identify a virtual MAC address.  */
    {"VMIFNET", IFF_VMIFNET, 0},
#endif
#if defined IFF_D1 && defined IFF_SNAP
# if IFF_D1 == IFF_SNAP
    /* IFF_SNAP == IFF_D1 on OSF 4.0g systems.  This entry is used as a
       fallback for if_flagtoname conversion, if no relevant EXPECT_
       macro is specified to figure out which one is meant.  */
    {"D1/SNAP", IFF_D2, 0},
# endif
#endif
#ifdef IFF_D2			/* Flag is specific to device.  */
    {"D2", IFF_D2, 0},
#endif
#ifdef IFF_SNAP			/* Ethernet driver outputs SNAP header.  */
    {"SNAP", IFF_SNAP, 0},
#endif
#ifdef IFF_MONITOR
    {"MONITOR", IFF_MONITOR, 0},
#endif
#ifdef IFF_STATICARP
    {"STATICARP", IFF_STATICARP, 0},
#endif
    { NULL, 0, 0}
  };

static int
cmpname (const void *a, const void *b)
{
  return strcmp (*(const char**)a, *(const char**)b);
}

char *
if_list_flags (const char *prefix)
{
#define FLAGS_COMMENT "\nPrepend 'no' to negate the effect."
  size_t len = 0;
  struct if_flag *fp;
  char **fnames;
  size_t i, fcount;
  char *str, *p;

  for (fp = if_flags, len = 0, fcount = 0; fp->name; fp++)
    if (!fp->rev)
      {
	fcount++;
	len += strlen (fp->name) + 1;
      }

  fcount = sizeof (if_flags) / sizeof (if_flags[0]) - 1;
  fnames = xmalloc (fcount * sizeof (fnames[0]) + len);
  p = (char*)(fnames + fcount);

  for (fp = if_flags, i = 0; fp->name; fp++)
    if (!fp->rev)
      {
	const char *q;

	if (fp->mask & IU_IFF_CANTCHANGE)
	  continue;

	fnames[i++] = p;
	q = fp->name;
	if (strncmp (q, "NO", 2) == 0)
	  q += 2;
	for (; *q; q++)
	  *p++ = tolower (*q);
	*p++ = 0;
      }
  fcount = i;
  qsort (fnames, fcount, sizeof (fnames[0]), cmpname);

  len += strlen (", ") * fcount;	/* Delimiters  */

  if (prefix)
    len += strlen (prefix);

  len += strlen (FLAGS_COMMENT);

  str = xmalloc (len + 1);
  p = str;
  if (prefix)
    {
      strcpy (p, prefix);
      p += strlen (prefix);
    }

  for (i = 0; i < fcount; i++)
    {
      /* Omit repeated, or alternate names, like "link2/altphys".  */
      if (i && strncmp (fnames[i - 1], fnames[i],
			strlen (fnames[i - 1])) == 0)
	continue;
      strcpy (p, fnames[i]);
      p += strlen (fnames[i]);
      if (i + 1 < fcount)
	{
	  *p++ = ',';
	  *p++ = ' ';
	}
    }
  strcpy (p, FLAGS_COMMENT);
  p += strlen (FLAGS_COMMENT);
#undef FLAGS_COMMENT
  *p = 0;
  free (fnames);
  return str;
}

/* Return the name corresponding to the interface flag FLAG.
   If FLAG is unknown, return NULL.
   AVOID contains a ':' surrounded and separated list of flag names
   that should be avoided if alternative names with the same flag value
   exists.  The first unavoided match is returned, or the first avoided
   match if no better is available.  */
const char *
if_flagtoname (int flag, const char *avoid)
{
  struct if_flag *fp;
  const char *first_match = NULL;
  char *start;

  for (fp = if_flags; ; fp++)
    {
      if (!fp->name)
	return NULL;
      if (flag == fp->mask && !fp->rev)
	break;
    }

  first_match = fp->name;

  /* We now have found the first match.  Look for a better one.  */
  if (avoid)
    do
      {
	start = strstr (avoid, fp->name);
	if (!start || *(start - 1) != ':'
	    || *(start + strlen (fp->name)) != ':')
	  break;
	fp++;
      }
    while (fp->name);

  if (fp->name)
    return fp->name;
  else
    return first_match;
}

int
if_nametoflag (const char *name, size_t len, int *prev)
{
  struct if_flag *fp;
  int rev = 0;

  if (len > 1 && name[0] == '-')
    {
      name++;
      len--;
      rev = 1;
    }
  else if (len > 2 && strncasecmp (name, "NO", 2) == 0)
    {
      name += 2;
      len -= 2;
      rev = 1;
    }

  for (fp = if_flags; fp->name; fp++)
    {
      if (strncasecmp (fp->name, name, len) == 0)
	{
	  *prev = fp->rev ^ rev;
	  return fp->mask;
	}
    }
  return 0;
}

int
if_nameztoflag (const char *name, int *prev)
{
  return if_nametoflag (name, strlen (name), prev);
}

struct if_flag_char
{
  int mask;
  int ch;
};

/* Interface flag bits and the corresponding letters for short output.
   Notice that the entries must be ordered alphabetically, by the letter name.
   There are two lamentable exceptions:

   1. The 'd' is misplaced.
   2. The 'P' letter is ambiguous. Depending on its position in the output
      line it may stand for IFF_PROMISC or IFF_POINTOPOINT.

   That's the way netstat does it.
*/
static struct if_flag_char flag_char_tab[] = {
#ifdef IFF_ALLMULTI
  { IFF_ALLMULTI,    'A' },
#endif
#ifdef IFF_BROADCAST
  { IFF_BROADCAST,   'B' },
#endif
#ifdef IFF_DEBUG
  { IFF_DEBUG,       'D' },
#endif
#ifdef IFF_LOOPBACK
  { IFF_LOOPBACK,    'L' },
#endif
#ifdef IFF_MULTICAST
  { IFF_MULTICAST,   'M' },
#endif
#ifdef HAVE_DYNAMIC
  { IFF_DYNAMIC,     'd' },
#endif
#ifdef IFF_PROMISC
  { IFF_PROMISC,     'P' },
#endif
#ifdef IFF_NOTRAILERS
  { IFF_NOTRAILERS,  'N' },
#endif
#ifdef IFF_NOARP
  { IFF_NOARP,       'O' },
#endif
#ifdef IFF_POINTOPOINT
  { IFF_POINTOPOINT, 'P' },
#endif
#ifdef IFF_SLAVE
  { IFF_SLAVE,       's' },	/* Linux */
#endif
#ifdef IFF_STATICARP
  { IFF_STATICARP,   's' },	/* FreeBSD */
#endif
#ifdef IFF_MASTER
  { IFF_MASTER,      'm' },	/* Linux */
#endif
#ifdef IFF_MONITOR
  { IFF_MONITOR,     'm' },	/* FreeBSD */
#endif
#ifdef IFF_SIMPLEX
  { IFF_SIMPLEX,     'S' },
#endif
#ifdef IFF_RUNNING
  { IFF_RUNNING,     'R' },
#endif
#ifdef IFF_UP
  { IFF_UP,          'U' },
#endif
  { 0, 0 }
};

void
if_format_flags (int flags, char *buf, size_t size)
{
  struct if_flag_char *fp;
  size--;
  for (fp = flag_char_tab; size && fp->mask; fp++)
    if (fp->mask & flags)
      {
	*buf++ = fp->ch;
	size--;
      }
  *buf = 0;
}


/* Print the flags in FLAGS, using AVOID as in if_flagtoname, and
   SEPARATOR between individual flags.  Returns the number of
   characters printed.  */
int
print_if_flags (int flags, const char *avoid, char separator)
{
  int f = 1;
  const char *name;
  int first = 1;
  int length = 0;

  while (flags && f)
    {
      if (f & flags)
	{
	  name = if_flagtoname (f, avoid);
	  if (name)
	    {
	      if (!first)
		{
		  putchar (separator);
		  length++;
		}
	      length += printf ("%s", name);
	      flags &= ~f;
	      first = 0;
	    }
	}
      f = f << 1;
    }
  if (flags)
    {
      if (!first)
	{
	  putchar (separator);
	  length++;
	}
      length += printf ("%#x", flags);
    }
  return length;
}
