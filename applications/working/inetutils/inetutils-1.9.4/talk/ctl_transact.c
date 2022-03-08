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
 * Copyright (c) 1983, 1993
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <protocols/talkd.h>
#include <errno.h>
#include "talk_ctl.h"
#include "talk.h"

#define CTL_WAIT 2		/* time to wait for a response, in seconds */

/*
 * SOCKDGRAM is unreliable, so we must repeat messages if we have
 * not recieved an acknowledgement within a reasonable amount
 * of time
 */
int
ctl_transact (struct in_addr target, CTL_MSG msg, int type, CTL_RESPONSE * rp)
{
  int nready, cc;
  fd_set read_mask, ctl_mask;
  struct timeval wait;

  msg.type = type;
  daemon_addr.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  daemon_addr.sin_len = sizeof (daemon_addr);
#endif
  daemon_addr.sin_addr = target;
  daemon_addr.sin_port = htons (daemon_port);
  FD_ZERO (&ctl_mask);
  FD_SET (ctl_sockt, &ctl_mask);

  /*
   * Keep sending the message until a response of
   * the proper type is obtained.
   */
  do
    {
      wait.tv_sec = CTL_WAIT;
      wait.tv_usec = 0;
      /* resend message until a response is obtained */
      do
	{
	  cc = sendto (ctl_sockt, (char *) &msg, sizeof (msg), 0,
		       (struct sockaddr *) &daemon_addr,
		       sizeof (daemon_addr));
	  if (cc != sizeof (msg))
	    {
	      if (errno == EINTR)
		continue;
	      p_error ("Error on write to talk daemon");
	    }
	  read_mask = ctl_mask;
	  nready = select (32, &read_mask, 0, 0, &wait);
	  if (nready < 0)
	    {
	      if (errno == EINTR)
		continue;
	      p_error ("Error waiting for daemon response");
	    }
	}
      while (nready == 0);
      /*
       * Keep reading while there are queued messages
       * (this is not necessary, it just saves extra
       * request/acknowledgements being sent)
       */
      do
	{
	  cc = recv (ctl_sockt, (char *) rp, sizeof (*rp), 0);
	  if (cc < 0)
	    {
	      if (errno == EINTR)
		continue;
	      p_error ("Error on read from talk daemon");
	    }
	  read_mask = ctl_mask;
	  /* an immediate poll */
	  timerclear (&wait);
	  nready = select (32, &read_mask, 0, 0, &wait);
	}
      while (nready > 0 && (rp->vers != TALK_VERSION || rp->type != type));
    }
  while (rp->vers != TALK_VERSION || rp->type != type);
  rp->id_num = ntohl (rp->id_num);
  rp->addr.sa_family = ntohs (rp->addr.sa_family);

  return 0;
}
