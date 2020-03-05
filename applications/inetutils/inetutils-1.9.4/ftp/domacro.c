/*
  Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
  2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014,
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

/*
 * Copyright (c) 1985, 1993, 1994
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

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftp_var.h"

/* Increase allocated length of `*startl' with `add'
 * beyond `*track',  should present length not suffice.
 * `*track' points to last character in use.  All of
 * the values `*start', `*track', * and `*size' are
 * updated during a successful reallocation.
 * Return zero on success.
 */
static int
lengthen (char **start, char **track, size_t *size, size_t add)
{
  char *new;
  size_t offset = (size_t) (*track - *start);

  if (*track < *start)
    return EXIT_FAILURE;	/* Sanity check.  */

  if (offset + add < *size)
    return EXIT_SUCCESS;	/* Sufficient allocation.  */

  new = realloc (*start, *size + add);
  if (!new)
    return EXIT_FAILURE;	/* Parameters are unchanged here.  */

  *start = new;
  *size += add;
  *track = *start + offset;

  return EXIT_SUCCESS;
}

void
domacro (int argc, char *argv[])
{
  int i, j, count = 2, loopflg = 0, allocflg = 0;
  char *cp1, *cp2;
  char *line2;		/* Saved original of `line'.  */
  size_t line2len;	/* Its allocated length.  */
  struct cmd *c;

  if (argc < 2 && !another (&argc, &argv, "macro name"))
    {
      printf ("Usage: %s macro_name.\n", argv[0]);
      code = -1;
      return;
    }
  for (i = 0; i < macnum; ++i)
    {
      if (!strncmp (argv[1], macros[i].mac_name,
		    sizeof (macros[i].mac_name)))
	{
	  break;
	}
    }
  if (i == macnum)
    {
      printf ("'%s' macro not found.\n", argv[1]);
      code = -1;
      return;
    }

  line2 = line;
  line2len = linelen;

  /* Generate a replacement for `line' to be used during
   * macro evaluation.  There might appear some intr(),
   * so care must be taken before changing `line'.
   * The original is available as LINE2.
   *
   * Initially allocate an amount sufficient for
   * storing a copy of the original `line', which
   * is repeatedly reused once for each text line
   * of the stored macro definition.
   */
  cp2 = malloc (strlen (line2) + 2);
  if (!cp2)
    {
      printf ("System refused resources for macro '%s'.\n", argv[1]);
      line = line2;
      linelen = line2len;
      code = -1;
      return;
    }

  linelen = strlen (line2) + 2;
  line = cp2;
  *line = '\0';

  do
    {
      cp1 = macros[i].mac_start;
      while (cp1 != macros[i].mac_end)
	{
	  /* Skip initial white space on each line of input.  */
	  while (isspace (*cp1))
	    {
	      cp1++;
	    }
	  /* Translate a line of text from macro definition
	   * and put it in `line'.  This global variable is
	   * referenced by some parsing functions, so the
	   * translation target cannot be changed easily.
	   */
	  cp2 = line;
	  while (*cp1 != '\0')
	    {
	      /* Usually two characters suffice.
	       * This covers the default case below.
	       */
	      if (lengthen (&line, &cp2, &linelen, 2))
		{
		  allocflg = 1;
		  goto end_exec;
		}
	      switch (*cp1)
		{
		case '\\':		/* Escaping character.  */
		  *cp2++ = *++cp1;
		  break;
		case '$':		/* Substitution.  */
		  if (isdigit (*(cp1 + 1)))
		    {
		      /* Argument expansion.  */
		      j = 0;
		      while (isdigit (*++cp1))
			j = 10 * j + *cp1 - '0';
		      cp1--;
		      if (argc - 2 >= j)
			{
			  if (lengthen (&line, &cp2, &linelen,
					strlen (argv[j + 1]) + 2))
			    {
			      allocflg = 1;
			      goto end_exec;
			    }
			  strcpy (cp2, argv[j + 1]);
			  cp2 += strlen (argv[j + 1]);
			}
		      break;
		    }
		  if (*(cp1 + 1) == 'i')
		    {
		      /* The loop counter "$i" was detected.  */
		      loopflg = 1;
		      cp1++;		/* Back to last used char.  */
		      if (count < argc)
			{
			  if (lengthen (&line, &cp2, &linelen,
					strlen (argv[count]) + 2))
			    {
			      allocflg = 1;
			      goto end_exec;
			    }
			  strcpy (cp2, argv[count]);
			  cp2 += strlen (argv[count]);
			}
		      break;
		    }
		  /* Intentional fall through, since no acceptable
		   * use of '$' was detected.  Present input is the
		   * dollar sign.
		   */
		default:
		  *cp2++ = *cp1;	/* Copy present character.  */
		  break;
		}
	      /* Advance to next usable character.  */
	      if (*cp1 != '\0')
		cp1++;
	    }
	  *cp2 = '\0';
	  makeargv ();
	  if (margv[0] == NULL)
	    return;
	  c = getcmd (margv[0]);

	  if (c == (struct cmd *) -1)
	    {
	      printf ("?Ambiguous command: '%s'.\n", margv[0]);
	      code = -1;
	      loopflg = 0;
	      break;
	    }
	  else if (c == 0)
	    {
	      printf ("?Invalid command: '%s'.\n", margv[0]);
	      code = -1;
	      loopflg = 0;
	      break;
	    }
	  else if (c->c_conn && !connected)
	    {
	      printf ("Not connected, needed for '%s'.\n", margv[0]);
	      code = -1;
	      loopflg = 0;
	      break;
	    }
	  else
	    {
	      if (verbose)
		printf ("%s\n", line);
	      (*c->c_handler) (margc, margv);
	      if (bell && c->c_bell)
		putchar ('\007');

	      /* The arguments set at the time of invoking
	       * the macro must be recovered, to be used
	       * in parsing next line of macro definition.
	       */
	      strcpy (line, line2);	/* Known to fit.  */
	      makeargv ();		/* Get the arguments.  */
	      argc = margc;
	      argv = margv;
	    }
	  if (cp1 != macros[i].mac_end)
	    cp1++;
	}
    }
  while (loopflg && ++count < argc);

end_exec:
  if (allocflg)
    {
      printf ("Memory allocation failed for macro '%s'.\n", argv[1]);
      code = -1;
    }
  free (line);
  line = line2;
  linelen = line2len;
}
