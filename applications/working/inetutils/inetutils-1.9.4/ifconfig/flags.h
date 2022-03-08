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

#ifndef IFCONFIG_FLAGS_H
# define IFCONFIG_FLAGS_H

#include <sys/types.h>

/* Using these avoid strings with if_flagtoname, the caller can set a
   preference on returned flag names.  If one of the names in the list
   is found for the flag, the search continues to attempt a better
   match.  */
/* FreeBSD */
# define EXPECT_LINK2 ":LINK2/ALTPHYS:ALTPHYS:"
# define EXPECT_ALTPHYS ":LINK2/ALTPHYS:LINK2:"

/* OSF 4.0g */
# define EXPECT_D2 ":D2/SNAP:SNAP:"
# define EXPECT_SNAP ":D2/SNAP:D2:"

/* Suppress flags that are not changeable by user.  */
#ifndef IFF_CANTCHANGE
# define IFF_CANTCHANGE 0
#endif /* IFF_CANTCHANGE */

/* Manually exclude flags that experience tell us be static.  */
#define IU_IFF_CANTCHANGE \
	(IFF_CANTCHANGE | IFF_LOOPBACK | IFF_RUNNING)

/* Return the name corresponding to the interface flag FLAG.
   If FLAG is unknown, return NULL.
   AVOID contains a ':' surrounded and seperated list of flag names
   that should be avoided if alternative names with the same flag value
   exists.  The first unavoided match is returned, or the first avoided
   match if no better is available.  */
const char *if_flagtoname (int flag, const char *avoid);

/* Return the flag mask corresponding to flag name NAME.  If no flag
   with this name is found, return 0.  */
int if_nametoflag (const char *name, size_t len, int *prev);
int if_nameztoflag (const char *name, int *prev);

/* Print the flags in FLAGS, using AVOID as in if_flagtoname, and
   SEPARATOR between individual flags.  Returns the number of
   characters printed.  */
int print_if_flags (int flags, const char *avoid, char seperator);

char *if_list_flags (const char *prefix);

/* Size of the buffer for the if_format_flags call */
# define IF_FORMAT_FLAGS_BUFSIZE 15
void if_format_flags (int flags, char *buf, size_t size);

#endif
