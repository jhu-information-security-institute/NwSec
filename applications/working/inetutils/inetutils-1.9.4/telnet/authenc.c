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
 * Copyright (c) 1991, 1993
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

#if defined AUTHENTICATION || defined ENCRYPTION
# include <unistd.h>
# include <sys/types.h>
# include <arpa/telnet.h>
# include <libtelnet/encrypt.h>
# include <libtelnet/misc.h>

# include "general.h"
# include "ring.h"
# include "externs.h"
# include "defines.h"
# include "types.h"

int
net_write (unsigned char *str, int len)
{
  if (NETROOM () > len)
    {
      ring_supply_data (&netoring, str, len);
      if (str[0] == IAC && str[1] == SE)
	printsub ('>', &str[2], len - 2);
      return (len);
    }
  return (0);
}

void
net_encrypt ()
{
# ifdef	ENCRYPTION
  if (encrypt_output)
    ring_encrypt (&netoring, encrypt_output);
  else
    ring_clearto (&netoring);
# endif	/* ENCRYPTION */
}

int
telnet_spin ()
{
  return (-1);
}

char *
telnet_getenv (char *val)
{
  return ((char *) env_getvalue (val));
}

char *
telnet_gets (char *prompt, char *result, int length, int echo)
{
# if !HAVE_DECL_GETPASS
  extern char *getpass ();
# endif
  extern int globalmode;
  int om = globalmode;
  char *res;

  TerminalNewMode (-1);
  if (echo)
    {
      printf ("%s", prompt);
      res = fgets (result, length, stdin);
    }
  else
    {
      res = getpass (prompt);
      if (res)
	{
	  strncpy (result, res, length);
	  memset (res, 0, strlen (res));
	  res = result;
	}
    }
  TerminalNewMode (om);
  return (res);
}
#endif /* defined(AUTHENTICATION) || defined(ENCRYPTION) */
