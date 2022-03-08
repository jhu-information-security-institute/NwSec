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
 * Copyright (c) 1989, 1993, 1994
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
   Michael Fischbein.  */

#include <config.h>

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include "fts.h"
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <filemode.h>

#ifdef HAVE_SYS_MKDEV_H
# include <sys/mkdev.h>
#endif

#define DAYSPERNYEAR 365
#define SECSPERDAY 86400

#include "ls.h"
#include "extern.h"

#ifndef howmany
# define howmany(x, y)   (((x)+((y)-1))/(y))
#endif

#ifndef major
# define major(x)        ((int)(((unsigned)(x)>>8)&0377))
#endif

#ifndef minor
# define minor(x)        ((int)((x)&0377))
#endif

static int printaname (FTSENT *, unsigned long, unsigned long);
static void printlink (FTSENT *);
static void printtime (time_t);
static int printtype (u_int);
static int compute_columns (DISPLAY *, int *);

#define IS_NOPRINT(p)	((p)->fts_number == NO_PRINT)

void
printscol (DISPLAY *dp)
{
  FTSENT *p;

  for (p = dp->list; p; p = p->fts_link)
    {
      if (IS_NOPRINT (p))
	continue;
      printaname (p, dp->s_inode, dp->s_block);
      putchar ('\n');
    }
}

void
printlong (DISPLAY *dp)
{
  struct stat *sp;
  FTSENT *p;
  NAMES *np;
  char buf[20];

  if (dp->list->fts_level != FTS_ROOTLEVEL && (f_longform || f_size))
    printf ("total %lu\n", howmany (dp->btotal, blocksize));

  for (p = dp->list; p; p = p->fts_link)
    {
      if (IS_NOPRINT (p))
	continue;
      sp = p->fts_statp;
      if (f_inode)
	printf ("%*lu ", dp->s_inode, (unsigned long) sp->st_ino);
      if (f_size)
	printf ("%*llu ",
		dp->s_block, (long long) howmany (sp->st_blocks, blocksize));
      strmode (sp->st_mode, buf);
      np = p->fts_pointer;
      printf ("%s %*d %-*s  %-*s  ",
	      buf, dp->s_nlink, (int) sp->st_nlink,
	      dp->s_user, np->user, dp->s_group, np->group);
      if (f_flags)
	printf ("%-*s ", dp->s_flags, np->flags);
      if (S_ISCHR (sp->st_mode) || S_ISBLK (sp->st_mode))
	printf ("%3d, %3d ", major (sp->st_rdev), minor (sp->st_rdev));
      else if (dp->bcfile)
	printf ("%*s%*llu ",
		8 - dp->s_size, "", dp->s_size, (long long) sp->st_size);
      else
	printf ("%*llu ", dp->s_size, (long long) sp->st_size);
      if (f_accesstime)
	printtime (sp->st_atime);
      else if (f_statustime)
	printtime (sp->st_ctime);
      else
	printtime (sp->st_mtime);
      putname (p->fts_name);
      if (f_type || (f_typedir && S_ISDIR (sp->st_mode)))
	printtype (sp->st_mode);
      if (S_ISLNK (sp->st_mode))
	printlink (p);
      putchar ('\n');
    }
}

static int
compute_columns (DISPLAY *dp, int *pnum)
{
  int colwidth;
  extern int termwidth;
  int mywidth;

  colwidth = dp->maxlen;
  if (f_inode)
    colwidth += dp->s_inode + 1;
  if (f_size)
    colwidth += dp->s_block + 1;
  if (f_type || f_typedir)
    colwidth += 1;

  colwidth += 1;
  mywidth = termwidth + 1;	/* no extra space for last column */

  if (mywidth < 2 * colwidth)
    {
      printscol (dp);
      return (0);
    }

  *pnum = mywidth / colwidth;
  return (mywidth / *pnum);	/* spread out if possible */
}

void
printcol (DISPLAY *dp)
{
  static FTSENT **array;
  static int lastentries = -1;
  FTSENT *p;
  int base, chcnt, col, colwidth, num;
  int numcols, numrows, row;

  if ((colwidth = compute_columns (dp, &numcols)) == 0)
    return;
  /*
   * Have to do random access in the linked list -- build a table
   * of pointers.
   */
  if (dp->entries > lastentries)
    {
      FTSENT **a;

      if ((a = realloc (array, dp->entries * sizeof (FTSENT *))) == NULL)
	{
	  fprintf (stderr, "realloci: %s \n", strerror (errno));
	  printscol (dp);
	  return;
	}
      lastentries = dp->entries;
      array = a;
    }
  for (p = dp->list, num = 0; p; p = p->fts_link)
    if (p->fts_number != NO_PRINT)
      array[num++] = p;

  numrows = num / numcols;
  if (num % numcols)
    ++numrows;

  if (dp->list->fts_level != FTS_ROOTLEVEL && (f_longform || f_size))
    printf ("total %lu\n", howmany (dp->btotal, blocksize));
  for (row = 0; row < numrows; ++row)
    {
      for (base = row, col = 0;;)
	{
	  chcnt = printaname (array[base], dp->s_inode, dp->s_block);
	  if ((base += numrows) >= num)
	    break;
	  if (++col == numcols)
	    break;
	  while (chcnt++ < colwidth)
	    putchar (' ');
	}
      putchar ('\n');
    }
}

