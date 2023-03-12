/* argcv.c - simple functions for parsing input based on whitespace
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

#include "argcv.h"
#include <ctype.h>
#include <progname.h>

/*
 * takes a string and splits it into several strings, breaking at ' '
 * command is the string to split
 * the number of strings is placed into argc
 * the split strings are put into argv
 * returns 0 on success, nonzero on failure
 */

#define isws(c) ((c)==' '||(c)=='\t')
#define isdelim(c,delim) ((c)=='"'||strchr(delim,(c))!=NULL)

static int
argcv_scan (int len, const char *command, const char *delim,
	    int *start, int *end, int *save)
{
  int i = *save;

  /* Skip initial whitespace */
  while (i < len && isws (command[i]))
    i++;
  *start = i;

  switch (command[i])
    {
    case '"':
    case '\'':
      while (++i < len && command[i] != command[*start])
	;
      if (i < len)		/* found matching quote */
	break;
    default:
      if (isdelim (command[i], delim))
	break;
      /* Skip until next whitespace character or end of line */
      while (++i < len && !(isws (command[i]) || isdelim (command[i], delim)))
	;
      i--;
      break;
    }

  *end = i;
  *save = i + 1;
  return *save;
}

int
argcv_get (const char *command, const char *delim, int *argc, char ***argv)
{
  int len = strlen (command);
  int i = 0;
  int start, end, save;

  *argc = 0;
  *argv = NULL;

  while (len > 0 && isspace ((int) command[len - 1]))
    len--;
  if (len < 1)
    return 1;

  /* Count number of arguments */
  *argc = 1;
  save = 0;
  while (argcv_scan (len, command, delim, &start, &end, &save) < len)
    (*argc)++;

  *argv = calloc ((*argc + 1), sizeof (char *));

  i = 0;
  save = 0;
  for (i = 0; i < *argc; i++)
    {
      int n;
      argcv_scan (len, command, delim, &start, &end, &save);

      if (command[start] == '"' && command[end] == '"')
	{
	  start++;
	  end--;
	}
      else if (command[start] == '\'' && command[end] == '\'')
	{
	  start++;
	  end--;
	}
      n = end - start + 1;
      (*argv)[i] = calloc (n + 1, sizeof (char));
      if ((*argv)[i] == NULL)
	return 1;
      memcpy ((*argv)[i], &command[start], n);
      (*argv)[i][n] = 0;
    }
  (*argv)[i] = NULL;
  return 0;
}

/*
 * frees all elements of an argv array
 * argc is the number of elements
 * argv is the array
 */
int
argcv_free (int argc, char **argv)
{
  while (--argc >= 0)
    free (argv[argc]);
  free (argv);
  return 1;
}

/* Take a argv an make string separated by ' '.  */

int
argcv_string (int argc, char **argv, char **pstring)
{
  int i;
  size_t len;
  char *buffer;

  /* No need.  */
  if (pstring == NULL)
    return 1;

  buffer = malloc (1);
  if (buffer == NULL)
    return 1;
  *buffer = '\0';

  for (len = i = 0; i < argc; i++)
    {
      len += strlen (argv[i]) + 2;
      buffer = realloc (buffer, len);
      if (buffer == NULL)
	return 1;
      if (i != 0)
	strcat (buffer, " ");
      strcat (buffer, argv[i]);
    }

  /* Strip off trailing space.  */
  if (*buffer != '\0')
    {
      while (buffer[strlen (buffer) - 1] == ' ')
	{
	  buffer[strlen (buffer) - 1] = '\0';
	}
    }
  if (pstring)
    *pstring = buffer;
  return 0;
}

#if 0
char *command = "set prompt=\"& \"";

int
main (int argc, char **argv)
{
  int i, argc;
  char **argv;
  set_program_name (argv[0]);
  argcv_get (command, "=", &argc, &argv);
  printf ("%d args:\n", argc);
  for (i = 0; i < argc; i++)
    printf ("%s\n", argv[i]);
}
#endif
