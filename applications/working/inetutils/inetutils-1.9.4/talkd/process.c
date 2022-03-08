/*
  Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
  2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015
  Free Software Foundation, Inc.

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

#include <intalkd.h>
#include <readutmp.h>

int find_user (char *name, char *tty);
void do_announce (CTL_MSG * mp, CTL_RESPONSE * rp);

int
process_request (CTL_MSG * msg, struct sockaddr_in *sa_in, CTL_RESPONSE * rp)
{
  CTL_MSG *ptr;

  rp->vers = TALK_VERSION;
  rp->type = msg->type;
  rp->id_num = htonl (0);
  if (msg->vers != TALK_VERSION)
    {
      if (logging || debug)
	syslog (LOG_NOTICE, "Bad protocol version %d", msg->vers);
      rp->answer = BADVERSION;
      return 0;
    }

  /* Convert the machine independent represention
   * of the talk protocol to the present architecture.
   * In particular, `msg->addr.sa_family' will be
   * valid for socket initialization.
   */
  msg->id_num = ntohl (msg->id_num);
  msg->addr.sa_family = ntohs (msg->addr.sa_family);
  if (msg->addr.sa_family != AF_INET)
    {
      if (logging || debug)
	syslog (LOG_NOTICE, "Bad address, family %d", msg->addr.sa_family);
      rp->answer = BADADDR;
      return 0;
    }
  /* Convert to valid socket address for this architecture.  */
  msg->ctl_addr.sa_family = ntohs (msg->ctl_addr.sa_family);
  if (msg->ctl_addr.sa_family != AF_INET)
    {
      if (logging || debug)
	syslog (LOG_NOTICE, "Bad control address, family %d",
		msg->ctl_addr.sa_family);
      rp->answer = BADCTLADDR;
      return 0;
    }
  /* FIXME: compare address and sa_in? */

  if (acl_match (msg, sa_in) == ACL_DENY)
    {
      /* This denial happens for each of LOOK_UP,
       * ANNOUNCE, and DELETE, in this order.
       * Make a syslog note only for the first of them.
       */
      if ((logging || debug) && msg->type == LOOK_UP)
	syslog (LOG_NOTICE, "dropping request: %s@%s",
		msg->l_name, inet_ntoa (sa_in->sin_addr));

      /* The answer FAILED is returned to minimize the amount
       * of information disclosure, since ACL has denied access.
       */
      rp->answer = FAILED;
      return 0;
    }

  if (debug)
    {
      print_request ("process_request", msg);
    }

  msg->pid = ntohl (msg->pid);

  switch (msg->type)
    {
    case ANNOUNCE:
      do_announce (msg, rp);
      if (logging && rp->answer == SUCCESS)
	syslog (LOG_INFO, "%s@%s called by %s@%s",
		msg->r_name,
		(msg->r_tty[0]
		  ? msg->r_tty
		  : inet_ntoa (sa_in->sin_addr)),
		msg->l_name,
		inet_ntoa (os2sin_addr (msg->addr)));
      break;

    case LEAVE_INVITE:
      ptr = find_request (msg);
      if (ptr)
	{
	  rp->id_num = htonl (ptr->id_num);
	  rp->answer = SUCCESS;
	}
      else
	insert_table (msg, rp);
      break;

    case LOOK_UP:
      ptr = find_match (msg);
      if (ptr)
	{
	  rp->id_num = htonl (ptr->id_num);
	  rp->addr = ptr->addr;
	  rp->addr.sa_family = htons (ptr->addr.sa_family);
	  rp->answer = SUCCESS;
	  if (logging)
	    syslog (LOG_INFO, "%s talks to %s@%s",
		msg->r_name, msg->l_name, inet_ntoa (sa_in->sin_addr));
	}
      else
	rp->answer = NOT_HERE;
      break;

    case DELETE:
      rp->answer = delete_invite (msg->id_num);
      break;

    default:
      rp->answer = UNKNOWN_REQUEST;
      break;
    }

  if (debug)
    print_response ("process_request response", rp);

  return 0;
}

void
do_announce (CTL_MSG * mp, CTL_RESPONSE * rp)
{
  struct hostent *hp;
  CTL_MSG *ptr;
  int result;

  result = find_user (mp->r_name, mp->r_tty);
  if (result != SUCCESS)
    {
      rp->answer = result;
      return;
    }

  hp = gethostbyaddr ((char *) &os2sin_addr (mp->ctl_addr),
		      sizeof (struct in_addr), AF_INET);
  if (!hp)
    {
      rp->answer = MACHINE_UNKNOWN;
      return;
    }
  ptr = find_request (mp);
  if (!ptr)
    {
      insert_table (mp, rp);
      rp->answer = announce (mp, hp->h_name);
      return;
    }
  if (mp->id_num > ptr->id_num)
    {
      /* Explicit re-announce: update the id_num to avoid duplicates
         and re-announce the talk. */
      ptr->id_num = new_id ();
      rp->id_num = htonl (ptr->id_num);
      rp->answer = announce (mp, hp->h_name);
    }
  else
    {
      /* a duplicated request, so ignore it */
      rp->id_num = htonl (ptr->id_num);
      rp->answer = SUCCESS;
    }
}

/* Search utmp for the local user.  */
int
find_user (char *name, char *tty)
{
  STRUCT_UTMP *uptr;
#ifndef HAVE_GETUTXUSER
  STRUCT_UTMP *utmpbuf;
  size_t utmp_count;
#endif /* HAVE_GETUTXUSER */
  int status;
  struct stat statb;
  char ftty[sizeof (PATH_TTY_PFX) + sizeof (uptr->ut_line)];
  time_t last_time = 0;
  int notty;

  notty = (*tty == '\0');

  status = NOT_HERE;
  strcpy (ftty, PATH_TTY_PFX);

#ifdef HAVE_GETUTXUSER
  setutxent ();

  while ((uptr = getutxuser (name)))
#else /* !HAVE_GETUTXUSER */
  if (read_utmp (UTMP_FILE, &utmp_count, &utmpbuf,
		 READ_UTMP_USER_PROCESS | READ_UTMP_CHECK_PIDS) < 0)
    return FAILED;

  for (uptr = utmpbuf; uptr < utmpbuf + utmp_count; uptr++)
    {
      if (!strncmp (UT_USER (uptr), name, sizeof (UT_USER (uptr))))
#endif /* !HAVE_GETUTXUSER */
	{
	  if (notty)
	    {
	      /* no particular tty was requested */
	      strncpy (ftty + sizeof (PATH_TTY_PFX) - 1,
		       uptr->ut_line,
		       sizeof (ftty) - sizeof (PATH_TTY_PFX) - 1);
	      ftty[sizeof (ftty) - 1] = 0;

	      if (stat (ftty, &statb) == 0)
		{
		  if (!(statb.st_mode & S_IWGRP))
		    {
		      if (status != SUCCESS)
			status = PERMISSION_DENIED;
		      continue;
		    }
		  if (statb.st_atime > last_time)
		    {
		      last_time = statb.st_atime;
		      strcpy (tty, uptr->ut_line);
		      status = SUCCESS;
		    }
		  continue;
		}
	    }
	  if (!strcmp (uptr->ut_line, tty))
	    {
	      status = SUCCESS;
	      break;
	    }
	}
#ifndef HAVE_GETUTXUSER
    }
  free (utmpbuf);
#else /* HAVE_GETUTXUSER */
  endutxent ();
#endif

  return status;
}
