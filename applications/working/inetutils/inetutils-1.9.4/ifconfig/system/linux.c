/* linux.c -- Linux specific code for ifconfig
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

#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <xalloc.h>

#include <unistd.h>

#include <string.h>

#if STDC_HEADERS
# include <stdlib.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
#endif

#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_ether.h>

#include <read-file.h>
#include <unused-parameter.h>

#include "../ifconfig.h"


/* ARPHRD stuff.  */

static void
print_hwaddr_ether (format_data_t form _GL_UNUSED_PARAMETER,
		    unsigned char *data)
{
  *column += printf ("%02X:%02X:%02X:%02X:%02X:%02X",
		     data[0], data[1], data[2], data[3], data[4], data[5]);
  had_output = 1;
}

static void
print_hwaddr_arcnet (format_data_t form _GL_UNUSED_PARAMETER,
		     unsigned char *data)
{
  *column += printf ("%02X", data[0]);
  had_output = 1;
}

static void
print_hwaddr_dlci (format_data_t form _GL_UNUSED_PARAMETER,
		   unsigned char *data)
{
  *column += printf ("%i", *((short *) data));
  had_output = 1;
}

static void
print_hwaddr_ax25 (format_data_t form, unsigned char *data)
{
  int i = 0;
  while (i < 6 && (data[i] >> 1) != ' ')
    {
      put_char (form, (data[i] >> 1));
      i++;
    }
  i = (data[6] & 0x1E) >> 1;
  if (i)
    {
      *column += printf ("-%i", i);
      had_output = 1;
    }
#undef mangle
}

static void
print_hwaddr_irda (format_data_t form _GL_UNUSED_PARAMETER,
		   unsigned char *data)
{
  *column += printf ("%02X:%02X:%02X:%02X",
		     data[3], data[2], data[1], data[0]);
  had_output = 1;
}

static void
print_hwaddr_rose (format_data_t form _GL_UNUSED_PARAMETER,
		   unsigned char *data)
{
  *column += printf ("%02X%02X%02X%02X%02X",
		     data[0], data[1], data[2], data[3], data[4]);
  had_output = 1;
}