/*
 * print [inode] [size] name
 * return # of characters printed, no trailing characters.
 */
static int
printaname (FTSENT *p, unsigned long inodefield, unsigned long sizefield)
{
  struct stat *sp;
  int chcnt;

  sp = p->fts_statp;
  chcnt = 0;
  if (f_inode)
    chcnt += printf ("%*lu ", (int) inodefield, (unsigned long) sp->st_ino);
  if (f_size)
    chcnt += printf ("%*llu ",
		     (int) sizefield, (long long) howmany (sp->st_blocks,
							   blocksize));
  chcnt += putname (p->fts_name);
  if (f_type || (f_typedir && S_ISDIR (sp->st_mode)))
    chcnt += printtype (sp->st_mode);
  return (chcnt);
}

static void
printtime (time_t ftime)
{
  int i;
  char *longstring;

  longstring = ctime (&ftime);
  for (i = 4; i < 11; ++i)
    putchar (longstring[i]);

#define SIXMONTHS	((DAYSPERNYEAR / 2) * SECSPERDAY)
  if (f_sectime)
    for (i = 11; i < 24; i++)
      putchar (longstring[i]);
  else if (ftime + SIXMONTHS > time (NULL))
    for (i = 11; i < 16; ++i)
      putchar (longstring[i]);
  else
    {
      putchar (' ');
      for (i = 20; i < 24; ++i)
	putchar (longstring[i]);
    }
  putchar (' ');
}

void
printacol (DISPLAY *dp)
{
  FTSENT *p;
  int chcnt, col, colwidth;
  int numcols;

  if ((colwidth = compute_columns (dp, &numcols)) == 0)
    return;

  if (dp->list->fts_level != FTS_ROOTLEVEL && (f_longform || f_size))
    printf ("total %llu\n", (long long) (howmany (dp->btotal, blocksize)));
  col = 0;
  for (p = dp->list; p; p = p->fts_link)
    {
      if (IS_NOPRINT (p))
	continue;
      if (col >= numcols)
	{
	  col = 0;
	  putchar ('\n');
	}
      chcnt = printaname (p, dp->s_inode, dp->s_block);
      col++;
      if (col < numcols)
	while (chcnt++ < colwidth)
	  putchar (' ');
    }
  putchar ('\n');
}

void
printstream (DISPLAY *dp)
{
  extern int termwidth;
  FTSENT *p;
  int col;
  int extwidth;

  extwidth = 0;
  if (f_inode)
    extwidth += dp->s_inode + 1;
  if (f_size)
    extwidth += dp->s_block + 1;
  if (f_type)
    extwidth += 1;

  for (col = 0, p = dp->list; p != NULL; p = p->fts_link)
    {
      if (IS_NOPRINT (p))
	continue;
      if (col > 0)
	{
	  putchar (','), col++;
	  if (col + 1 + extwidth + p->fts_namelen >= termwidth)
	    putchar ('\n'), col = 0;
	  else
	    putchar (' '), col++;
	}
      col += printaname (p, dp->s_inode, dp->s_block);
    }
  putchar ('\n');
}

static int
printtype (u_int mode)
{
  switch (mode & S_IFMT)
    {
    case S_IFDIR:
      putchar ('/');
      return (1);
    case S_IFIFO:
      putchar ('|');
      return (1);
    case S_IFLNK:
      putchar ('@');
      return (1);
    case S_IFSOCK:
      putchar ('=');
      return (1);
    }
  if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
    {
      putchar ('*');
      return (1);
    }
  return (0);
}

static void
printlink (FTSENT *p)
{
  int lnklen;
#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif
  char name[MAXPATHLEN], path[MAXPATHLEN];

  if (p->fts_level == FTS_ROOTLEVEL)
    snprintf (name, sizeof (name), "%s", p->fts_name);
  else
    snprintf (name, sizeof (name),
	      "%s/%s", p->fts_parent->fts_accpath, p->fts_name);
  if ((lnklen = readlink (name, path, sizeof (path) - 1)) == -1)
    {
      fprintf (stderr, "\nls: %s: %s\n", name, strerror (errno));
      return;
    }
  path[lnklen] = '\0';
  printf (" -> ");
  putname (path);
}
