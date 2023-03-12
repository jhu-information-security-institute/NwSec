/* changeif.c -- change the configuration of a network interface
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
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "ifconfig.h"

/* Necessary for BSD systems.  */
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
# define SET_SIN_LEN	sin->sin_len = sizeof (struct sockaddr_in);
#else /* !HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
# define SET_SIN_LEN
#endif

#define SIOCSIF(type, addr)						\
  int err = 0;								\
  struct sockaddr_in *sin = (struct sockaddr_in *) &ifr->ifr_addr;	\
									\
  sin->sin_family = AF_INET;						\
  err = inet_aton (addr, &sin->sin_addr);				\
  if (!err)								\
    {									\
      error (0, 0, "`%s' is not a valid address", addr);		\
      return -1;							\
    }									\
  SET_SIN_LEN								\
  err = ioctl (sfd, SIOCSIF##type, ifr);				\
  if (err < 0)								\
    {									\
      error (0, errno, "%s failed", "SIOCSIF" #type);			\
      return -1;							\
    }

#if !HAVE_DECL_GETADDRINFO
extern void herror (const char *pfx);
#endif

/* Set address of interface in IFR. Destroys union in IFR, but leaves
   ifr_name intact.  ADDRESS may be an IP number or a hostname that
   can be resolved.  */
int
set_address (int sfd, struct ifreq *ifr, char *address)
{
#ifndef SIOCSIFADDR
  error (0, 0,
	   "don't know how to set an interface address on this system");
  return -1;
#else
# if HAVE_DECL_GETADDRINFO
  int rc;
  char addr[INET_ADDRSTRLEN];
  struct addrinfo hints, *ai, *res;

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET;

  rc = getaddrinfo (address, NULL, &hints, &res);
  if (rc)
    {
      error (0, 0, "cannot resolve `%s': %s", address, gai_strerror (rc));
      return -1;
    }
  for (ai = res; ai; ai = ai->ai_next)
    if (ai->ai_family == AF_INET)
      break;

  if (ai == NULL)
    {
      error (0, 0, "`%s' refers to an unknown address type", address);
      freeaddrinfo (res);
      return -1;
    }

  rc = getnameinfo (ai->ai_addr, ai->ai_addrlen,
		    addr, sizeof (addr), NULL, 0,
		    NI_NUMERICHOST);
  freeaddrinfo (res);
  if (rc)
    {
      error (0, 0, "cannot resolve `%s': %s", address, gai_strerror (rc));
      return -1;
    }
# else /* !HAVE_DECL_GETADDRINFO */
  char *addr;
  struct hostent *host = gethostbyname (address);

  if (!host)
    {
      error (0, 0, "cannot resolve `%s': %s", address, hstrerror (h_errno));
      return -1;
    }
  if (host->h_addrtype != AF_INET)
    {
      error (0, 0, "`%s' refers to an unknown address type", address);
      return -1;
    }

  addr = inet_ntoa (*((struct in_addr *) host->h_addr));
# endif /* !HAVE_DECL_GETADDRINFO */

  {
    SIOCSIF (ADDR, addr)
    if (verbose)
      printf ("Set interface address of `%s' to %s.\n",
	      ifr->ifr_name, inet_ntoa (sin->sin_addr));
  }
  return 0;
#endif
}

int
set_netmask (int sfd, struct ifreq *ifr, char *netmask)
{
#ifndef SIOCSIFNETMASK
  error (0, 0, "don't know how to set an interface netmask on this system");
  return -1;
#else

  SIOCSIF (NETMASK, netmask)
  if (verbose)
    printf ("Set interface netmask of `%s' to %s.\n",
	    ifr->ifr_name, inet_ntoa (sin->sin_addr));
  return 0;
#endif
}

int
set_dstaddr (int sfd, struct ifreq *ifr, char *dstaddr)
{
#ifndef SIOCSIFDSTADDR
  error (0, 0,
         "don't know how to set an interface peer address on this system");
  return -1;
#else
  SIOCSIF (DSTADDR, dstaddr)
  if (verbose)
    printf ("Set interface peer address of `%s' to %s.\n",
	    ifr->ifr_name, inet_ntoa (sin->sin_addr));
  return 0;
#endif
}

int
set_brdaddr (int sfd, struct ifreq *ifr, char *brdaddr)
{
#ifndef SIOCSIFBRDADDR
  error (0, 0,
         "don't know how to set an interface broadcast address on this system");
  return -1;
#else
  SIOCSIF (BRDADDR, brdaddr)
  if (verbose)
    printf ("Set interface broadcast address of `%s' to %s.\n",
	    ifr->ifr_name, inet_ntoa (sin->sin_addr));
  return 0;
#endif
}

int
set_mtu (int sfd, struct ifreq *ifr, int mtu)
{
#ifndef SIOCSIFMTU
  error (0, 0,
         "don't know how to set the interface mtu on this system");
  return -1;
#else
  int err = 0;

  ifr->ifr_mtu = mtu;
  err = ioctl (sfd, SIOCSIFMTU, ifr);
  if (err < 0)
    {
      error (0, errno, "SIOCSIFMTU failed");
      return -1;
    }
  if (verbose)
    printf ("Set mtu value of `%s' to `%i'.\n", ifr->ifr_name, ifr->ifr_mtu);
  return 0;
#endif
}

int
set_metric (int sfd, struct ifreq *ifr, int metric)
{
#ifndef SIOCSIFMETRIC
  error (0, 0,
         "don't know how to set the interface metric on this system");
  return -1;
#else
  int err = 0;

  ifr->ifr_metric = metric;
  err = ioctl (sfd, SIOCSIFMETRIC, ifr);
  if (err < 0)
    {
      error (0, errno, "SIOCSIFMETRIC failed");
      return -1;
    }
  if (verbose)
    printf ("Set metric value of `%s' to `%i'.\n",
	    ifr->ifr_name, ifr->ifr_metric);
  return 0;
#endif
}

int
set_flags (int sfd, struct ifreq *ifr, int setflags, int clrflags)
{
#if !defined SIOCGIFFLAGS || !defined SIOCSIFFLAGS
  error (0, 0,
         "don't know how to set the interface flags on this system");
  return -1;
#else
  struct ifreq tifr = *ifr;

  if (ioctl (sfd, SIOCGIFFLAGS, &tifr) < 0)
    {
      error (0, errno, "SIOCGIFFLAGS failed");
      return -1;
    }
  /* Some systems, notably FreeBSD, use two short integers.  */
  ifr->ifr_flags = (tifr.ifr_flags | setflags) & ~clrflags & 0xffff;

# ifdef ifr_flagshigh
  ifr->ifr_flagshigh = (tifr.ifr_flagshigh | (setflags >> 16))
		       & ~(clrflags >> 16);
# endif /* ifr_flagshigh */

  if (ioctl (sfd, SIOCSIFFLAGS, ifr) < 0)
    {
      error (0, errno, "SIOCSIFFLAGS failed");
      return -1;
    }
  return 0;
#endif
}

int
configure_if (int sfd, struct ifconfig *ifp)
{
  int err = 0;
  struct ifreq ifr;

  memset (&ifr, 0, sizeof (ifr));
  strncpy (ifr.ifr_name, ifp->name, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ - 1] = '\0';

  if (ifp->valid & IF_VALID_ADDR)
    err = set_address (sfd, &ifr, ifp->address);
  if (!err && ifp->valid & IF_VALID_NETMASK)
    err = set_netmask (sfd, &ifr, ifp->netmask);
  if (!err && ifp->valid & IF_VALID_DSTADDR)
    err = set_dstaddr (sfd, &ifr, ifp->dstaddr);
  if (!err && ifp->valid & IF_VALID_BRDADDR)
    err = set_brdaddr (sfd, &ifr, ifp->brdaddr);
  if (!err && ifp->valid & IF_VALID_MTU)
    err = set_mtu (sfd, &ifr, ifp->mtu);
  if (!err && ifp->valid & IF_VALID_METRIC)
    err = set_metric (sfd, &ifr, ifp->metric);
  if (!err && ifp->valid & IF_VALID_SYSTEM)
    err = system_configure (sfd, &ifr, ifp->system);
  if (!err && (ifp->setflags || ifp->clrflags))
    err = set_flags (sfd, &ifr, ifp->setflags, ifp->clrflags);
  if (!err && ifp->valid & IF_VALID_FORMAT)
    print_interface (sfd, ifp->name, &ifr, ifp->format);
  return err;
}