struct arphrd_symbol
{
  const char *name;
  const char *title;
  int value;
  void (*print_hwaddr) (format_data_t form, unsigned char *data);
} arphrd_symbols[] =
  {
    /* ARP protocol HARDWARE identifiers. */
#ifdef ARPHRD_NETROM		/* From KA9Q: NET/ROM pseudo.  */
    {"NETROM", "AMPR NET/ROM", ARPHRD_NETROM, print_hwaddr_ax25},
#endif
#ifdef ARPHRD_ETHER		/* Ethernet 10/100Mbps.  */
    {"ETHER", "Ethernet", ARPHRD_ETHER, print_hwaddr_ether},
#endif
#ifdef ARPHRD_EETHER		/* Experimental Ethernet.  */
    {"EETHER", "Experimental Etherner", ARPHRD_EETHER, NULL},
#endif
#ifdef ARPHRD_AX25		/* AX.25 Level 2.  */
    {"AX25", "AMPR AX.25", ARPHRD_AX25, print_hwaddr_ax25},
#endif
#ifdef ARPHRD_PRONET		/* PROnet token ring.  */
    {"PRONET", "PROnet token ring", ARPHRD_PRONET, NULL},
#endif
#ifdef ARPHRD_CHAOS		/* Chaosnet.  */
    {"CHAOS", "Chaosnet", ARPHRD_CHAOS, NULL},
#endif
#ifdef ARPHRD_IEEE802		/* IEEE 802.2 Ethernet/TR/TB.  */
    {"IEEE802", "16/4 Mbps Token Ring", ARPHRD_IEEE802, print_hwaddr_ether},
#endif
#ifdef ARPHRD_ARCNET		/* ARCnet.  */
    {"ARCNET", "ARCnet", ARPHRD_ARCNET, print_hwaddr_arcnet},
#endif
#ifdef ARPHRD_APPLETLK		/* APPLEtalk.  */
    {"APPLETLK", "Appletalk", ARPHRD_APPLETLK, NULL},
#endif
#ifdef ARPHRD_DLCI		/* Frame Relay DLCI.  */
    {"DLCI", "Frame Relay DLCI", ARPHRD_DLCI, print_hwaddr_dlci},
#endif
#ifdef ARPHRD_ATM		/* ATM.  */
    {"ATM", "ATM", ARPHRD_ATM, NULL},
#endif
#ifdef ARPHRD_METRICOM		/* Metricom STRIP (new IANA id).  */
    {"METRICOM", "Metricom STRIP", ARPHRD_METRICOM, NULL},
#endif
    /* Dummy types for non ARP hardware.  */
#ifdef ARPHRD_SLIP
    {"SLIP", "Serial Line IP", ARPHRD_SLIP, NULL},
#endif
#ifdef ARPHRD_CSLIP
    {"CSLIP", "VJ Serial Line IP", ARPHRD_CSLIP, NULL},
#endif
#ifdef ARPHRD_SLIP6
    {"SLIP6", "6-bit Serial Line IP", ARPHRD_SLIP6, NULL},
#endif
#ifdef ARPHRD_CSLIP6
    {"CSLIP6", "VJ 6-bit Serial Line IP", ARPHRD_CSLIP6, NULL},
#endif
#ifdef ARPHRD_RSRVD		/* Notional KISS type.  */
    {"SLIP", "Notional KISS type", ARPHRD_SLIP, NULL},
#endif
#ifdef ARPHRD_ADAPT
    {"ADAPT", "Adaptive Serial Line IP", ARPHRD_ADAPT, NULL},
#endif
#ifdef ARPHRD_ROSE
    {"ROSE", "AMPR ROSE", ARPHRD_ROSE, print_hwaddr_rose},
#endif
#ifdef ARPHRD_X25		/* CCITT X.25.  */
    {"X25", "CCITT X.25", ARPHRD_X25, NULL},
#endif
#ifdef ARPHRD_HWX25		/* Boards with X.25 in firmware.  */
    {"HWX25", "CCITT X.25 in firmware", ARPHRD_HWX25, NULL},
#endif
#ifdef ARPHRD_PPP
    {"PPP", "Point-to-Point Protocol", ARPHRD_PPP, NULL},
#endif
#ifdef ARPHRD_HDLC		/* (Cisco) HDLC.  */
    {"HDLC", "(Cisco)-HDLC", ARPHRD_HDLC, NULL},
#endif
#ifdef ARPHRD_LAPB		/* LAPB.  */
    {"LAPB", "LAPB", ARPHRD_LAPB, NULL},
#endif
#ifdef ARPHRD_DDCMP		/* Digital's DDCMP.  */
    {"DDCMP", "DDCMP", ARPHRD_DDCMP, NULL},
#endif
#ifdef ARPHRD_TUNNEL		/* IPIP tunnel.  */
    {"TUNNEL", "IPIP Tunnel", ARPHRD_TUNNEL, NULL},
#endif
#ifdef ARPHRD_TUNNEL6		/* IPIP6 tunnel.  */
    {"TUNNEL", "IPIP6 Tunnel", ARPHRD_TUNNEL6, NULL},
#endif
#ifdef ARPHRD_FRAD		/* Frame Relay Access Device.  */
    {"FRAD", "Frame Relay Access Device", ARPHRD_FRAD, NULL},
#endif
#ifdef ARPHRD_SKIP		/* SKIP vif.  */
    {"SKIP", "SKIP vif", ARPHRD_SKIP, NULL},
#endif
#ifdef ARPHRD_LOOPBACK		/* Loopback device.  */
    {"LOOPBACK", "Local Loopback", ARPHRD_LOOPBACK, NULL},
#endif
#ifdef ARPHRD_LOCALTALK		/* Localtalk device.  */
    {"LOCALTALK", "Localtalk", ARPHRD_LOCALTALK, NULL},
#endif
#ifdef ARPHRD_FDDI		/* Fiber Distributed Data Interface. */
    {"FDDI", "Fiber Distributed Data Interface", ARPHRD_FDDI, NULL},
#endif
#ifdef ARPHRD_BIF		/* AP1000 BIF.  */
    {"BIF", "AP1000 BIF", ARPHRD_BIF, NULL},
#endif
#ifdef ARPHRD_SIT		/* sit0 device - IPv6-in-IPv4.  */
    {"SIT", "IPv6-in-IPv4", ARPHRD_SIT, NULL},
#endif
#ifdef ARPHRD_IPDDP		/* IP-in-DDP tunnel.  */
    {"IPDDP", "IP-in-DDP", ARPHRD_IPDDP, NULL},
#endif
#ifdef ARPHRD_IPGRE		/* GRE over IP.  */
    {"IPGRE", "GRE over IP", ARPHRD_IPGRE, NULL},
#endif
#ifdef ARPHRD_PIMREG		/* PIMSM register interface.  */
    {"PIMREG", "PIMSM register", ARPHRD_PIMREG, NULL},
#endif
#ifdef ARPHRD_HIPPI		/* High Performance Parallel I'face. */
    {"HIPPI", "HIPPI", ARPHRD_HIPPI, print_hwaddr_ether},
#endif
#ifdef ARPHRD_ASH		/* (Nexus Electronics) Ash.  */
    {"ASH", "Ash", ARPHRD_ASH, NULL},
#endif
#ifdef ARPHRD_ECONET		/* Acorn Econet.  */
    {"ECONET", "Econet", ARPHRD_ECONET, NULL},
#endif
#ifdef ARPHRD_IRDA		/* Linux-IrDA.  */
    {"IRDA", "IrLap", ARPHRD_IRDA, print_hwaddr_irda},
#endif
#ifdef ARPHRD_FCPP		/* Point to point fibrechanel.  */
    {"FCPP", "FCPP", ARPHRD_FCPP, NULL},
#endif
#ifdef ARPHRD_FCAL		/* Fibrechanel arbitrated loop.  */
    {"FCAL", "FCAL", ARPHRD_FCAL, NULL},
#endif
#ifdef ARPHRD_FCPL		/* Fibrechanel public loop.  */
    {"FCPL", "FCPL", ARPHRD_FCPL, NULL},
#endif
#ifdef ARPHRD_FCPFABRIC		/* Fibrechanel fabric.  */
    {"FCFABRIC", "FCFABRIC", ARPHRD_FCPFABRIC, NULL},
#endif
#ifdef ARPHRD_IEEE802_TR	/* Magic type ident for TR.  */
    {"IEEE802_TR", "16/4 Mbps Token Ring (New)", ARPHRD_IEEE802_TR,
     print_hwaddr_ether},
#endif
#ifdef ARPHRD_VOID
    {"VOID", "Void (nothing is known)", ARPHRD_VOID, NULL},
#endif
  };

