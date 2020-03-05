/*
  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014,
  2015 Free Software Foundation, Inc.

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

#ifndef HAVE_CONFIG_H
#error "<config.h> has not been included; please included it"
#endif

#include "argp-version-etc.h"
#include <signal.h>

sighandler_t setsig (int sig, sighandler_t handler);
void utmp_init (char *line, char *user, char *id, char *host);
char *utmp_ptsid (char *line, char *tag);
void utmp_logout (char *line);
char *localhost (void);
void logwtmp (const char *, const char *, const char *);
void cleanup_session (char *tty, int pty_fd);
void logwtmp_keep_open (char *line, char *name, char *host);

#ifndef HAVE_STRUCT_IF_NAMEINDEX
struct if_nameindex
{
  char *if_name;
  int if_index;
};

unsigned int if_nametoindex (const char *ifname);
struct if_nameindex *if_nameindex (void);
void if_freenameindex (struct if_nameindex *ptr);
#endif

extern const char *default_program_authors[];

#define iu_argp_init(name, authors)				\
  argp_program_bug_address = "<" PACKAGE_BUGREPORT ">";		\
  argp_version_setup (name, authors);

#ifdef HAVE_FORK
# define fork_exit(s) exit(s)
#else
# define fork() vfork()
# define fork_exit(s) _exit(s)
#endif
