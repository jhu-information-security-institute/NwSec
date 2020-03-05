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

#include <config.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#ifdef HAVE_TCPD_H
# include <tcpd.h>
#endif

#include <libinetutils.h>
#include "unused-parameter.h"

int usefamily = AF_UNSPEC;	/* Address family for daemon.  */

static void reapchild (int);

#ifndef DEFPORT
# ifdef IPPORT_FTP
#  define DEFPORT IPPORT_FTP
# else /* !IPPORT_FTP */
#  define DEFPORT 21
# endif
#endif /* !DEFPORT */

#ifdef WITH_WRAP

# if !HAVE_DECL_HOSTS_CTL
extern int hosts_ctl (char *, char *, char *, char *);
# endif

int allow_severity = LOG_INFO;
int deny_severity = LOG_NOTICE;

static int
check_host (struct sockaddr *sa, socklen_t len)
{
  int err;
  char addr[INET6_ADDRSTRLEN];
  char name[NI_MAXHOST];

  if (sa->sa_family != AF_INET
      && sa->sa_family != AF_INET6)
    return 1;

  (void) getnameinfo(sa, len, addr, sizeof (addr), NULL, 0, NI_NUMERICHOST);
  err = getnameinfo(sa, len, name, sizeof (name), NULL, 0, NI_NAMEREQD);
  if (!err)
    {
      if (!hosts_ctl ("ftpd", name, addr, STRING_UNKNOWN))
	{
	  syslog (deny_severity, "tcpwrappers rejected: %s [%s]",
		  name, addr);
	  return 0;
	}
    }
  else
    {
      if (!hosts_ctl ("ftpd", STRING_UNKNOWN, addr, STRING_UNKNOWN))
	{
	  syslog (deny_severity, "tcpwrappers rejected: [%s]", addr);
	  return 0;
	}
    }
  return (1);
}
#endif /* WITH_WRAP */

static void
reapchild (int signo _GL_UNUSED_PARAMETER)
{
  int save_errno = errno;

  while (waitpid (-1, NULL, WNOHANG) > 0)
    ;
  errno = save_errno;
}

/* The parameter '*phis_addrlen' must be initiated
   with the space available at calling time.
   The size of used space will then be returned.
 */
int
server_mode (const char *pidfile, struct sockaddr *phis_addr,
	     socklen_t *phis_addrlen, char *argv[])
{
  int ctl_sock, fd;
  struct servent *sv;
  int port, err;
  char portstr[8];
  socklen_t saved_addrlen = *phis_addrlen;
  struct addrinfo hints, *res, *ai;

  /* Become a daemon.  */
  if (daemon (1, 1) < 0)
    {
      syslog (LOG_ERR, "failed to become a daemon");
      return -1;
    }
  signal (SIGCHLD, reapchild);

  /* Get port for ftp/tcp.  */
  sv = getservbyname ("ftp", "tcp");
  port = (sv == NULL) ? DEFPORT : ntohs (sv->s_port);
  snprintf (portstr, sizeof (portstr), "%u", port);

  memset (&hints, 0, sizeof (hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  /* If an undetermined address family is passed,
   * we build a dual stacked listener from an AF_INET6
   * socket by unsetting the sockopt IPV6_V6ONLY.
   * Otherwise the resolver often prefers AF_INET.
   */
  hints.ai_family = (usefamily != AF_UNSPEC) ? usefamily : AF_INET6;

  err = getaddrinfo (NULL, portstr, &hints, &res);
  if (err)
    {
      syslog (LOG_ERR, "control socket: %s", gai_strerror (err));
      return -1;
    }
  /* Attempt to open socket, bind and start listen.  */
  for (ai = res; ai; ai = ai->ai_next)
    {
      ctl_sock = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (ctl_sock < 0)
	continue;

      /* Enable local address reuse.  */
      {
	int on = 1;
	if (setsockopt (ctl_sock, SOL_SOCKET, SO_REUSEADDR,
			(char *) &on, sizeof (on)) < 0)
	  syslog (LOG_ERR, "control setsockopt: %m");
      }

      /* Upgrade to dual stacked socket.  */
      if (usefamily == AF_UNSPEC && ai->ai_family == AF_INET6)
	{
	  int off = 0;
	  if (setsockopt (ctl_sock, IPPROTO_IPV6, IPV6_V6ONLY,
			  (char *) &off, sizeof (off)) < 0)
	    syslog (LOG_DEBUG, "setsockopt bindv6only: %m");
	}

      if (bind (ctl_sock, ai->ai_addr, ai->ai_addrlen))
	{
	  close (ctl_sock);
	  ctl_sock = -1;
	  continue;
	}

      if (listen (ctl_sock, 32) < 0)
	{
	  close (ctl_sock);
	  ctl_sock = -1;
	  continue;
	}

      /* Accept the first choice!  */
      break;
    }

  if (res)
    freeaddrinfo (res);

  if (ai == NULL)
    {
      syslog (LOG_ERR, "control socket: %m");
      return -1;
    }

  /* Stash pid in pidfile.  */
  {
    FILE *pid_fp = fopen (pidfile, "w");
    if (pid_fp == NULL)
      syslog (LOG_ERR, "can't open %s: %m", PATH_FTPDPID);
    else
      {
	fprintf (pid_fp, "%d\n", (int) getpid ());
	fchmod (fileno (pid_fp), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	fclose (pid_fp);
      }
  }

  /* Loop forever accepting connection requests and forking off
     children to handle them.  */
  while (1)
    {
      *phis_addrlen = saved_addrlen;
      fd = accept (ctl_sock, phis_addr, phis_addrlen);
      if (fork () == 0)		/* child */
	{
	  dup2 (fd, 0);
	  dup2 (fd, 1);
	  close (ctl_sock);
	  break;
	}
      close (fd);
    }

#ifdef WITH_WRAP
  /* In the child.  */
  if (!check_host (phis_addr, *phis_addrlen))
    return -1;
#endif

#ifndef HAVE_FORK
  _exit (execvp (argv[0], argv));
#else
  (void) argv;		/* Silence warnings.  */
#endif

  return fd;
}