struct arphrd_symbol *
arphrd_findvalue (int value)
{
  struct arphrd_symbol *arp = arphrd_symbols;
  while (arp->name != NULL)
    {
      if (arp->value == value)
	break;
      arp++;
    }
  if (arp->name)
    return arp;
  else
    return NULL;
}


struct pnd_stats
{
  struct pnd_stats *next;
  char *name;
  unsigned long long rx_packets;   /* total packets received */
  unsigned long long tx_packets;   /* total packets transmitted */
  unsigned long long rx_bytes;     /* total bytes received */
  unsigned long long tx_bytes;     /* total bytes transmitted */
  unsigned long rx_errors;         /* packets receive errors */
  unsigned long tx_errors;         /* packet transmit errors */
  unsigned long rx_dropped;        /* input packets dropped */
  unsigned long tx_dropped;        /* transmit packets dropped */
  unsigned long rx_multicast;      /* multicast packets received */
  unsigned long rx_compressed;     /* compressed packets received */
  unsigned long tx_compressed;     /* compressed packets transmitted */
  unsigned long collisions;

  /* detailed rx_errors: */
  unsigned long rx_length_errors;
   unsigned long rx_over_errors;    /* receiver ring buffer overflow */
  unsigned long rx_crc_errors;     /* received packets with crc error */
  unsigned long rx_frame_errors;   /* received frame alignment errors */
  unsigned long rx_fifo_errors;    /* recveiver fifo overruns */
  unsigned long rx_missed_errors;  /* receiver missed packets */
  /* detailed tx_errors */
  unsigned long tx_aborted_errors;
  unsigned long tx_carrier_errors;
  unsigned long tx_fifo_errors;
  unsigned long tx_heartbeat_errors;
  unsigned long tx_window_errors;
};

