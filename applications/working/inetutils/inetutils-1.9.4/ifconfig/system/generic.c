/* generic.c -- generic system code for ifconfig
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

#include "../ifconfig.h"


/* Output format stuff.  */

const char *system_default_format;


/* Argument parsing stuff.  */

const char *system_help;

struct argp_child system_argp_child;

int
system_parse_opt (struct ifconfig **ifp, char option, char *optarg)
{
  return 0;
}

int
system_parse_opt_rest (struct ifconfig **ifp, int argc, char *argv[])
{
  return 0;
}

int
system_configure (int sfd, struct ifreq *ifr, struct system_ifconfig *ifs)
{
  return 0;
}

struct if_nameindex* (*system_if_nameindex) (void) = if_nameindex;
