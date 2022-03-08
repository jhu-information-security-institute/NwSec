/*
  Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
  2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

/*
 * Copyright (c) 1992, 1993
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

/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * This routine prints the supplied string to standard
 * output as a prompt, and reads a password string without
 * echoing.
 */

#include <config.h>

#if defined RSA_ENCPWD || defined KRB4_ENCPWD

# include <stdio.h>
# include <sys/ioctl.h>
# include <setjmp.h>

static jmp_buf env;

/*** Routines ****************************************************** */
/*
 * This version just returns the string, doesn't map to key.
 *
 * Returns 0 on success, non-zero on failure.
 */

int
local_des_read_pw_string (s, max, prompt, verify)
     char *s;
     int max;
     char *prompt;
     int verify;
{
  int ok = 0;
  char *ptr;

  jmp_buf old_env;
  struct sgttyb tty_state;
  char key_string[BUFSIZ];

  if (max > BUFSIZ)
    {
      return -1;
    }

  /* XXX assume jmp_buf is typedef'ed to an array */
  memmove ((char *) env, (char *) old_env, sizeof (env));
  if (setjmp (env))
    goto lose;

  /* save terminal state */
  if (ioctl (0, TIOCGETP, (char *) &tty_state) == -1)
    return -1;
/*
    push_signals();
*/
  /* Turn off echo */
  tty_state.sg_flags &= ~ECHO;
  if (ioctl (0, TIOCSETP, (char *) &tty_state) == -1)
    return -1;
  while (!ok)
    {
      printf ("%s", prompt);
      fflush (stdout);
      while (!fgets (s, max, stdin));

      if ((ptr = strchr (s, '\n')))
	*ptr = '\0';
      if (verify)
	{
	  printf ("\nVerifying, please re-enter %s", prompt);
	  fflush (stdout);
	  if (!fgets (key_string, sizeof (key_string), stdin))
	    {
	      clearerr (stdin);
	      continue;
	    }
	  if ((ptr = strchr (key_string, '\n')))
	    *ptr = '\0';
	  if (strcmp (s, key_string))
	    {
	      printf ("\n\07\07Mismatch - try again\n");
	      fflush (stdout);
	      continue;
	    }
	}
      ok = 1;
    }

lose:
  if (!ok)
    memset (s, 0, max);
  printf ("\n");
  /* turn echo back on */
  tty_state.sg_flags |= ECHO;
  if (ioctl (0, TIOCSETP, (char *) &tty_state))
    ok = 0;
/*
    pop_signals();
*/
  memmove ((char *) old_env, (char *) env, sizeof (env));
  if (verify)
    memset (key_string, 0, sizeof (key_string));
  s[max - 1] = 0;		/* force termination */
  return !ok;			/* return nonzero if not okay */
}
#endif /* defined(RSA_ENCPWD) || defined(KRB4_ENCPWD) */
