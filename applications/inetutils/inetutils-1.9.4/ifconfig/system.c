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

#include <config.h>

#if defined __linux__
# include "system/linux.c"
#elif defined __sun
# include "system/solaris.c"
#elif defined __QNX__
# include "system/qnx.c"
# elif defined __DragonFly__ || defined __FreeBSD__ || \
       defined __FreeBSD_kernel__ || \
       defined __NetBSD__ || defined __OpenBSD__
#  include "system/bsd.c"
#else
# include "system/generic.c"
#endif
