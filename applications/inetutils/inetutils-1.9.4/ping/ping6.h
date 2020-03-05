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

#include "ping_common.h"

#define PING_MAX_DATALEN (65535 - sizeof (struct icmp6_hdr))

#define USE_IPV6 1

static PING *ping_init (int type, int ident);
static int ping_set_dest (PING * ping, char *host);
static int ping_recv (PING * p);
static int ping_xmit (PING * p);

static int ping_run (PING * ping, int (*finish) ());
static int ping_finish (void);
