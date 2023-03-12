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

#if defined KRB5 || defined SHISHI

# include <sys/param.h>
# include <sys/file.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <fcntl.h>

# include <netinet/in.h>
# include <arpa/inet.h>

# if defined KRB5
#  ifdef HAVE_KRB5_H
#   include <krb5.h>
#  endif
#  include "kerberos5_def.h"
# elif defined(SHISHI) /* !KRB5 */
#  include <shishi.h>
#  include "shishi_def.h"
# endif /* SHISHI && !KRB5 */

# include <ctype.h>
# include <errno.h>
# include <netdb.h>
# include <pwd.h>
# include <signal.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>

# ifndef MAXHOSTNAMELEN
#  define MAXHOSTNAMELEN 64
# endif

# define START_PORT	5120	/* arbitrary */

int getport (int *, int);

# if defined KRB5
int
kcmd (krb5_context *ctx, int *sock, char **ahost, unsigned short rport,
      char *locuser, char **remuser, char *cmd, int *fd2p,
      char *service, const char *realm, krb5_keyblock **key,
      struct sockaddr_in *laddr, struct sockaddr_in *faddr,
      long authopts)
# elif defined(SHISHI) /* !KRB5 */
int
kcmd (Shishi ** h, int *sock, char **ahost, unsigned short rport,
      char *locuser, char **remuser, char *cmd, int *fd2p,
      char *service, const char *realm, Shishi_key ** key,
      struct sockaddr_storage *laddr, struct sockaddr_storage *faddr,
      long authopts, int af)
