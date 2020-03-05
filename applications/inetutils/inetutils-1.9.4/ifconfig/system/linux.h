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

/* Written by Marcus Brinkmann.  */

#ifndef IFCONFIG_SYSTEM_LINUX_H
# define IFCONFIG_SYSTEM_LINUX_H

# include "../printif.h"
# include "../options.h"


/* Option support.  */

struct system_ifconfig
{
  int valid;
# define IF_VALID_TXQLEN 0x1
  int txqlen;
};



/* Output format support.  */

# define _IU_CAT2(a,b) a ## b
# define _IU_DCL(name,fld)				\
  {#name, _IU_CAT2(system_fh_,fld) }
# define _IU_EXTRN(fld)				\
  extern void _IU_CAT2(system_fh_,fld) (format_data_t, int, char *[]);	\

# define SYSTEM_FORMAT_HANDLER \
  {"linux", fh_nothing}, \
  {"hwaddr?", system_fh_hwaddr_query}, \
  {"hwaddr", system_fh_hwaddr}, \
  {"hwtype?", system_fh_hwtype_query}, \
  {"hwtype", system_fh_hwtype}, \
  {"metric?", system_fh_metric_query}, \
  {"metric", system_fh_metric}, \
  {"txqlen?", system_fh_txqlen_query}, \
  {"txqlen", system_fh_txqlen}, \
  {"ifstat?", system_fh_ifstat_query}, \
  _IU_DCL(rxpackets,rx_packets),\
  _IU_DCL(txpackets,tx_packets),\
  _IU_DCL(rxbytes,rx_bytes),\
  _IU_DCL(txbytes,tx_bytes),\
  _IU_DCL(rxerrors,rx_errors),\
  _IU_DCL(txerrors,tx_errors),\
  _IU_DCL(rxdropped,rx_dropped),\
  _IU_DCL(txdropped,tx_dropped),\
  _IU_DCL(rxmulticast,rx_multicast),\
  _IU_DCL(rxcompressed,rx_compressed),\
  _IU_DCL(txcompressed,tx_compressed),\
  _IU_DCL(collisions,collisions),\
  _IU_DCL(rxlenerr,rx_length_errors),\
  _IU_DCL(rxovererr,rx_over_errors),\
  _IU_DCL(rxcrcerr,rx_crc_errors),\
  _IU_DCL(rxframeerr,rx_frame_errors),\
  _IU_DCL(rxfifoerr,rx_fifo_errors),\
  _IU_DCL(rxmisserr,rx_missed_errors),\
  _IU_DCL(txabrterr,tx_aborted_errors),\
  _IU_DCL(txcarrier,tx_carrier_errors),\
  _IU_DCL(txfifoerr,tx_fifo_errors),\
  _IU_DCL(txhberr,tx_heartbeat_errors),\
  _IU_DCL(txwinerr,tx_window_errors),

void system_fh_hwaddr_query (format_data_t form, int argc, char *argv[]);
void system_fh_hwaddr (format_data_t form, int argc, char *argv[]);
void system_fh_hwtype_query (format_data_t form, int argc, char *argv[]);
void system_fh_hwtype (format_data_t form, int argc, char *argv[]);
void system_fh_metric_query (format_data_t form, int argc, char *argv[]);
void system_fh_metric (format_data_t form, int argc, char *argv[]);
void system_fh_txqlen_query (format_data_t form, int argc, char *argv[]);
void system_fh_txqlen (format_data_t form, int argc, char *argv[]);

void system_fh_ifstat_query (format_data_t form, int argc, char *argv[]);

_IU_EXTRN (rx_packets)
_IU_EXTRN (tx_packets)
_IU_EXTRN (rx_bytes)
_IU_EXTRN (tx_bytes)
_IU_EXTRN (rx_errors)
_IU_EXTRN (tx_errors)
_IU_EXTRN (rx_dropped)
_IU_EXTRN (tx_dropped)
_IU_EXTRN (rx_multicast)
_IU_EXTRN (tx_multicast)
_IU_EXTRN (rx_compressed)
_IU_EXTRN (tx_compressed)
_IU_EXTRN (collisions)
_IU_EXTRN (rx_length_errors)
_IU_EXTRN (rx_over_errors)
_IU_EXTRN (rx_crc_errors)
_IU_EXTRN (rx_frame_errors)
_IU_EXTRN (rx_fifo_errors)
_IU_EXTRN (rx_missed_errors)
_IU_EXTRN (tx_aborted_errors)
_IU_EXTRN (tx_carrier_errors)
_IU_EXTRN (tx_fifo_errors)
_IU_EXTRN (tx_heartbeat_errors)
_IU_EXTRN (tx_window_errors)

#endif
