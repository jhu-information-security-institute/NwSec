/*
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,
  2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software Foundation, Inc.

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

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* This code is derived from software contributed to Berkeley by
   Michael Fischbein. */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "fts.h"
#include <string.h>

#include "ls.h"
#include "extern.h"

int
namecmp (const FTSENT *a, const FTSENT *b)
{
  return (strcmp (a->fts_name, b->fts_name));
}

int
revnamecmp (const FTSENT *a, const FTSENT *b)
{
  return (strcmp (b->fts_name, a->fts_name));
}

int
modcmp (const FTSENT *a, const FTSENT *b)
{
  if (b->fts_statp->st_mtime > a->fts_statp->st_mtime
      ||
#ifdef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
      ( b->fts_statp->st_mtime == a->fts_statp->st_mtime
	&&
	b->fts_statp->st_mtim.tv_nsec > a->fts_statp->st_mtim.tv_nsec
      )
#elif defined HAVE_STRUCT_STAT_ST_MTIM_TV_USEC
      ( b->fts_statp->st_mtime == a->fts_statp->st_mtime
	&&
	b->fts_statp->st_mtim.tv_usec > a->fts_statp->st_mtim.tv_usec
      )
#else
      0
#endif
     )
    return (1);
  else if (b->fts_statp->st_mtime < a->fts_statp->st_mtime
	   ||
#ifdef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
	   ( b->fts_statp->st_mtime == a->fts_statp->st_mtime
	     &&
	     b->fts_statp->st_mtim.tv_nsec < a->fts_statp->st_mtim.tv_nsec
	   )
#elif defined HAVE_STRUCT_STAT_ST_MTIM_TV_USEC
	   ( b->fts_statp->st_mtime == a->fts_statp->st_mtime
	     &&
	     b->fts_statp->st_mtim.tv_usec < a->fts_statp->st_mtim.tv_usec
	   )
#else
	   0
#endif
	  )
    return (-1);
  else
    return (namecmp (a, b));
}

int
revmodcmp (const FTSENT *a, const FTSENT *b)
{
  return (- modcmp (a, b));
}

int
acccmp (const FTSENT *a, const FTSENT *b)
{
  if (b->fts_statp->st_atime > a->fts_statp->st_atime
      ||
#ifdef HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC
      ( b->fts_statp->st_atime == a->fts_statp->st_atime
	&&
	b->fts_statp->st_atim.tv_nsec > a->fts_statp->st_atim.tv_nsec
      )
#elif defined HAVE_STRUCT_STAT_ST_ATIM_TV_USEC
      ( b->fts_statp->st_atime == a->fts_statp->st_atime
	&&
	b->fts_statp->st_atim.tv_usec > a->fts_statp->st_atim.tv_usec
      )
#else
      0
#endif
     )
    return (1);
  else if (b->fts_statp->st_atime < a->fts_statp->st_atime
	   ||
#ifdef HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC
	   ( b->fts_statp->st_atime == a->fts_statp->st_atime
	     &&
	     b->fts_statp->st_atim.tv_nsec < a->fts_statp->st_atim.tv_nsec
	   )
#elif defined HAVE_STRUCT_STAT_ST_ATIM_TV_USEC
	   ( b->fts_statp->st_atime == a->fts_statp->st_atime
	     &&
	     b->fts_statp->st_atim.tv_usec < a->fts_statp->st_atim.tv_usec
	   )
#else
	   0
#endif
	  )
    return (-1);
  else
    return (namecmp (a, b));
}

int
revacccmp (const FTSENT *a, const FTSENT *b)
{
  return (- acccmp (a, b));
}

int
statcmp (const FTSENT *a, const FTSENT *b)
{
  if (b->fts_statp->st_ctime > a->fts_statp->st_ctime
      ||
#ifdef HAVE_STRUCT_STAT_ST_CTIM_TV_NSEC
      ( b->fts_statp->st_ctime == a->fts_statp->st_ctime
	&&
	b->fts_statp->st_ctim.tv_nsec > a->fts_statp->st_ctim.tv_nsec
      )
#elif defined HAVE_STRUCT_STAT_ST_CTIM_TV_USEC
      ( b->fts_statp->st_ctime == a->fts_statp->st_ctime
	&&
	b->fts_statp->st_ctim.tv_usec > a->fts_statp->st_ctim.tv_usec
      )
#else
      0
#endif
     )
    return (1);
  else if (b->fts_statp->st_ctime < a->fts_statp->st_ctime
	   ||
#ifdef HAVE_STRUCT_STAT_ST_CTIM_TV_NSEC
	   ( b->fts_statp->st_ctime == a->fts_statp->st_ctime
	     &&
	     b->fts_statp->st_ctim.tv_nsec < a->fts_statp->st_ctim.tv_nsec
	   )
#elif defined HAVE_STRUCT_STAT_ST_CTIM_TV_USEC
	   ( b->fts_statp->st_ctime == a->fts_statp->st_ctime
	     &&
	     b->fts_statp->st_ctim.tv_usec < a->fts_statp->st_ctim.tv_usec
	   )
#else
	   0
#endif
	  )
    return (-1);
  else
    return (namecmp (a, b));
}

int
revstatcmp (const FTSENT *a, const FTSENT *b)
{
  return (- statcmp (a, b));
}

int
sizecmp (const FTSENT *a, const FTSENT *b)
{
  if (b->fts_statp->st_size > a->fts_statp->st_size)
    return (1);
  if (b->fts_statp->st_size < a->fts_statp->st_size)
    return (-1);
  else
    return (namecmp (a, b));
}

int
revsizecmp (const FTSENT *a, const FTSENT *b)
{
  return (- sizecmp (a, b));
}