# endif /* SHISHI && !KRB5 */
{
  int s, timo = 1, pid;
# ifdef HAVE_SIGACTION
  sigset_t sigs, osigs;
# else
  long oldmask;
# endif /* !HAVE_SIGACTION */
  struct sockaddr_storage sin, from;
  socklen_t len;
  char c;

# ifdef ATHENA_COMPAT
  int lport = IPPORT_RESERVED - 1;
# else
  int lport = START_PORT;
# endif
# if HAVE_DECL_GETADDRINFO
  struct addrinfo hints, *ai, *res;
  char portstr[8], fqdn[NI_MAXHOST];
# else /* !HAVE_DECL_GETADDRINFO */
  struct hostent *hp;
# endif
  int rc;
  char *host_save, *host;
  int status;

  pid = getpid ();

  /* Extract Kerberos instance name.  */
  host = strchr (*ahost, '/');
  if (host)
    ++host;
  else
    host = *ahost;

# if HAVE_DECL_GETADDRINFO
  memset (&hints, 0, sizeof (hints));
#  ifdef KRB5
  hints.ai_family = AF_INET;
#  else /* SHISHI && !KRB5 */
  hints.ai_family = af;
#  endif
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;
  snprintf (portstr, sizeof (portstr), "%hu", ntohs (rport));

  rc = getaddrinfo (host, portstr, &hints, &res);
  if (rc)
    {
      fprintf (stderr, "kcmd: host %s: %s\n", host, gai_strerror (rc));
      return (-1);
    }

  ai = res;

  /* Attempt back resolving into the official host name.  */
  rc = getnameinfo (ai->ai_addr, ai->ai_addrlen, fqdn, sizeof (fqdn),
		    NULL, 0, NI_NAMEREQD);
  if (!rc)
    {
      host_save = malloc (strlen (fqdn) + 1);
      if (host_save == NULL)
	return (-1);
      strcpy (host_save, fqdn);
    }
  else
    {
      host_save = malloc (strlen (ai->ai_canonname) + 1);
      if (host_save == NULL)
	return (-1);
      strcpy (host_save, ai->ai_canonname);
    }

# else /* !HAVE_DECL_GETADDRINFO */
  /* Often the following rejects non-IPv4.
   * This is dependent on system implementation.  */
  hp = gethostbyname (host);
  if (hp == NULL)
    {
      /* fprintf(stderr, "%s: unknown host\n", host); */
      return (-1);
    }

  host_save = malloc (strlen (hp->h_name) + 1);
  if (host_save == NULL)
    return -1;
  strcpy (host_save, hp->h_name);
# endif /* !HAVE_DECL_GETADDRINFO */

  if (host == *ahost)
    *ahost = host_save;		/* Simple host name string.  */
  else
    {
      /* Server name `*ahost' is a Kerberized name.  */
      char *p;

      p = malloc ((host - *ahost) + strlen (host_save) + 1);
      if (p == NULL)
	return (-1);

      /* Extract prefix from `*ahost', excluding slash,
       * and concatenate the host's canonical name, but
       * preceeded by a slash.
       */
      sprintf (p, "%.*s/%s", (int) (host - *ahost - 1), *ahost, host_save);
      *ahost = p;
    }

# ifdef HAVE_SIGACTION
  sigemptyset (&sigs);
  sigaddset (&sigs, SIGURG);
  sigprocmask (SIG_BLOCK, &sigs, &osigs);
# else
  oldmask = sigblock (sigmask (SIGURG));
# endif /* !HAVE_SIGACTION */
  for (;;)
    {
# if HAVE_DECL_GETADDRINFO
      s = getport (&lport, ai->ai_family);
# else /* !HAVE_DECL_GETADDRINFO */
      s = getport (&lport, hp->h_addrtype);
# endif
      if (s < 0)
	{
	  if (errno == EAGAIN)
	    fprintf (stderr, "kcmd(socket): All ports in use\n");
	  else
	    perror ("kcmd: socket");
# if HAVE_SIGACTION
	  sigprocmask (SIG_SETMASK, &osigs, NULL);
# else
	  sigsetmask (oldmask);
# endif /* !HAVE_SIGACTION */
	  return (-1);
	}
      fcntl (s, F_SETOWN, pid);

# if HAVE_DECL_GETADDRINFO
      len = ai->ai_addrlen;
      memcpy (&sin, ai->ai_addr, ai->ai_addrlen);
# else /* !HAVE_DECL_GETADDRINFO */
      sin.ss_family = hp->h_addrtype;
      switch (hp->h_addrtype)
	{
	case AF_INET6:
	  len = sizeof (struct sockaddr_in6);
#  ifdef HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN
	  sin.ss_len = len;
#  endif
	  memcpy (&((struct sockaddr_in6 *) &sin)->sin6_addr,
		  hp->h_addr, hp->h_length);
	  ((struct sockaddr_in6 *) &sin)->sin6_port = rport;
	  break;
	case AF_INET:
	default:
	  len = sizeof (struct sockaddr_in);
#  ifdef HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN
	  sin.ss_len = len;
#  endif
	  memcpy (&((struct sockaddr_in *) &sin)->sin_addr,
		  hp->h_addr, hp->h_length);
	  ((struct sockaddr_in *) &sin)->sin_port = rport;
	}
# endif /* !HAVE_DECL_GETADDRINFO */

      if (connect (s, (struct sockaddr *) &sin, len) >= 0)
	break;
      close (s);
      if (errno == EADDRINUSE)
	{
	  lport--;
	  continue;
	}
      /*
       * don't wait very long for Kerberos rcmd.
       */
      if (errno == ECONNREFUSED && timo <= 4)
	{
	  /* sleep(timo); don't wait at all here */
	  timo *= 2;
	  continue;
	}
# if ! defined ultrix || defined sun
#  if HAVE_DECL_GETADDRINFO
      if (ai->ai_next)
#  else /* !HAVE_DECL_GETADDRINFO */
      if (hp->h_addr_list[1] != NULL)
#  endif
	{
	  int oerrno = errno;
	  char addrstr[INET6_ADDRSTRLEN];

#  if HAVE_DECL_GETADDRINFO
	  getnameinfo (ai->ai_addr, ai->ai_addrlen,
		       addrstr, sizeof (addrstr), NULL, 0,
		       NI_NUMERICHOST);
	  fprintf (stderr, "kcmd: connect to address %s: ", addrstr);
	  errno = oerrno;
	  perror (NULL);
	  ai = ai->ai_next;
	  getnameinfo (ai->ai_addr, ai->ai_addrlen,
		       addrstr, sizeof (addrstr), NULL, 0,
		       NI_NUMERICHOST);
	  fprintf (stderr, "Trying %s...\n", addrstr);
#  else /* !HAVE_DECL_GETADDRINFO */
	  fprintf (stderr, "kcmd: connect to address %s: ",
		   inet_ntop (hp->h_addrtype, hp->h_addr_list[0],
			      addrstr, sizeof (addrstr)));
	  errno = oerrno;
	  perror (NULL);
	  hp->h_addr_list++;
	  fprintf (stderr, "Trying %s...\n",
		   inet_ntop (hp->h_addrtype, hp->h_addr_list[0],
			      addrstr, sizeof (addrstr)));
#  endif /* !HAVE_DECL_GETADDRINFO */
	  continue;
	}
# endif	/* !(defined(ultrix) || defined(sun)) */
# if HAVE_DECL_GETADDRINFO
      if (errno != ECONNREFUSED)
	perror (res->ai_canonname);

      if (res)
	freeaddrinfo (res);
# else /* !HAVE_DECL_GETADDRINFO */
      if (errno != ECONNREFUSED)
	perror (hp->h_name);
# endif

# if HAVE_SIGACTION
      sigprocmask (SIG_SETMASK, &osigs, NULL);
# else
      sigsetmask (oldmask);
# endif /* !HAVE_SIGACTION */

      return (-1);
    }
# if HAVE_DECL_GETADDRINFO
  if (res)
    freeaddrinfo (res);
# endif

  lport--;
  if (fd2p == 0)
    {
      write (s, "", 1);
      lport = 0;
    }
  else
    {
      char num[8];
      int port, s2, s3;

      s2 = getport (&lport, sin.ss_family);
      len = sizeof (from);

      if (s2 < 0)
	{
	  status = -1;
	  goto bad;
	}
      listen (s2, 1);
      sprintf (num, "%d", lport);
      if (write (s, num, strlen (num) + 1) != (ssize_t) strlen (num) + 1)
	{
	  perror ("kcmd(write): setting up stderr");
	  close (s2);
	  status = -1;
	  goto bad;
	}
      s3 = accept (s2, (struct sockaddr *) &from, &len);
      close (s2);
      if (s3 < 0)
	{
	  perror ("kcmd:accept");
	  lport = 0;
	  status = -1;
	  goto bad;
	}
      *fd2p = s3;
      port = (from.ss_family == AF_INET6)
	     ? ntohs (((struct sockaddr_in6 *) &from)->sin6_port)
	     : ntohs (((struct sockaddr_in *) &from)->sin_port);

      if (port >= IPPORT_RESERVED
          || (from.ss_family != AF_INET && from.ss_family != AF_INET6))
	{
	  fprintf (stderr,
		   "kcmd(socket): protocol failure in circuit setup.\n");
	  status = -1;
	  goto bad2;
	}
    }
  /*
   * Kerberos-authenticated service.  Don't have to send locuser,
   * since its already in the ticket, and we'll extract it on
   * the other side.
   */
  /* write(s, locuser, strlen(locuser)+1); */

  /* set up the needed stuff for mutual auth, but only if necessary */
# ifdef KRB5
  if (authopts & AP_OPTS_MUTUAL_REQUIRED)
# elif defined(SHISHI) /* !KRB5 */
  if (authopts & SHISHI_APOPTIONS_MUTUAL_REQUIRED)
# endif
    {
      socklen_t sin_len;

      memcpy (faddr, &sin, sizeof(*faddr));

      sin_len = sizeof (*laddr);
      if (getsockname (s, (struct sockaddr *) laddr, &sin_len) < 0)
	{
	  perror ("kcmd(getsockname)");
	  status = -1;
	  goto bad2;
	}
    }

  (void) service;	/* Silence warning.  XXX: Implicit use?  */

# ifdef KRB5
  status = kerberos_auth (ctx, 0, remuser, *ahost, s,
			  cmd, rport, key, realm);
  if (status != 0)
    goto bad2;

# elif defined(SHISHI) /* !KRB5 */
  status = shishi_auth (h, 0, remuser, *ahost, s,
			cmd, rport, key, realm);
  if (status != SHISHI_OK)
    goto bad2;

# endif /* SHISHI && !KRB5 */

  write (s, *remuser, strlen (*remuser) + 1);

  write (s, cmd, strlen (cmd) + 1);

  if (locuser && locuser[0])
    write (s, locuser, strlen (locuser) + 1);
  else
    write (s, *remuser, strlen (*remuser) + 1);

  {
    int zero = 0;	/* No forwarding of credentials.  */

    write (s, &zero, sizeof (zero));
  }

  rc = read (s, &c, sizeof (c));
  if (rc != sizeof (c))
    {
      if (rc == -1)
	perror (*ahost);
      else
	fprintf (stderr, "kcmd: bad connection with remote host\n");
      status = -1;
      goto bad2;
    }
  if (c != '\0')
    {
      while (read (s, &c, 1) == 1)
	{
	  write (2, &c, 1);
	  if (c == '\n')
	    break;
	}
      status = -1;
      errno = ENOENT;
      goto bad2;
    }
# if HAVE_SIGACTION
  sigprocmask (SIG_SETMASK, &osigs, NULL);
# else
  sigsetmask (oldmask);
# endif /* !HAVE_SIGACTION */
  *sock = s;
# if defined KRB5
  return (0);
# elif defined(SHISHI) /* !KRB5 */
  return (SHISHI_OK);
# endif /* SHISHI && !KRB5 */
bad2:
  if (lport)
    close (*fd2p);
bad:
  close (s);
# if HAVE_SIGACTION
  sigprocmask (SIG_SETMASK, &osigs, NULL);
# else
  sigsetmask (oldmask);
# endif /* !HAVE_SIGACTION */
  return (status);
}

