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

#ifndef IFCONFIG_SYSTEM_H
# define IFCONFIG_SYSTEM_H


/* Option parsing.  */

extern struct argp_child system_argp_child;

/* Define this if ifconfig supports parsing the remaining non-option
   arguments on the command line (see system_parse_opt_rest) to a string
   usable in the help info.  Like "  <addr> [ netmask <mask> ]" */
extern const char *system_help;

/* Hooked into a struct ifconfig (setting the flag IF_VALID_SYSTEM),
   to store system specific configurations from the command line
   arguments.  */
/* struct system_ifconfig; */

/* Parse option OPTION, with argument OPTARG (may be NULL), and store
   it in IFP.  You may create a new struct ifconfig if appropriate,
   and store its pointer in IFP to make it the current one.
   Return 0 if option was not recognized, otherwise 1.  */
extern int system_parse_opt (struct ifconfig **ifp, char option,
			     char *optarg);

/* Parse remaining ARGC arguments ARGV on the command line. IFP has
   the same meaning as in system_parse_opt.  (There is some
   post-processing, so you are not reliefed from setting IPF is
   appropriate.)
   Return 0 if all options were not recognized, otherwise 1.  */
extern int system_parse_opt_rest (struct ifconfig **ifp, int argc,
				  char *argv[]);


/* Output format support.  */

/* Define this if you want to set a system specific default output
   format.  Possible value is a string with the same meaning as an
   argument to the --format option, e.g. `"gnu"', or `"${name}:"'.  */
extern const char *system_default_format;

/* Define this to a list of struct format_handler items.  Add a
   trailing comma, too.  */
# undef SYSTEM_FORMAT_HANDLER


int system_configure (int sfd, struct ifreq *ifr,
		      struct system_ifconfig *__ifs);



/* For systems which loose.  */

# ifndef HAVE_STRUCT_IFREQ_IFR_INDEX
#  define ifr_index ifr_ifindex
# endif

# ifndef HAVE_STRUCT_IFREQ_IFR_NETMASK
#  define ifr_netmask ifr_addr
# endif

# ifndef HAVE_STRUCT_IFREQ_IFR_BROADADDR
#  define ifr_broadaddr ifr_addr
# endif


extern struct if_nameindex* (*system_if_nameindex) (void);

# if defined __linux__
#  include "system/linux.h"
# elif defined __sun
#  include "system/solaris.h"
# elif defined __QNX__
#  include "system/qnx.h"
# elif defined __DragonFly__ || defined __FreeBSD__ || \
       defined __FreeBSD_kernel__ || \
       defined __NetBSD__ || defined __OpenBSD__
#  include "system/bsd.h"
# else
#  include "system/generic.h"
# endif

#endif
