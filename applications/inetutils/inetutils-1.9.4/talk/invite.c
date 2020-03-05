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

#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <netinet/in.h>
#include <protocols/talkd.h>
#include <errno.h>
#include <unistd.h>
#include <setjmp.h>
#include <unused-parameter.h>
#include "talk_ctl.h"
#include "talk.h"

static char *answers[] = {
  "answer #0",			/* SUCCESS */
  "Your party is not logged on",	/* NOT_HERE */
  "Target machine is too confused to talk to us",	/* FAILED */
  "Target machine does not recognize us",	/* MACHINE_UNKNOWN */
  "Your party is refusing messages",	/* PERMISSION_REFUSED */
  "Target machine cannot handle remote talk",	/* UNKNOWN_REQUEST */
  "Target machine indicates protocol mismatch",	/* BADVERSION */
  "Target machine indicates protocol botch (addr)",	/* BADADDR */
  "Target machine indicates protocol botch (ctl_addr)",	/* BADCTLADDR */
};

#define NANSWERS	(sizeof (answers) / sizeof (answers[0]))

/*
 * The msg.id's for the invitations
 * on the local and remote machines.
 * These are used to delete the
 * invitations.
 */
int local_id, remote_id;
jmp_buf invitebuf;

/*
 * Transmit the invitation and process the response
 */
int
announce_invite (void)
{
  CTL_RESPONSE response;

  current_state = "Trying to connect to your party's talk daemon";
  ctl_transact (his_machine_addr, msg, ANNOUNCE, &response);
  remote_id = response.id_num;
  if (response.answer != SUCCESS)
    {
      if (response.answer < NANSWERS)
	message (answers[response.answer]);
      quit ();
    }
  /* leave the actual invitation on my talk daemon */
  ctl_transact (my_machine_addr, msg, LEAVE_INVITE, &response);
  local_id = response.id_num;

  return 0;
}

/*
 * Routine called on interupt to re-invite the callee
 */
void
re_invite (int sig _GL_UNUSED_PARAMETER)
{

  message ("Ringing your party again");
  current_line++;
  /* force a re-announce */
  msg.id_num = htonl (remote_id + 1);
  announce_invite ();
  longjmp (invitebuf, 1);
}

int
invite_remote (void)
{
  int new_sockt;
  struct itimerval itimer;
  CTL_RESPONSE response;

  itimer.it_value.tv_sec = RING_WAIT;
  itimer.it_value.tv_usec = 0;
  itimer.it_interval = itimer.it_value;
  if (listen (sockt, 5) != 0)
    p_error ("Error on attempt to listen for caller");

  msg.addr.sa_family = htons (my_addr.sin_family);
  memcpy (msg.addr.sa_data,
	  ((struct sockaddr *) &my_addr)->sa_data,
	  sizeof ((struct sockaddr *) & my_addr)->sa_data);

  msg.id_num = htonl (-1);	/* an impossible id_num */
  invitation_waiting = 1;
  announce_invite ();
  /*
   * Shut off the automatic messages for a while,
   * so we can use the interupt timer to resend the invitation
   */
  end_msgs ();
  setitimer (ITIMER_REAL, &itimer, (struct itimerval *) 0);
  message ("Waiting for your party to respond");
  signal (SIGALRM, re_invite);
  setjmp (invitebuf);
  while ((new_sockt = accept (sockt, 0, 0)) < 0)
    {
      if (errno == EINTR)
	continue;
      p_error ("Unable to connect with your party");
    }
  close (sockt);
  sockt = new_sockt;

  /*
   * Have the daemons delete the invitations now that we
   * have connected.
   */
  current_state = "Waiting for your party to respond";
  start_msgs ();

  msg.id_num = htonl (local_id);
  ctl_transact (my_machine_addr, msg, DELETE, &response);
  msg.id_num = htonl (remote_id);
  ctl_transact (his_machine_addr, msg, DELETE, &response);
  invitation_waiting = 0;

  return 0;
}

/*
 * Tell the daemon to remove your invitation
 */
int
send_delete (void)
{

  msg.type = DELETE;
  /*
   * This is just a extra clean up, so just send it
   * and don't wait for an answer
   */
  msg.id_num = htonl (remote_id);
  daemon_addr.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  daemon_addr.sin_len = sizeof (daemon_addr);
#endif
  daemon_addr.sin_addr = his_machine_addr;
  if (sendto (ctl_sockt, (const char *) &msg, sizeof (msg), 0,
	      (struct sockaddr *) &daemon_addr,
	      sizeof (daemon_addr)) != sizeof (msg))
    perror ("send_delete (remote)");
  msg.id_num = htonl (local_id);
  daemon_addr.sin_addr = my_machine_addr;
  if (sendto (ctl_sockt, (const char *) &msg, sizeof (msg), 0,
	      (struct sockaddr *) &daemon_addr,
	      sizeof (daemon_addr)) != sizeof (msg))
    perror ("send_delete (local)");

  return 0;
}
