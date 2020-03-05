/*
  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
  2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

#ifndef _WHOIS_H
# define _WHOIS_H

/* 6bone referto: extension */
# define REFERTO_FORMAT	"%% referto: whois -h %255s -p %15s %1021[^\n\r]"

/* String sent to RIPE servers - ONLY FIVE CHARACTERS! */
/* Do *NOT* change it if you don't know what you are doing! */
# define IDSTRING "Md4.5"

/* system features */
# ifdef ENABLE_NLS
#  ifndef NLS_CAT_NAME
#   define NLS_CAT_NAME   "whois"
#  endif
#  ifndef LOCALEDIR
#   define LOCALEDIR     "/usr/share/locale"
#  endif
# endif

/* NLS stuff */
# ifdef ENABLE_NLS
#  include <libintl.h>
#  include <locale.h>
#  define _(a) (gettext (a))
#  ifdef gettext_noop
#   define N_(a) gettext_noop (a)
#  else
#   define N_(a) (a)
#  endif
# else
#  define _(a) (a)
#  define N_(a) a
# endif


/* prototypes */
const char *whichwhois (const char *);
const char *whereas (int, struct as_del[]);
char *queryformat (const char *, const char *, const char *);
void do_query (const int, const char *);
const char *query_crsnic (const int, const char *);
int openconn (const char *, const char *);
void closeconn (const int);
void sighandler (int);
unsigned long myinet_aton (const char *);
int domcmp (const char *, const char *);
int domfind (const char *, const char *[]);

void err_quit (const char *, ...);
void err_sys (const char *, ...);


/* flags for RIPE-like servers */
const char *ripeflags = "acFlLmMrRSx";
const char *ripeflagsp = "gisTtvq";

/* Configurable features */

/* 6bone referto: support */
# define EXT_6BONE

/* Always hide legal disclaimers */
# undef ALWAYS_HIDE_DISCL

/* Default server */
# define DEFAULTSERVER   "whois.internic.net"

#endif /* _WHOIS_H */