struct pnd_stats *stats_head, *stats_tail;
static int pnd_stats_init = 0;

static int
procnet_parse_fields_v1 (char *buf, struct pnd_stats *stats)
{
  int n = sscanf (buf,
		  "%llu %lu %lu %lu %lu %llu %lu %lu %lu %lu %lu",
		  &stats->rx_packets,
		  &stats->rx_errors,
		  &stats->rx_dropped,
		  &stats->rx_fifo_errors,
		  &stats->rx_frame_errors,
		  &stats->tx_packets,
		  &stats->tx_errors,
		  &stats->tx_dropped,
		  &stats->tx_fifo_errors,
		  &stats->collisions,
		  &stats->tx_carrier_errors);
  stats->rx_bytes = 0;
  stats->tx_bytes = 0;
  stats->rx_multicast = 0;
  return n == 11;
}

static int
procnet_parse_fields_v2 (char *buf, struct pnd_stats *stats)
{
  int n = sscanf (buf,
		  "%llu %llu %lu %lu %lu %lu %llu %llu %lu %lu %lu %lu %lu",
		  &stats->rx_bytes,
		  &stats->rx_packets,
		  &stats->rx_errors,
		  &stats->rx_dropped,
		  &stats->rx_fifo_errors,
		  &stats->rx_frame_errors,
		  &stats->tx_bytes,
		  &stats->tx_packets,
		  &stats->tx_errors,
		  &stats->tx_dropped,
		  &stats->tx_fifo_errors,
		  &stats->collisions,
		  &stats->tx_carrier_errors);
  stats->rx_multicast = 0;
  return n == 13;
}

static int
procnet_parse_fields_v3 (char *buf, struct pnd_stats *stats)
{
  int n = sscanf (buf,
		  "%llu %llu %lu %lu %lu %lu %lu %lu %llu %llu %lu %lu %lu %lu %lu %lu",
		  &stats->rx_bytes,
		  &stats->rx_packets,
		  &stats->rx_errors,
		  &stats->rx_dropped,
		  &stats->rx_fifo_errors,
		  &stats->rx_frame_errors,
		  &stats->rx_compressed,
		  &stats->rx_multicast,
		  &stats->tx_bytes,
		  &stats->tx_packets,
		  &stats->tx_errors,
		  &stats->tx_dropped,
		  &stats->tx_fifo_errors,
		  &stats->collisions,
		  &stats->tx_carrier_errors,
		  &stats->tx_compressed);
  return n == 16;
}

static int (*pnd_parser[]) (char *, struct pnd_stats *) =
{
  procnet_parse_fields_v1,
  procnet_parse_fields_v2,
  procnet_parse_fields_v3
};

static int
pnd_version (char *buf)
{
  if (strstr (buf, "compressed"))
    return 2;
  if (strstr(buf, "bytes"))
    return 1;
  return 0;
}

