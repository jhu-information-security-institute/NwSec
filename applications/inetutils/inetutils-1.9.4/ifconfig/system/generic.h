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

#ifndef IFCONFIG_SYSTEM_GENERIC_H
# define IFCONFIG_SYSTEM_GENERIC_H


/* Option support.  */

# undef SYSTEM_SHORT_OPTIONS
# undef SYSTEM_LONG_OPTIONS

struct system_ifconfig
{
  int valid;
};


/* Output format support.  */

/* Set this to a comma seperated, comma terminated list of struct
   format_handler entries.  They are inserted at the beginning of the
   default list, so they override generic implementations if they have
   the same name.  For example:
   #define SYSTEM_FORMAT_HANDLER { "foobar", system_fh_nothing }, \
   { "newline", system_fh_newline },
   Define some architecture symbol like "foobar", so it can be tested
   for in generic format strings with ${exists?}{foobar?}.  */
# undef SYSTEM_FORMAT_HANDLER

/* Prototype system_fh_* functions here.
   void system_fh_newline (format_data_t, int, char **);
*/

#endif
