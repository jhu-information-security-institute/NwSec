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
 * Copyright (c) 1988, 1990, 1993
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

#include <stdlib.h>

#include <arpa/telnet.h>
#include <sys/types.h>

#include "ring.h"

#include "externs.h"
#include "types.h"

#ifdef  ENCRYPTION
# include <libtelnet/encrypt.h>
#endif

Ring ttyoring, ttyiring;
unsigned char ttyobuf[2 * BUFSIZ], ttyibuf[BUFSIZ];

int termdata;			/* Debugging flag */

#ifdef	USE_TERMIO
# ifndef VDISCARD
cc_t termFlushChar;
# endif
# ifndef VLNEXT
cc_t termLiteralNextChar;
# endif
# ifndef VSUSP
cc_t termSuspChar;
# endif
# ifndef VWERASE
cc_t termWerasChar;
# endif
# ifndef VREPRINT
cc_t termRprntChar;
# endif
# ifndef VSTART
cc_t termStartChar;
# endif
# ifndef VSTOP
cc_t termStopChar;
# endif
# ifndef VEOL
cc_t termForw1Char;
# endif
# ifndef VEOL2
cc_t termForw2Char;
# endif
# ifndef VSTATUS
cc_t termAytChar;
# endif
#else
cc_t termForw2Char;
cc_t termAytChar;
#endif

/*
 * initialize the terminal data structures.
 */

void
init_terminal (void)
{
  if (ring_init (&ttyoring, ttyobuf, sizeof ttyobuf) != 1)
    {
      exit (EXIT_FAILURE);
    }
  if (ring_init (&ttyiring, ttyibuf, sizeof ttyibuf) != 1)
    {
      exit (EXIT_FAILURE);
    }
  autoflush = TerminalAutoFlush ();
}


/*
 *		Send as much data as possible to the terminal.
 *
 *		Return value:
 *			-1: No useful work done, data waiting to go out.
 *			 0: No data was waiting, so nothing was done.
 *			 1: All waiting data was written out.
 *			 n: All data - n was written out.
 */


int
ttyflush (int drop)
{
  register int n, n0, n1;

  n0 = ring_full_count (&ttyoring);
  if ((n1 = n = ring_full_consecutive (&ttyoring)) > 0)
    {
      if (drop)
	{
	  TerminalFlushOutput ();
	  /* we leave 'n' alone! */
	}
      else
	{
	  n = TerminalWrite ((char *) ttyoring.consume, n);
	}
    }
  if (n > 0)
    {
      if (termdata && n)
	{
	  Dump ('>', ttyoring.consume, n);
	}
      /*
       * If we wrote everything, and the full count is
       * larger than what we wrote, then write the
       * rest of the buffer.
       */
      if (n1 == n && n0 > n)
	{
	  n1 = n0 - n;
	  if (!drop)
	    n1 = TerminalWrite ((char *) ttyoring.bottom, n1);
	  if (n1 > 0)
	    n += n1;
	}
      ring_consumed (&ttyoring, n);
    }
  if (n < 0)
    return -1;
  if (n == n0)
    {
      if (n0)
	return -1;
      return 0;
    }
  return n0 - n + 1;
}


/*
 * These routines decides on what the mode should be (based on the values
 * of various global variables).
 */


int
getconnmode (void)
{
  extern int linemode;
  int mode = 0;
#ifdef	KLUDGELINEMODE
  extern int kludgelinemode;
#endif

  if (In3270)
    return (MODE_FLOW);

  if (my_want_state_is_dont (TELOPT_ECHO))
    mode |= MODE_ECHO;

  if (localflow)
    mode |= MODE_FLOW;

  if (my_want_state_is_will (TELOPT_BINARY))
    mode |= MODE_INBIN;

  if (his_want_state_is_will (TELOPT_BINARY))
    mode |= MODE_OUTBIN;

#ifdef	KLUDGELINEMODE
  if (kludgelinemode)
    {
      if (my_want_state_is_dont (TELOPT_SGA))
	{
	  mode |= (MODE_TRAPSIG | MODE_EDIT);
	  if (dontlecho && (clocks.echotoggle > clocks.modenegotiated))
	    {
	      mode &= ~MODE_ECHO;
	    }
	}
      return (mode);
    }
#endif
  if (my_want_state_is_will (TELOPT_LINEMODE))
    mode |= linemode;
  return (mode);
}

void
setconnmode (int force)
{
#ifdef	ENCRYPTION
  static int enc_passwd = 0;
#endif /* ENCRYPTION */
  register int newmode;

  newmode = getconnmode () | (force ? MODE_FORCE : 0);

  TerminalNewMode (newmode);

#ifdef  ENCRYPTION
  if ((newmode & (MODE_ECHO | MODE_EDIT)) == MODE_EDIT)
    {
      if (my_want_state_is_will (TELOPT_ENCRYPT)
	  && (enc_passwd == 0) && !encrypt_output)
	{
	  encrypt_request_start (0, 0);
	  enc_passwd = 1;
	}
    }
  else
    {
      if (enc_passwd)
	{
	  encrypt_request_end ();
	  enc_passwd = 0;
	}
    }
#endif /* ENCRYPTION */

}


void
setcommandmode (void)
{
  TerminalNewMode (-1);
}