static void
pnd_read ()
{
  FILE *fp;
  char *buf = NULL;
  size_t bufsize = 0;
  int v;

  fp = fopen (PATH_PROCNET_DEV, "r");
  if (!fp)
    {
      error (0, errno, "cannot open %s", PATH_PROCNET_DEV);
      return;
    }

  /* Skip header lines */
  if (getline (&buf, &bufsize, fp) < 0
      || getline (&buf, &bufsize, fp) < 0)
    {
      error (0, errno, "malformed %s", PATH_PROCNET_DEV);
      fclose (fp);
      free (buf);
      return;
    }

  v = pnd_version (buf);

  while (getline (&buf, &bufsize, fp) > 0)
    {
      struct pnd_stats *stats = xzalloc (sizeof (*stats));
      char *p = buf, *q;
      size_t len;

      while (*p
	     && isascii (*(unsigned char*) p) && isspace (*(unsigned char*) p))
	p++;
      q = strchr (p, ':');
      if (!q)
	{
	  error (0, errno, "malformed %s", PATH_PROCNET_DEV);
	  free (stats);
	  break;
	}
      len = q - p;
      stats->name = xmalloc (len + 1);
      memcpy (stats->name, p, len);
      stats->name[len] = 0;
      if (pnd_parser[v] (q + 1, stats) == 0)
	{
	  error (0, errno, "malformed %s", PATH_PROCNET_DEV);
	  free (stats);
	  break;
	}
      stats->next = NULL;
      if (stats_tail)
	stats_tail->next = stats;
      else
	stats_head = stats;
      stats_tail = stats;
    }

  fclose (fp);
  free (buf);
}

struct pnd_stats *
pnd_stats_locate (const char *name)
{
  struct pnd_stats *stats;
  if (!pnd_stats_init)
    {
      pnd_read ();
      pnd_stats_init = 1;
    }
  for (stats = stats_head; stats; stats = stats->next)
    if (strcmp (stats->name, name) == 0)
      break;
  return stats;
}



/* Output format stuff.  */

const char *system_default_format = "net-tools";

void
system_fh_ifstat_query (format_data_t form, int argc, char *argv[])
{
  select_arg (form, argc, argv,
	      pnd_stats_locate (form->ifr->ifr_name) ? 0 : 1);
}

#define _IU_DECLARE(fld)						     \
void                                                                         \
_IU_CAT2 (system_fh_,fld) (format_data_t form, int argc, char *argv[])       \
{                                                                            \
  struct pnd_stats *stats = pnd_stats_locate (form->ifr->ifr_name);          \
  if (!stats)                                                                \
    put_string (form, "(" #fld " unknown)");                                 \
  else								             \
    put_ulong (form, argc, argv, stats->fld);                                \
}

_IU_DECLARE (rx_packets)
_IU_DECLARE (tx_packets)
_IU_DECLARE (rx_bytes)
_IU_DECLARE (tx_bytes)
_IU_DECLARE (rx_errors)
_IU_DECLARE (tx_errors)
_IU_DECLARE (rx_dropped)
_IU_DECLARE (tx_dropped)
_IU_DECLARE (rx_multicast)
_IU_DECLARE (rx_compressed)
_IU_DECLARE (tx_compressed)
_IU_DECLARE (collisions)
_IU_DECLARE (rx_length_errors)
_IU_DECLARE (rx_over_errors)
_IU_DECLARE (rx_crc_errors)
_IU_DECLARE (rx_frame_errors)
_IU_DECLARE (rx_fifo_errors)
_IU_DECLARE (rx_missed_errors)
_IU_DECLARE (tx_aborted_errors)
_IU_DECLARE (tx_carrier_errors)
_IU_DECLARE (tx_fifo_errors)
_IU_DECLARE (tx_heartbeat_errors)
_IU_DECLARE (tx_window_errors)

void
system_fh_hwaddr_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFHWADDR
  struct arphrd_symbol *arp;

  if (ioctl (form->sfd, SIOCGIFHWADDR, form->ifr) < 0)
    select_arg (form, argc, argv, 1);

  arp = arphrd_findvalue (form->ifr->ifr_hwaddr.sa_family);
  select_arg (form, argc, argv, (arp && arp->print_hwaddr) ? 0 : 1);
#else
  select_arg (form, argc, argv, 1);
#endif
}