int
getport (int *alport, int af)
{
  struct sockaddr_storage sin;
  socklen_t len;
  int s;

  memset (&sin, 0, sizeof (sin));
  sin.ss_family = af;
  len = (af == AF_INET6) ? sizeof (struct sockaddr_in6)
	: sizeof (struct sockaddr_in);
# ifdef HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN
  sin.ss_len = len;
# endif

  s = socket (sin.ss_family, SOCK_STREAM, 0);
  if (s < 0)
    return (-1);
  for (;;)
    {
      switch (af)
	{
	case AF_INET6:
	  ((struct sockaddr_in6 *) &sin)->sin6_port =
		htons ((unsigned short) * alport);
	  break;
	case AF_INET:
	default:
	  ((struct sockaddr_in *) &sin)->sin_port =
		htons ((unsigned short) * alport);
	}

      if (bind (s, (struct sockaddr *) &sin, len) >= 0)
	return (s);
      if (errno != EADDRINUSE)
	{
	  close (s);
	  return (-1);
	}
      (*alport)--;
# ifdef ATHENA_COMPAT
      if (*alport == IPPORT_RESERVED / 2)
	{
# else
      if (*alport == IPPORT_RESERVED)
	{
# endif
	  close (s);
	  errno = EAGAIN;	/* close */
	  return (-1);
	}
    }
}

#endif /* KRB5 || SHISHI */
