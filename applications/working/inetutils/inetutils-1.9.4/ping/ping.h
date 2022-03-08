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

#include "ping_common.h"

#define USE_IPV6 0

PING *ping_init (int type, int ident);
void ping_reset (PING * p);
void ping_set_type (PING * p, int type);
void ping_set_packetsize (PING * ping, size_t size);
int ping_set_dest (PING * ping, char *host);
int ping_set_pattern (PING * p, int len, unsigned char * pat);
void ping_set_event_handler (PING * ping, ping_efp fp, void *closure);
int ping_recv (PING * p);
int ping_xmit (PING * p);