void
system_fh_hwaddr (format_data_t form, int argc _GL_UNUSED_PARAMETER,
		  char *argv[] _GL_UNUSED_PARAMETER)
{
#ifdef SIOCGIFHWADDR
  if (ioctl (form->sfd, SIOCGIFHWADDR, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFHWADDR failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    {
      struct arphrd_symbol *arp;

      arp = arphrd_findvalue (form->ifr->ifr_hwaddr.sa_family);
      if (arp && arp->print_hwaddr)
	arp->print_hwaddr (form,
			   (unsigned char *) form->ifr->ifr_hwaddr.sa_data);
      else
	put_string (form, "(hwaddr unknown)");
    }
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}

void
system_fh_hwtype_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFHWADDR
  if (ioctl (form->sfd, SIOCGIFHWADDR, form->ifr) >= 0)
    select_arg (form, argc, argv, 0);
  else
#endif
    select_arg (form, argc, argv, 1);
}

void
system_fh_hwtype (format_data_t form, int argc _GL_UNUSED_PARAMETER,
		  char *argv[] _GL_UNUSED_PARAMETER)
{
#ifdef SIOCGIFHWADDR
  if (ioctl (form->sfd, SIOCGIFHWADDR, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFHWADDR failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    {
      struct arphrd_symbol *arp;

      arp = arphrd_findvalue (form->ifr->ifr_hwaddr.sa_family);
      if (arp)
	put_string (form, arp->title);
      else
	put_string (form, "(hwtype unknown)");
    }
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}

/* Accept every value of metric as printable.  The net-tools'
 * implementation of ifconfig displays metric 0 as `1', so we
 * aim at the same thing, even though all other unices disagree.
 */
void
system_fh_metric_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFMETRIC
  if (ioctl (form->sfd, SIOCGIFMETRIC, form->ifr) >= 0)
    select_arg (form, argc, argv, 0);
  else
#endif
    select_arg (form, argc, argv, 1);
}

void
system_fh_metric (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFMETRIC
  if (ioctl (form->sfd, SIOCGIFMETRIC, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFMETRIC failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    put_int (form, argc, argv,
	     form->ifr->ifr_metric ? form->ifr->ifr_metric : 1);
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}

void
system_fh_txqlen_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFTXQLEN
  if (ioctl (form->sfd, SIOCGIFTXQLEN, form->ifr) >= 0)
    select_arg (form, argc, argv, (form->ifr->ifr_qlen >= 0) ? 0 : 1);
  else
#endif
    select_arg (form, argc, argv, 1);
}

void
system_fh_txqlen (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFTXQLEN
  if (ioctl (form->sfd, SIOCGIFTXQLEN, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFTXQLEN failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    put_int (form, argc, argv, form->ifr->ifr_qlen);
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}


/* Argument parsing stuff.  */

const char *system_help = "\
 NAME [ADDR] [broadcast BRDADDR]\
 [pointopoint|dstaddr DSTADDR] [netmask MASK]\
 [metric N] [mtu N] [txqueuelen N] [up|down] [FLAGS]";

void
system_parse_opt_set_txqlen (struct ifconfig *ifp, char *arg)
{
  char *end;

  if (!ifp)
    error (EXIT_FAILURE, 0,
	   "no interface specified for txqlen `%s'", arg);

  if (!(ifp->valid & IF_VALID_SYSTEM))
    {
      ifp->system = malloc (sizeof (struct system_ifconfig));
      if (!ifp->system)
	error (EXIT_FAILURE, errno,
	       "can't get memory for system interface configuration");
      ifp->system->valid = 0;
      ifp->valid |= IF_VALID_SYSTEM;
    }
  if (ifp->system->valid & IF_VALID_TXQLEN)
    error (EXIT_FAILURE, 0,
	   "only one txqlen allowed for interface `%s'",
	   ifp->name);
  ifp->system->txqlen = strtol (arg, &end, 0);
  if (*arg == '\0' || *end != '\0')
    error (EXIT_FAILURE, 0,
	   "txqlen value `%s' for interface `%s' is not a number",
	   arg, ifp->name);
  ifp->system->valid |= IF_VALID_TXQLEN;
}

static struct argp_option linux_argp_options[] = {
  { "txqlen", 'T', "N", 0,
    "set transmit queue length to N", 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
linux_argp_parser (int key, char *arg, struct argp_state *state)
{
  struct ifconfig *ifp = *(struct ifconfig **)state->input;
  switch (key)
    {
    case 'T':			/* txqlen */
      system_parse_opt_set_txqlen (ifp, arg);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp linux_argp =
  { linux_argp_options, linux_argp_parser, NULL, NULL, NULL, NULL, NULL };

struct argp_child system_argp_child = {
  &linux_argp,
  0,
  "Linux-specific options",
  0
};


int
system_parse_opt (struct ifconfig **ifpp, char option, char *arg)
{
  struct ifconfig *ifp = *ifpp;

  switch (option)
    {
    case 'T':			/* txqlen */
      system_parse_opt_set_txqlen (ifp, arg);
      break;

    default:
      return 0;
    }
  return 1;
}

int
system_parse_opt_rest (struct ifconfig **ifp, int argc, char *argv[])
{
  int i = 0;
  enum
  {
    EXPECT_AF,
    EXPECT_NOTHING,
    EXPECT_BROADCAST,
    EXPECT_DSTADDR,
    EXPECT_NETMASK,
    EXPECT_MTU,
    EXPECT_METRIC,
    EXPECT_TXQLEN,
  } expect = EXPECT_AF;
  int mask, rev;

  *ifp = parse_opt_new_ifs (argv[0]);

  while (++i < argc)
    {
      switch (expect)
	{
	case EXPECT_BROADCAST:
	  parse_opt_set_brdaddr (*ifp, argv[i]);
	  break;

	case EXPECT_DSTADDR:
	  parse_opt_set_point_to_point (*ifp, argv[i]);
	  break;

	case EXPECT_NETMASK:
	  parse_opt_set_netmask (*ifp, argv[i]);
	  break;

	case EXPECT_MTU:
	  parse_opt_set_mtu (*ifp, argv[i]);
	  break;

	case EXPECT_METRIC:
	  parse_opt_set_metric (*ifp, argv[i]);
	  break;

	case EXPECT_TXQLEN:
	  system_parse_opt_set_txqlen (*ifp, argv[i]);
	  break;

	case EXPECT_AF:
	  expect = EXPECT_NOTHING;
	  if (!strcmp (argv[i], "inet"))
	    continue;
	  else if (!strcmp (argv[i], "inet6"))
	    {
	      error (0, 0, "%s is not a supported address family", argv[i]);
	      return 0;
	    }
	  break;

	case EXPECT_NOTHING:
	  break;
	}

      if (expect != EXPECT_NOTHING)
	expect = EXPECT_NOTHING;
      else if (!strcmp (argv[i], "broadcast"))
	expect = EXPECT_BROADCAST;
      else if (!strcmp (argv[i], "dstaddr")
	       || !strcmp (argv[i], "pointopoint"))
	expect = EXPECT_DSTADDR;
      else if (!strcmp (argv[i], "netmask"))
	expect = EXPECT_NETMASK;
      else if (!strcmp (argv[i], "metric"))
	expect = EXPECT_METRIC;
      else if (!strcmp (argv[i], "mtu"))
	expect = EXPECT_MTU;
      else if (!strcmp (argv[i], "txqueuelen"))
	expect = EXPECT_TXQLEN;
      else if (!strcmp (argv[i], "up"))
	parse_opt_set_flag (*ifp, IFF_UP | IFF_RUNNING, 0);
      else if (!strcmp (argv[i], "down"))
	parse_opt_set_flag (*ifp, IFF_UP, 1);
      else if (((mask = if_nameztoflag (argv[i], &rev))
		& ~IU_IFF_CANTCHANGE) != 0)
	parse_opt_set_flag (*ifp, mask, rev);
      else
	parse_opt_set_address (*ifp, argv[i]);
    }

  switch (expect)
    {
    case EXPECT_BROADCAST:
      error (0, 0, "option `broadcast' requires an argument");
      break;

    case EXPECT_DSTADDR:
      error (0, 0, "option `pointopoint' (`dstaddr') requires an argument");
      break;

    case EXPECT_NETMASK:
      error (0, 0, "option `netmask' requires an argument");
      break;

    case EXPECT_METRIC:
      error (0, 0, "option `metric' requires an argument");
      break;

    case EXPECT_MTU:
      error (0, 0, "option `mtu' requires an argument");
      break;

    case EXPECT_TXQLEN:
      error (0, 0, "option `txqueuelen' requires an argument");
      break;

    case EXPECT_AF:
    case EXPECT_NOTHING:
      return 1;
    }
  return 0;
}


int
system_configure (int sfd, struct ifreq *ifr, struct system_ifconfig *ifs)
{
  if (ifs->valid & IF_VALID_TXQLEN)
    {
#ifndef SIOCSIFTXQLEN
      error (0, 0, "don't know how to set the txqlen on this system");
      return -1;
#else
      int err = 0;

      ifr->ifr_qlen = ifs->txqlen;
      err = ioctl (sfd, SIOCSIFTXQLEN, ifr);
      if (err < 0)
	error (0, errno, "SIOCSIFTXQLEN failed");
      if (verbose)
	printf ("Set txqlen value of `%s' to `%i'.\n",
		ifr->ifr_name, ifr->ifr_qlen);
#endif
    }
  return 0;
}

static struct if_nameindex *
linux_if_nameindex (void)
{
  char *content, *it;
  size_t length, index;
  struct if_nameindex *idx = NULL;
  int fd;

  fd = socket (AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return NULL;

  content = read_file (PATH_PROCNET_DEV, &length);
  if (content == NULL)
    return NULL;

  /* Count how many interfaces we have.  */
  {
    size_t n = 0;
    it = content;
    do
      {
        it = memchr (it + 1, ':', length - (it - content));
        n++;
      }
    while (it);

    idx = malloc (n * sizeof (*idx));
    if (idx == NULL)
      {
        int saved_errno = errno;
        close (fd);
        free (content);
        errno = saved_errno;
        return NULL;
      }
  }

  for (it = memchr (content, ':', length), index = 0; it;
       it = memchr (it, ':', length - (it - content)), index++)
    {
      char *start = it - 1;
      *it = '\0';

      while (*start != ' ' && *start != '\n')
        start--;

      idx[index].if_name = strdup (start + 1);
      idx[index].if_index = index + 1;

# if defined SIOCGIFINDEX
      {
        struct ifreq cur;
        strcpy (cur.ifr_name, idx[index].if_name);
        cur.ifr_index = -1;
        if (ioctl (fd, SIOCGIFINDEX, &cur) >= 0)
          idx[index].if_index = cur.ifr_index;
      }
# endif

      if (idx[index].if_name == NULL)
        {
          int saved_errno = errno;
          close (fd);
          free (content);
          errno = saved_errno;
          return NULL;
        }
    }

  idx[index].if_index = 0;
  idx[index].if_name = NULL;

  free (content);
  return idx;
}



/* System hooks. */

struct if_nameindex* (*system_if_nameindex) (void) = linux_if_nameindex;
