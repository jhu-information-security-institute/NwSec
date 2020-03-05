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
 * Copyright (c) 1985, 1989, 1993, 1994
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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <sys/file.h>

#include <netinet/in.h>
#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif
#include <arpa/inet.h>
#include <arpa/ftp.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>	/* intmax_t */
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/select.h>

#ifdef HAVE_IDNA_H
# include <idna.h>
#endif

#include "ftp_var.h"
#include "unused-parameter.h"

#if !HAVE_DECL_FCLOSE
/* Some systems don't declare fclose in <stdio.h>, so do it ourselves.  */
extern int fclose (FILE *);
#endif

#if !HAVE_DECL_PCLOSE
/* Some systems don't declare pclose in <stdio.h>, so do it ourselves.  */
extern int pclose (FILE *);
#endif

extern int h_errno;

int data = -1;
int abrtflag = 0;
jmp_buf ptabort;
int ptabflg;
int ptflag = 0;
off_t restart_point = 0;

struct sockaddr_storage myctladdr;
struct sockaddr_storage hisctladdr;
struct sockaddr_storage data_addr;
socklen_t ctladdrlen;	/* Applies to all addresses.  */

/* For temporary resolving: hookup() and initconn()/noport.  */
static char ia[INET6_ADDRSTRLEN];
static char portstr[10];

FILE *cin, *cout;

#if ! defined FTP_CONNECT_TIMEOUT || FTP_CONNECT_TIMEOUT < 1
# define FTP_CONNECT_TIMEOUT 5
#endif

char *
hookup (char *host, int port)
{
  struct addrinfo hints, *ai = NULL, *res = NULL;
  struct timeval timeout;
  int status, again = 0;
  int s, tos;
  socklen_t len;
  static char hostnamebuf[80];
  char *rhost;

#ifdef HAVE_IDN
  status = idna_to_ascii_lz (host, &rhost, 0);
  if (status)
    {
      error (0, 0, "%s: %s", host, idna_strerror (status));
      code = -1;
      return ((char *) 0);
    }
#else /* !HAVE_IDN */
  rhost = strdup (host);
#endif

  snprintf (portstr, sizeof (portstr) - 1, "%u", port);
  memset (&hisctladdr, 0, sizeof (hisctladdr));
  memset (&hints, 0, sizeof (hints));

  hints.ai_family = usefamily;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;
#ifdef AI_IDN
  hints.ai_flags |= AI_IDN;
#endif
#ifdef AI_CANONIDN
  hints.ai_flags |= AI_CANONIDN;
#endif

  status = getaddrinfo (rhost, portstr, &hints, &res);
  if (status)
    {
      error (0, 0, "%s: %s", rhost, gai_strerror (status));
      code = -1;
      free (rhost);
      return ((char *) 0);
    }

  if (res->ai_canonname)
    strncpy (hostnamebuf, res->ai_canonname, sizeof (hostnamebuf));
  else
    strncpy (hostnamebuf, rhost, sizeof (hostnamebuf));

  hostname = hostnamebuf;
  free (rhost);

  for (ai = res; ai != NULL; ai = ai->ai_next, ++again)
    {
      if (again)
        {
	  getnameinfo (ai->ai_addr, ai->ai_addrlen, ia, sizeof (ia),
			NULL, 0, NI_NUMERICHOST);
	  error (0, 0, "Trying %s ...", ia);
	}

      s = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (s < 0)
	continue;

      timeout.tv_sec = FTP_CONNECT_TIMEOUT;
      timeout.tv_usec = 0;
      if (setsockopt (s, SOL_SOCKET, SO_SNDTIMEO, &timeout,
			sizeof (timeout)) < 0 && debug)
	error (0, errno, "setsockopt (SO_SNDTIMEO)");

      if (connect (s, ai->ai_addr, ai->ai_addrlen) < 0)
	{
	  int oerrno = (errno != EINPROGRESS) ? errno : ETIMEDOUT;

	  getnameinfo (ai->ai_addr, ai->ai_addrlen, ia, sizeof (ia),
			NULL, 0, NI_NUMERICHOST);
	  error (0, oerrno, "connect to address %s", ia);
	  close (s);
	  s = -1;
	  continue;
	}

      /* A standing connection is found: register the address.  */
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;
      (void) setsockopt (s, SOL_SOCKET, SO_SNDTIMEO, &timeout,
			  sizeof (timeout));

      ctladdrlen = ai->ai_addrlen;
      memmove ((caddr_t) &hisctladdr, ai->ai_addr, ai->ai_addrlen);
      break;
    } /* for (ai = ai->ai_next) */

  if (res)
    freeaddrinfo (res);

  if (ai == NULL)
    {
      error (0, 0, "no response from host");
      code = -1;
      goto bad;
    }

  len = sizeof (myctladdr);
  if (getsockname (s, (struct sockaddr *) &myctladdr, &len) < 0)
    {
      error (0, errno, "getsockname");
      code = -1;
      goto bad;
    }

#if defined IP_TOS && defined IPPROTO_IP && defined IPTOS_LOWDELAY
  tos = IPTOS_LOWDELAY;
  if (myctladdr.ss_family == AF_INET &&
	setsockopt (s, IPPROTO_IP, IP_TOS, (char *) &tos, sizeof (int)) < 0)
    error (0, errno, "setsockopt TOS (ignored)");
#endif

  cin = fdopen (s, "r");
  /* dup(s) is for sake of stdio implementations who do not
     allow two fdopen's on the same file-descriptor */
  cout = fdopen (dup (s), "w");
  if (cin == NULL || cout == NULL)
    {
      error (0, 0, "fdopen failed.");
      if (cin)
	fclose (cin);
      if (cout)
	fclose (cout);
      code = -1;
      goto bad;
    }
  if (verbose)
    printf ("Connected to %s.\n", hostname);
  if (getreply (0) > 2)
    {				/* read startup message from server */
      if (cin)
	fclose (cin);
      if (cout)
	fclose (cout);
      code = -1;
      goto bad;
    }

#ifdef SO_OOBINLINE
  {
    int on = 1;

    if (setsockopt (s, SOL_SOCKET, SO_OOBINLINE, (char *) &on, sizeof (on))
	< 0 && debug)
      {
	error (0, errno, "setsockopt");
      }
  }
#endif /* SO_OOBINLINE */

  return (hostname);
bad:
  close (s);
  return ((char *) 0);
}

int
login (char *host)
{
  char tmp[80];
  char *user, *pass, *acct;
  int n, aflag = 0;

  user = pass = acct = 0;
  if (remote_userpass (host, &user, &pass, &acct) < 0)
    {
      code = -1;
      return (0);
    }
  while (user == NULL)
    {
      char *myname = getlogin ();

      if (myname == NULL)
	{
	  struct passwd *pp = getpwuid (getuid ());

	  if (pp != NULL)
	    myname = pp->pw_name;
	}
      if (myname)
	printf ("Name (%s:%s): ", host, myname);
      else
	printf ("Name (%s): ", host);
      if (fgets (tmp, sizeof (tmp) - 1, stdin))
	{
	  /* If the user presses return immediately, we get "\n".
	   * In all other cases, the assignment is a no-op,
	   * and is always well defined thanks to fgets().
	   */
	  tmp[strlen (tmp) - 1] = '\0';
	}
      else
	*tmp = '\0';		/* Ctrl-D received.  */
      if (*tmp == '\0')
	user = myname;
      else
	user = tmp;
    }
  n = command ("USER %s", user);
  if (n == CONTINUE)
    {
      /* Is this a case of challenge-response?
       * RFC 2228 stipulates code 336 for this.
       * Suppress the message in verbose mode,
       * since it has already been displayed.
       */
      if (code == 336 && !verbose)
	printf ("%s\n", reply_string + strlen ("336 "));
      /* In addition, any password given on the
       * command line is irrelevant, so ignore it.
       */
      if (pass == NULL || code == 336)
	pass = getpass ("Password: ");
      n = command ("PASS %s", pass);
      if (pass)
	memset (pass, 0, strlen (pass));
    }
  if (n == CONTINUE)
    {
      aflag++;
      acct = getpass ("Account: ");
      n = command ("ACCT %s", acct);
      if (acct)
	memset (acct, 0, strlen (acct));
    }
  if (n != COMPLETE)
    {
      error (0, 0, "Login failed.");
      return (0);
    }
  if (!aflag && acct != NULL)
    {
      command ("ACCT %s", acct);
      memset (acct, 0, strlen (acct));
    }
  if (proxy)
    return (1);
  for (n = 0; n < macnum; ++n)
    {
      if (!strcmp ("init", macros[n].mac_name))
	{
	  free (line);
	  line = calloc (MAXLINE, sizeof (*line));
	  linelen = MAXLINE;
	  if (!line)
	    quit (0, 0);

	  /* Simulate input of the macro 'init'.  */
	  strcpy (line, "$init");
	  makeargv ();
	  domacro (margc, margv);
	  break;
	}
    }
  return (1);
}

void
cmdabort (int sig _GL_UNUSED_PARAMETER)
{

  printf ("\n");
  fflush (stdout);
  abrtflag++;
  if (ptflag)
    longjmp (ptabort, 1);
}

int
command (const char *fmt, ...)
{
  va_list ap;
  int r;
  sighandler_t oldintr;

  abrtflag = 0;
  if (debug)
    {
      printf ("---> ");
      va_start (ap, fmt);
      if (strncmp ("PASS ", fmt, 5) == 0)
	printf ("PASS XXXX");
      else if (strncmp ("ACCT ", fmt, 5) == 0)
	printf ("ACCT XXXX");
      else
	vfprintf (stdout, fmt, ap);
      va_end (ap);
      printf ("\n");
      fflush (stdout);
    }
  if (cout == NULL)
    {
      error (0, 0, "No control connection for command");
      code = -1;
      return (0);
    }
  oldintr = signal (SIGINT, cmdabort);
  /* Under weird circumstances, we get a SIGPIPE from fflush().  */
  signal (SIGPIPE, SIG_IGN);
  va_start (ap, fmt);
  vfprintf (cout, fmt, ap);
  va_end (ap);
  fprintf (cout, "\r\n");
  fflush (cout);
  cpend = 1;
  r = getreply (!strcmp (fmt, "QUIT"));
  if (abrtflag && oldintr != SIG_IGN)
    (*oldintr) (SIGINT);
  signal (SIGINT, oldintr);
  signal (SIGPIPE, SIG_DFL);
  return (r);
}

char reply_string[BUFSIZ];	/* last line of previous reply */

int
getreply (int expecteof)
{
  int c, n;
  int dig;
  int originalcode = 0, continuation = 0;
  sighandler_t oldintr;
  int pflag = 0;
  char *cp, *pt = pasv;

  oldintr = signal (SIGINT, cmdabort);
  for (;;)
    {
      dig = n = code = 0;
      cp = reply_string;
      while ((c = getc (cin)) != '\n')
	{
	  if (c == IAC)
	    {			/* handle telnet commands */
	      switch (c = getc (cin))
		{
		case WILL:
		case WONT:
		  c = getc (cin);
		  fprintf (cout, "%c%c%c", IAC, DONT, c);
		  fflush (cout);
		  break;
		case DO:
		case DONT:
		  c = getc (cin);
		  fprintf (cout, "%c%c%c", IAC, WONT, c);
		  fflush (cout);
		  break;
		default:
		  break;
		}
	      continue;
	    }
	  dig++;
	  if (c == EOF)
	    {
	      if (expecteof)
		{
		  signal (SIGINT, oldintr);
		  code = 221;
		  return (0);
		}
	      lostpeer (0);
	      if (verbose)
		{
		  printf
		    ("421 Service not available, remote server has closed connection\n");
		  fflush (stdout);
		}
	      code = 421;
	      return (TRANSIENT);
	    }
	  if (c != '\r' && (verbose > 0 ||
			    (verbose > -1 && n == ERROR && dig > 4)))
	    {
	      if (proxflag && (dig == 1 || (dig == 5 && verbose == 0)))
		printf ("%s:", hostname);
	      putchar (c);
	    }
	  if (dig < 4 && isdigit (c))
	    code = code * 10 + (c - '0');
	  if (!pflag && (code == 227 || code == 228 || code == 229)) /* PASV || LPSV || EPSV */
	    pflag = 1;
	  if (dig > 4 && pflag == 1 && isdigit (c))
	    pflag = 2;
	  if (pflag == 2)
	    {
	      if (c != '\r' && c != ')')
		*pt++ = c;
	      else
		{
		  *pt = '\0';
		  pflag = 3;
		}
	    }
	  if (dig == 4 && c == '-')
	    {
	      if (continuation)
		code = 0;
	      continuation++;
	    }
	  if (n == 0)
	    n = c - '0';	/* Extract ARPA's reply code.  */
	  if (cp < &reply_string[sizeof (reply_string) - 1])
	    *cp++ = c;
	}
      if (verbose > 0 || (verbose > -1 && n == ERROR))
	{
	  putchar (c);
	  fflush (stdout);
	}
      if (continuation && code != originalcode)
	{
	  if (originalcode == 0)
	    originalcode = code;
	  continue;
	}
      *cp = '\0';
      if (n != PRELIM)
	cpend = 0;
      signal (SIGINT, oldintr);
      if (code == 421 || originalcode == 421)
	lostpeer (0);
      if (abrtflag && oldintr != cmdabort && oldintr != SIG_IGN)
	(*oldintr) (SIGINT);
      return n;
    }
}

int
empty (fd_set *mask, int sec)
{
  struct timeval t;

  t.tv_sec = (long) sec;
  t.tv_usec = 0;
  return (select (32, mask, (fd_set *) 0, (fd_set *) 0, &t));
}

jmp_buf sendabort;

void
abortsend (int sig _GL_UNUSED_PARAMETER)
{

  mflag = 0;
  abrtflag = 0;
  printf ("\nsend aborted\nwaiting for remote to finish abort\n");
  fflush (stdout);
  longjmp (sendabort, 1);
}

void
sendrequest (char *cmd, char *local, char *remote, int printnames)
{
  struct stat st;
  struct timeval start, stop;
  int c, d;
  FILE *fin, *dout = 0, *popen (const char *, const char *);
  int (*closefunc) (FILE *);
  sighandler_t oldintr, oldintp;
  long long bytes = 0, local_hashbytes = hashbytes;
  char *lmode, *bufp;
  int blksize = BUFSIZ;
  static int bufsize = 0;
  static char *buf;

  if (verbose && printnames)
    {
      if (local && *local != '-')
	printf ("local: %s ", local);
      if (remote)
	printf ("remote: %s\n", remote);
    }
  if (proxy)
    {
      proxtrans (cmd, local, remote);
      return;
    }
  if (curtype != type)
    changetype (type, 0);
  closefunc = NULL;
  oldintr = NULL;
  oldintp = NULL;
  lmode = "w";
  if (setjmp (sendabort))
    {
      while (cpend)
	{
	  getreply (0);
	}
      if (data >= 0)
	{
	  close (data);
	  data = -1;
	}
      if (oldintr)
	signal (SIGINT, oldintr);
      if (oldintp)
	signal (SIGPIPE, oldintp);
      code = -1;
      return;
    }
  oldintr = signal (SIGINT, abortsend);
  if (strcmp (local, "-") == 0)
    fin = stdin;
  else if (*local == '|')
    {
      oldintp = signal (SIGPIPE, SIG_IGN);
      fin = popen (local + 1, "r");
      if (fin == NULL)
	{
	  error (0, errno, "%s", local + 1);
	  signal (SIGINT, oldintr);
	  signal (SIGPIPE, oldintp);
	  code = -1;
	  return;
	}
      closefunc = pclose;
    }
  else
    {
      fin = fopen (local, "r");
      if (fin == NULL)
	{
	  error (0, errno, "local: %s", local);
	  signal (SIGINT, oldintr);
	  code = -1;
	  return;
	}
      closefunc = fclose;
      if (fstat (fileno (fin), &st) < 0 || (st.st_mode & S_IFMT) != S_IFREG)
	{
	  fprintf (stdout, "%s: not a plain file.\n", local);
	  signal (SIGINT, oldintr);
	  fclose (fin);
	  code = -1;
	  return;
	}
	blksize = st.st_blksize;
    }
  if (initconn ())
    {
      signal (SIGINT, oldintr);
      if (oldintp)
	signal (SIGPIPE, oldintp);
      code = -1;
      if (closefunc != NULL)
	(*closefunc) (fin);
      return;
    }
  if (setjmp (sendabort))
    goto abort;

  if (restart_point &&
      (strcmp (cmd, "STOR") == 0 || strcmp (cmd, "APPE") == 0))
    {
      off_t rc;

      switch (curtype)
	{
	case TYPE_A:
	  rc = fseeko (fin, restart_point, SEEK_SET);
	  break;
	case TYPE_I:
	case TYPE_L:
	  rc = lseek (fileno (fin), restart_point, SEEK_SET);
	  break;
	}
      if (rc < 0)
	{
	  (void) command ("ABOR");
	  getreply (0);
	  error (0, errno, "local: %s", local);
	  restart_point = 0;
	  if (closefunc != NULL)
	    (*closefunc) (fin);
	  return;
	}
      if (command ("REST %jd", (intmax_t) restart_point) != CONTINUE)
	{
	  restart_point = 0;
	  if (closefunc != NULL)
	    (*closefunc) (fin);
	  return;
	}
      restart_point = 0;
      lmode = "r+w";
    }
  if (remote)
    {
      if (command ("%s %s", cmd, remote) != PRELIM)
	{
	  signal (SIGINT, oldintr);
	  if (oldintp)
	    signal (SIGPIPE, oldintp);
	  if (closefunc != NULL)
	    (*closefunc) (fin);
	  return;
	}
    }
  else if (command ("%s", cmd) != PRELIM)
    {
      signal (SIGINT, oldintr);
      if (oldintp)
	signal (SIGPIPE, oldintp);
      if (closefunc != NULL)
	(*closefunc) (fin);
      return;
    }
  dout = dataconn (lmode);
  if (dout == NULL)
    goto abort;

  if (blksize > bufsize)
    {
      free (buf);
      buf = malloc ((unsigned) blksize);
      if (buf == NULL)
	{
	  error (0, errno, "malloc");
	  bufsize = 0;
	  goto abort;
	}
      bufsize = blksize;
    }

  gettimeofday (&start, (struct timezone *) 0);
  oldintp = signal (SIGPIPE, SIG_IGN);
  switch (curtype)
    {

    case TYPE_I:
    case TYPE_L:
      errno = d = 0;
      while ((c = read (fileno (fin), buf, bufsize)) > 0)
	{
	  bytes += c;
	  for (bufp = buf; c > 0; c -= d, bufp += d)
	    if ((d = write (fileno (dout), bufp, c)) <= 0)
	      break;
	  if (hash)
	    {
	      while (bytes >= local_hashbytes)
		{
		  putchar ('#');
		  local_hashbytes += hashbytes;
		}
	      fflush (stdout);
	    }
	}
      if (hash && bytes > 0)
	{
	  if (bytes < local_hashbytes)
	    putchar ('#');
	  putchar ('\n');
	  fflush (stdout);
	}
      if (c < 0)
	error (0, errno, "local: %s", local);
      if (d < 0)
	{
	  if (errno != EPIPE)
	    error (0, errno, "netout");
	  bytes = -1;
	}
      break;

    case TYPE_A:
      while ((c = getc (fin)) != EOF)
	{
	  if (c == '\n')
	    {
	      while (hash && (bytes >= local_hashbytes))
		{
		  putchar ('#');
		  fflush (stdout);
		  local_hashbytes += hashbytes;
		}
	      if (ferror (dout))
		break;
	      putc ('\r', dout);
	      bytes++;
	    }
	  putc (c, dout);
	  bytes++;
	  /*              if (c == '\r') {                                */
	  /*                      putc('\0', dout);  // this violates rfc */
	  /*                      bytes++;                                */
	  /*              }                                               */
	}
      if (hash)
	{
	  if (bytes < local_hashbytes)
	    putchar ('#');
	  putchar ('\n');
	  fflush (stdout);
	}
      if (ferror (fin))
	error (0, errno, "local: %s", local);
      if (ferror (dout))
	{
	  if (errno != EPIPE)
	    error (0, errno, "netout");
	  bytes = -1;
	}
      break;
    }
  if (closefunc != NULL)
    (*closefunc) (fin);
  fclose (dout);
  gettimeofday (&stop, (struct timezone *) 0);
  getreply (0);
  signal (SIGINT, oldintr);
  if (oldintp)
    signal (SIGPIPE, oldintp);
  if (bytes > 0)
    ptransfer ("sent", bytes, &start, &stop);
  return;
abort:
  signal (SIGINT, oldintr);
  if (oldintp)
    signal (SIGPIPE, oldintp);
  if (!cpend)
    {
      code = -1;
      return;
    }
  if (data >= 0)
    {
      close (data);
      data = -1;
    }
  if (dout)
    fclose (dout);
  getreply (0);
  code = -1;
  if (closefunc != NULL && fin != NULL)
    (*closefunc) (fin);
  gettimeofday (&stop, (struct timezone *) 0);
  if (bytes > 0)
    ptransfer ("sent", bytes, &start, &stop);
}

jmp_buf recvabort;

void
abortrecv (int sig _GL_UNUSED_PARAMETER)
{

  mflag = 0;
  abrtflag = 0;
  printf ("\nreceive aborted\nwaiting for remote to finish abort\n");
  fflush (stdout);
  longjmp (recvabort, 1);
}

void
recvrequest (char *cmd, char *local, char *remote, char *lmode, int printnames)
{
  FILE *fout, *din = 0;
  int (*closefunc) (FILE *);
  sighandler_t oldintr, oldintp;
  int c, d, is_retr, tcrflag, bare_lfs = 0;
  int blksize = BUFSIZ;
  static int bufsize = 0;
  static char *buf;
  long long bytes = 0, local_hashbytes = hashbytes;
  struct timeval start, stop;

  is_retr = strcmp (cmd, "RETR") == 0;
  if (is_retr && verbose && printnames)
    {
      if (local && *local != '-')
	printf ("local: %s ", local);
      if (remote)
	printf ("remote: %s\n", remote);
    }
  if (proxy && is_retr)
    {
      proxtrans (cmd, local, remote);
      return;
    }
  closefunc = NULL;
  oldintr = NULL;
  oldintp = NULL;
  tcrflag = !crflag && is_retr;
  if (setjmp (recvabort))
    {
      while (cpend)
	{
	  getreply (0);
	}
      if (data >= 0)
	{
	  close (data);
	  data = -1;
	}
      if (oldintr)
	signal (SIGINT, oldintr);
      code = -1;
      return;
    }
  oldintr = signal (SIGINT, abortrecv);
  if (strcmp (local, "-") && *local != '|')
    {
      if (runique && (local = gunique (local)) == NULL)
	{
	  signal (SIGINT, oldintr);
	  code = -1;
	  return;
	}
    }
  if (!is_retr)
    {
      if (curtype != TYPE_A)
	changetype (TYPE_A, 0);
    }
  else if (curtype != type)
    changetype (type, 0);
  if (initconn ())
    {
      signal (SIGINT, oldintr);
      code = -1;
      return;
    }
  if (setjmp (recvabort))
    goto abort;
  if (is_retr && restart_point &&
      command ("REST %jd", (intmax_t) restart_point) != CONTINUE)
    return;
  if (remote)
    {
      if (command ("%s %s", cmd, remote) != PRELIM)
	{
	  signal (SIGINT, oldintr);
	  return;
	}
    }
  else
    {
      if (command ("%s", cmd) != PRELIM)
	{
	  signal (SIGINT, oldintr);
	  return;
	}
    }
  din = dataconn ("r");
  if (din == NULL)
    goto abort;

  if (strcmp (local, "-") == 0)
    fout = stdout;
  else if (*local == '|')
    {
      oldintp = signal (SIGPIPE, SIG_IGN);
      fout = popen (local + 1, "w");
      if (fout == NULL)
	{
	  error (0, errno, "%s", local + 1);
	  goto abort;
	}
      closefunc = pclose;
    }
  else
    {
      struct stat st;

      fout = fopen (local, lmode);
      if (fout == NULL || fstat (fileno (fout), &st) < 0)
	{
	  error (0, errno, "local: %s", local);
	  goto abort;
	}
      closefunc = fclose;
      blksize = st.st_blksize;
    }

  if (blksize > bufsize)
    {
      free (buf);
      buf = malloc ((unsigned) blksize);
      if (buf == NULL)
	{
	  error (0, errno, "malloc");
	  bufsize = 0;
	  goto abort;
	}
      bufsize = blksize;
    }

  gettimeofday (&start, (struct timezone *) 0);
  switch (curtype)
    {

    case TYPE_I:
    case TYPE_L:
      if (restart_point && lseek (fileno (fout), restart_point, SEEK_SET) < 0)
	{
	  error (0, errno, "local: %s", local);
	  if (closefunc != NULL)
	    (*closefunc) (fout);
	  return;
	}
      errno = d = 0;
      while ((c = read (fileno (din), buf, bufsize)) > 0)
	{
	  if ((d = write (fileno (fout), buf, c)) != c)
	    break;
	  bytes += c;
	  if (hash)
	    {
	      while (bytes >= local_hashbytes)
		{
		  putchar ('#');
		  local_hashbytes += hashbytes;
		}
	      fflush (stdout);
	    }
	}

      if (hash && bytes > 0)
	{
	  if (bytes < local_hashbytes)
	    putchar ('#');
	  putchar ('\n');
	  fflush (stdout);
	}
      if (c < 0)
	{
	  if (errno != EPIPE)
	    error (0, errno, "netin");
	  bytes = -1;
	}
      if (d < c)
	{
	  if (d < 0)
	    error (0, errno, "local: %s", local);
	  else
	    error (0, 0, "%s: short write", local);
	}
      break;

    case TYPE_A:
      if (restart_point)
	{
	  off_t i, n;
	  int ch;

	  errno = 0;

	  if (fseeko (fout, 0L, SEEK_SET) < 0)
	    goto done;
	  n = restart_point;
	  for (i = 0; i++ < n;)
	    {
	      if ((ch = getc (fout)) == EOF)
		goto done;
	      if (ch == '\n')
		i++;
	    }
	  if (fseeko (fout, 0L, SEEK_CUR) < 0)
	    {
	    done:
	      /* Cancel server's action quickly.  */
	      (void) command ("ABOR");
	      getreply (0);

	      /* Explain our failure.  */
	      if (ch == EOF)
		printf ("Action not taken: offset %jd is outside of %s.\n",
		       restart_point, local);
	      else
		error (0, errno, "local: %s", local);

	      if (closefunc != NULL)
		(*closefunc) (fout);
	      return;
	    }
	}
      while ((c = getc (din)) != EOF)
	{
	  if (c == '\n')
	    bare_lfs++;
	  while (c == '\r')
	    {
	      while (hash && (bytes >= local_hashbytes))
		{
		  putchar ('#');
		  fflush (stdout);
		  local_hashbytes += hashbytes;
		}
	      bytes++;
	      if ((c = getc (din)) != '\n' || tcrflag)
		{
		  if (ferror (fout))
		    goto break2;
		  putc ('\r', fout);
		  if (c == '\0')
		    {
		      bytes++;
		      goto contin2;
		    }
		  if (c == EOF)
		    goto contin2;
		}
	    }
	  putc (c, fout);
	  bytes++;
	contin2:;
	}
    break2:
      if (bare_lfs)
	{
	  printf ("WARNING! %d bare linefeeds received in ASCII mode\n",
		  bare_lfs);
	  printf ("File may not have transferred correctly.\n");
	}
      if (hash)
	{
	  if (bytes < local_hashbytes)
	    putchar ('#');
	  putchar ('\n');
	  fflush (stdout);
	}
      if (ferror (din))
	{
	  if (errno != EPIPE)
	    error (0, errno, "netin");
	  bytes = -1;
	}
      if (ferror (fout))
	error (0, errno, "local: %s", local);
      break;
    }
  if (closefunc != NULL)
    (*closefunc) (fout);
  signal (SIGINT, oldintr);
  if (oldintp)
    signal (SIGPIPE, oldintp);
  fclose (din);
  gettimeofday (&stop, (struct timezone *) 0);
  getreply (0);
  if (bytes > 0 && is_retr)
    ptransfer ("received", bytes, &start, &stop);
  return;
abort:

/* abort using RFC959 recommended IP,SYNC sequence  */

  if (oldintp)
    signal (SIGPIPE, oldintr);
  signal (SIGINT, SIG_IGN);
  if (!cpend)
    {
      code = -1;
      signal (SIGINT, oldintr);
      return;
    }

  abort_remote (din);
  code = -1;
  if (data >= 0)
    {
      close (data);
      data = -1;
    }
  if (closefunc != NULL && fout != NULL)
    (*closefunc) (fout);
  if (din)
    fclose (din);
  gettimeofday (&stop, (struct timezone *) 0);
  if (bytes > 0)
    ptransfer ("received", bytes, &start, &stop);
  signal (SIGINT, oldintr);
}

/*
 * Need to start a listen on the data channel before we send the command,
 * otherwise the server's connect may fail.
 */
int
initconn (void)
{
  char *p = NULL, *a = NULL;
  int result, tmpno = 0;
  int good_epsv = 0, good_lpsv = 0, j;
  socklen_t len;
  int on = 1;
  uint32_t a0, a1, a2, a3, p0, p1, port;
  uint32_t af, hal, h[16], pal; /* RFC 1639: LPSV resonse.  */
  struct sockaddr_in *data_addr_sa4 = (struct sockaddr_in *) &data_addr;
  struct sockaddr_in6 *data_addr_sa6 = (struct sockaddr_in6 *) &data_addr;

  if (passivemode)
    {
      data = socket (myctladdr.ss_family, SOCK_STREAM, 0);
      if (data < 0)
	{
	  perror ("ftp: socket");
	  return (1);
	}
      if ((options & SO_DEBUG) &&
	  setsockopt (data, SOL_SOCKET, SO_DEBUG, (char *) &on,
		      sizeof (on)) < 0)
	if (errno != EACCES)	/* Ignore insufficient permission.  */
	  error (0, errno, "setsockopt DEBUG (ignored)");

      /* Be contemporary:
       *   first try EPSV,
       *   then fall back to PASV/LPSV.
       */
      switch (myctladdr.ss_family)
        {
	  case AF_INET:
	    if (doepsv4 && command ("EPSV") == COMPLETE)
	      {
	        good_epsv = 1;
	        break;
	      }
	    if (doepsv4)
	      {
		/* When arriving here, EPSV failed. Prevent new attempts.  */
		doepsv4 = 0;
	      }
	    if (command ("PASV") == COMPLETE)
		break;
	    if (command ("LPSV") == COMPLETE)
	      {
		good_lpsv = 1;
		break;
	      }
	    printf ("Passive mode refused.\n");
	    goto bad;
	    break;
	  case AF_INET6:
	    if (command ("EPSV") == COMPLETE)
	      {
		good_epsv = 1;
		break;
	      }
	    if (command ("LPSV") == COMPLETE)
	      {
		good_lpsv = 1;
		break;
	      }
	    printf ("Passive mode refused.\n");
	    goto bad;
	    break;
	}

      if (good_epsv)
	{
	  /* EPSV: IPv4 or IPv6
	   *
	   * Expected response (perl): pasv =~ '%u|'
	   * This communicates a port number.
	   */
	  if (sscanf (pasv, "%u|", &port) != 1)
	    {
	      printf ("Extended passive mode scan failure. "
			"Should not happen!\n");
	      (void) command ("ABOR");	/* Cancel any open connection.  */
	      goto bad;
	    }
	  data_addr = hisctladdr;
	  switch (data_addr.ss_family)
	    {
	      case AF_INET:
		data_addr_sa4->sin_port = htons (port);
		break;
	      case AF_INET6:
		data_addr_sa6->sin6_port = htons (port);
		break;
	    }
	} /* EPSV */
      else if (good_lpsv)
	{
	  /* LPSV: IPv4 or IPv6
	   *
	   * At this point we have got a string of comma
	   * separated, one-byte unsigned integer values.
	   * Length and interpretation depends on address
	   * family.
	   */

	  if (myctladdr.ss_family == AF_INET)
	    {
	      if ((sscanf (pasv, "%u," /* af */
				"%u,%u,%u,%u,%u," /* hal, h[4] */
				"%u,%u,%u", /* pal, p0, p1 */
				&af, &hal, &h[0], &h[1], &h[2], &h[3], &pal, &p0, &p1) != 9)
		  || (/* Strong checking */ af != 4 || hal != 4 || pal != 2) )
		{
		  printf ("Passive mode address scan failure. "
			  "Shouldn't happen!\n");
		  (void) command ("ABOR");	/* Cancel any open connection.  */
		  goto bad;
		}
	      for (j = 0; j < 4; ++j)
		h[j] &= 0xff; /* Mask only the significant bits.  */

	      data_addr.ss_family = AF_INET;
	      data_addr_sa4->sin_port =
		  htons (((p0 & 0xff) << 8) | (p1 & 0xff));

		{
		  uint32_t *pu32 = (uint32_t *) &data_addr_sa4->sin_addr.s_addr;
		  pu32[0] = htonl ( (h[0] << 24) | (h[1] << 16) | (h[2] << 8) | h[3]);
		}
	    } /* LPSV IPv4 */
	  else /* IPv6 */
	    {
	      if ((sscanf (pasv, "%u," /* af */
				"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u," /* hal, h[16] */
				"%u,%u,%u", /* pal, p0, p1 */
				&af, &hal, &h[0], &h[1], &h[2], &h[3], &h[4], &h[5], &h[6], &h[7],
				&h[8], &h[9], &h[10], &h[11], &h[12], &h[13], &h[14], &h[15],
				&pal, &p0, &p1) != 21)
		  || (/* Strong checking */ af != 6 || hal != 16 || pal != 2) )
		{
		  printf ("Passive mode address scan failure. "
			  "Shouldn't happen!\n");
		  (void) command ("ABOR");	/* Cancel any open connection.  */
		  goto bad;
		}
	      for (j = 0; j < 16; ++j)
		h[j] &= 0xff; /* Mask only the significant bits.  */

	      data_addr.ss_family = AF_INET6;
	      data_addr_sa6->sin6_port =
		  htons (((p0 & 0xff) << 8) | (p1 & 0xff));

		{
		  uint32_t *pu32 = (uint32_t *) &data_addr_sa6->sin6_addr.s6_addr;
		  pu32[0] = htonl ( (h[0] << 24) | (h[1] << 16) | (h[2] << 8) | h[3]);
		  pu32[1] = htonl ( (h[4] << 24) | (h[5] << 16) | (h[6] << 8) | h[7]);
		  pu32[2] = htonl ( (h[8] << 24) | (h[9] << 16) | (h[10] << 8) | h[11]);
		  pu32[3] = htonl ( (h[12] << 24) | (h[13] << 16) | (h[14] << 8) | h[15]);
		}
	    } /* LPSV IPv6 */
	}
      else /* !EPSV && !LPSV */
	{ /* PASV */
	  if (myctladdr.ss_family == AF_INET)
	    { /* PASV */
	      if (sscanf (pasv, "%u,%u,%u,%u,%u,%u",
			  &a0, &a1, &a2, &a3, &p0, &p1) != 6)
		{
		  printf ("Passive mode address scan failure. "
			  "Shouldn't happen!\n");
		  (void) command ("ABOR");	/* Cancel any open connection.  */
		  goto bad;
		}
	      data_addr.ss_family = AF_INET;
	      data_addr_sa4->sin_addr.s_addr =
		  htonl ( (a0 << 24) | ((a1 & 0xff) << 16)
			 | ((a2 & 0xff) << 8) | (a3 & 0xff) );
	      data_addr_sa4->sin_port =
		  htons (((p0 & 0xff) << 8) | (p1 & 0xff));
	    } /* PASV */
	  else
	    {
	      /* Catch all impossible cases.  */
	      printf ("Passive mode address scan failure. Shouldn't happen!\n");
	      goto bad;
	    }
	} /* PASV */

      if (connect (data, (struct sockaddr *) &data_addr, ctladdrlen) < 0)
	{
	  perror ("ftp: connect");
	  goto bad;
	}
#if defined IP_TOS && defined IPPROTO_IP && defined IPTOS_THROUGHPUT
      on = IPTOS_THROUGHPUT;
      if (data_addr.ss_family == AF_INET &&
	   setsockopt (data, IPPROTO_IP, IP_TOS, (char *) &on,
		      sizeof (int)) < 0)
	perror ("ftp: setsockopt TOS (ignored)");
#endif
      return (0);
    }

noport:
  data_addr = myctladdr;
  if (sendport)
    /* Let the system pick a port.  */
    switch (myctladdr.ss_family)
      {
	case AF_INET:
	  data_addr_sa4->sin_port = 0;
	  break;
	case AF_INET6:
	  data_addr_sa6->sin6_port = 0;
	  break;
      }

  if (data != -1)
    close (data);
  data = socket (myctladdr.ss_family, SOCK_STREAM, 0);
  if (data < 0)
    {
      error (0, errno, "socket");
      if (tmpno)
	sendport = 1;
      return (1);
    }
  if (!sendport)
    if (setsockopt (data, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof (on))
	< 0)
      {
	error (0, errno, "setsockopt (reuse address)");
	goto bad;
      }
  if (bind (data, (struct sockaddr *) &data_addr, ctladdrlen) < 0)
    {
      error (0, errno, "bind");
      goto bad;
    }
  if (options & SO_DEBUG
      && setsockopt (data, SOL_SOCKET, SO_DEBUG,
		     (char *) &on, sizeof (on)) < 0)
    if (errno != EACCES)	/* Ignore insufficient permission.  */
      error (0, errno, "setsockopt DEBUG (ignored)");
  len = sizeof (data_addr);
  if (getsockname (data, (struct sockaddr *) &data_addr, &len) < 0)
    {
      error (0, errno, "getsockname");
      goto bad;
    }
  if (listen (data, 1) < 0)
    error (0, errno, "listen");
  if (sendport)
    {
#define UC(b)	(((int)b)&0xff)
      /* Preferences:
       *   IPv4: EPRT, PORT, LPRT
       *   IPv6: EPRT, LPRT
       */
      result = ERROR;	/* For success detection.  */
      if (data_addr.ss_family != AF_INET || doepsv4)
	{
	  /* Use EPRT mode.  */
	  getnameinfo ((struct sockaddr *) &data_addr, ctladdrlen,
			ia, sizeof (ia), portstr, sizeof (portstr),
			NI_NUMERICHOST | NI_NUMERICSERV);
	  result = command ("EPRT |%d|%s|%s|",
			    (data_addr.ss_family == AF_INET) ? 1 : 2,
			    ia, portstr);
	}

      if (data_addr.ss_family == AF_INET && doepsv4 && result != COMPLETE)
	/* Do not try EPRT with IPv4 again.  It fails for this host.  */
	doepsv4 = 0;

      if (data_addr.ss_family == AF_INET && result != COMPLETE)
	{
	  /* PORT for IPv4; possibly EPRT has failed.  */
	  a = (char *) &data_addr_sa4->sin_addr;
	  p = (char *) &data_addr_sa4->sin_port;
	  result = command ("PORT %d,%d,%d,%d,%d,%d",
			    UC (a[0]), UC (a[1]), UC (a[2]), UC (a[3]),
			    UC (p[0]), UC (p[1]));
	}

      if (result != COMPLETE)
	{
	  /* Fall back to LPRT.  */
	  uint8_t *h, *p;

	  switch (data_addr.ss_family)
	    {
	      case AF_INET:
		h = (uint8_t *) &data_addr_sa4->sin_addr;
		p = (uint8_t *) &data_addr_sa4->sin_port;
		result = command ("LPRT 4,4,%u,%u,%u,%u,2,%u,%u",
				  h[0], h[1], h[2], h[3], p[0], p[1]);
		break;
	      case AF_INET6:
		h = (uint8_t *) &data_addr_sa6->sin6_addr;
		p = (uint8_t *) &data_addr_sa6->sin6_port;
		result = command ("LPRT 6,16," /* af, hal */
				  "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u," /* h[16] */
				  "2,%u,%u", /* pal, p[2] */
				  h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7],
				  h[8], h[9], h[10], h[11], h[12], h[13], h[14], h[15],
				  p[0], p[1]);
		break;
	    }
	}

      if (result == ERROR && sendport == -1)
	{
	  sendport = 0;
	  tmpno = 1;
	  goto noport;
	}
      return (result != COMPLETE);
    }
  if (tmpno)
    sendport = 1;
#if defined IP_TOS && defined IPPROTO_IP && defined IPTOS_THROUGHPUT
  on = IPTOS_THROUGHPUT;
  if (data_addr.ss_family == AF_INET &&
	setsockopt (data, IPPROTO_IP, IP_TOS, (char *) &on, sizeof (int)) < 0)
    error (0, errno, "setsockopt TOS (ignored)");
#endif
  return (0);
bad:
  close (data), data = -1;
  if (tmpno)
    sendport = 1;
  return (1);
}

FILE *
dataconn (char *lmode)
{
  struct sockaddr_storage from;
  int s, tos;
  socklen_t fromlen = sizeof (from);

  if (passivemode)
    return (fdopen (data, lmode));

  s = accept (data, (struct sockaddr *) &from, &fromlen);
  if (s < 0)
    {
      error (0, errno, "accept");
      close (data), data = -1;
      return (NULL);
    }
  close (data);
  data = s;
#if defined IP_TOS && defined IPPROTO_IP && defined IPTOS_THROUGHPUT
  tos = IPTOS_THROUGHPUT;
  if (from.ss_family == AF_INET &&
	setsockopt (s, IPPROTO_IP, IP_TOS, (char *) &tos, sizeof (int)) < 0)
    error (0, errno, "setsockopt TOS (ignored)");
#endif
  return (fdopen (data, lmode));
}

void
ptransfer (char *direction, long long int bytes,
	   struct timeval *t0, struct timeval *t1)
{
  struct timeval td;
  float s, bs;

  if (verbose)
    {
      tvsub (&td, t1, t0);
      s = td.tv_sec + (td.tv_usec / 1000000.);
#define nz(x)	((x) == 0 ? 1 : (x))
      bs = bytes / nz (s);

      printf ("%lld bytes %s in %.3g seconds", bytes, direction, s);

      if (bs > 1048576.0)
	printf (" (%.3g Mbytes/s)\n", bs / 1048576.0);
      else if (bs > 1024.0)
	printf (" (%.3g kbytes/s)\n", bs / 1024.0);
      else
	printf (" (%.3g bytes/s)\n", bs);
    }
}

/*
void
tvadd(tsum, t0)
	struct timeval *tsum, *t0;
{

	tsum->tv_sec += t0->tv_sec;
	tsum->tv_usec += t0->tv_usec;
	if (tsum->tv_usec > 1000000)
		tsum->tv_sec++, tsum->tv_usec -= 1000000;
}
*/

void
tvsub (struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{

  tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
  tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
  if (tdiff->tv_usec < 0)
    tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

void
psabort (int sig _GL_UNUSED_PARAMETER)
{

  abrtflag++;
}

void
pswitch (int flag)
{
  sighandler_t oldintr;
  static struct comvars
  {
    int connect;
    char *name;
    struct sockaddr_storage mctl;
    struct sockaddr_storage hctl;
    FILE *in;
    FILE *out;
    int tpe;
    int curtpe;
    int cpnd;
    int sunqe;
    int runqe;
    int mcse;
    int ntflg;
    char nti[sizeof (ntin)];
    char nto[sizeof (ntout)];
    int mapflg;
    char *mi;
    char *mo;
  } proxstruct =
  {
  0}, tmpstruct =
  {
  0};
  struct comvars *ip, *op;

  abrtflag = 0;
  oldintr = signal (SIGINT, psabort);
  if (flag)
    {
      if (proxy)
	return;
      ip = &tmpstruct;
      op = &proxstruct;
      proxy++;
    }
  else
    {
      if (!proxy)
	return;
      ip = &proxstruct;
      op = &tmpstruct;
      proxy = 0;
    }
  ip->connect = connected;
  connected = op->connect;

  free (ip->name);
  ip->name = hostname;
  hostname = op->name;
  op->name = 0;

  ip->hctl = hisctladdr;
  hisctladdr = op->hctl;
  ip->mctl = myctladdr;
  myctladdr = op->mctl;
  ip->in = cin;
  cin = op->in;
  ip->out = cout;
  cout = op->out;
  ip->tpe = type;
  type = op->tpe;
  ip->curtpe = curtype;
  curtype = op->curtpe;
  ip->cpnd = cpend;
  cpend = op->cpnd;
  ip->sunqe = sunique;
  sunique = op->sunqe;
  ip->runqe = runique;
  runique = op->runqe;
  ip->mcse = mcase;
  mcase = op->mcse;
  ip->ntflg = ntflag;
  ntflag = op->ntflg;
  strncpy (ip->nti, ntin, sizeof (ntin) - 1);
  (ip->nti)[strlen (ip->nti)] = '\0';
  strcpy (ntin, op->nti);
  strncpy (ip->nto, ntout, sizeof (ntout) - 1);
  (ip->nto)[strlen (ip->nto)] = '\0';
  strcpy (ntout, op->nto);
  ip->mapflg = mapflag;
  mapflag = op->mapflg;

  free (ip->mi);
  ip->mi = mapin;
  mapin = op->mi;
  op->mi = 0;

  free (ip->mo);
  ip->mo = mapout;
  mapout = op->mo;
  op->mo = 0;

  signal (SIGINT, oldintr);
  if (abrtflag)
    {
      abrtflag = 0;
      (*oldintr) (SIGINT);
    }
}

void
abortpt (int sig _GL_UNUSED_PARAMETER)
{

  printf ("\n");
  fflush (stdout);
  ptabflg++;
  mflag = 0;
  abrtflag = 0;
  longjmp (ptabort, 1);
}

void
proxtrans (char *cmd, char *local, char *remote)
{
  sighandler_t oldintr;
  int secndflag = 0, prox_type, nfnd;
  char *cmd2;
  fd_set mask;

  if (strcmp (cmd, "RETR"))
    cmd2 = "RETR";
  else
    cmd2 = runique ? "STOU" : "STOR";
  if ((prox_type = type) == 0)
    {
      if (unix_server && unix_proxy)
	prox_type = TYPE_I;
      else
	prox_type = TYPE_A;
    }
  if (curtype != prox_type)
    changetype (prox_type, 1);
  if (command ("PASV") != COMPLETE)
    {
      printf ("proxy server does not support third party transfers.\n");
      return;
    }
  pswitch (0);
  if (!connected)
    {
      printf ("No primary connection\n");
      pswitch (1);
      code = -1;
      return;
    }
  if (curtype != prox_type)
    changetype (prox_type, 1);
  if (command ("PORT %s", pasv) != COMPLETE)
    {
      pswitch (1);
      return;
    }
  if (setjmp (ptabort))
    goto abort;
  oldintr = signal (SIGINT, abortpt);
  if (command ("%s %s", cmd, remote) != PRELIM)
    {
      signal (SIGINT, oldintr);
      pswitch (1);
      return;
    }
  sleep (2);
  pswitch (1);
  secndflag++;
  if (command ("%s %s", cmd2, local) != PRELIM)
    goto abort;
  ptflag++;
  getreply (0);
  pswitch (0);
  getreply (0);
  signal (SIGINT, oldintr);
  pswitch (1);
  ptflag = 0;
  printf ("local: %s remote: %s\n", local, remote);
  return;
abort:
  signal (SIGINT, SIG_IGN);
  ptflag = 0;
  if (strcmp (cmd, "RETR") && !proxy)
    pswitch (1);
  else if (!strcmp (cmd, "RETR") && proxy)
    pswitch (0);
  if (!cpend && !secndflag)
    {				/* only here if cmd = "STOR" (proxy=1) */
      if (command ("%s %s", cmd2, local) != PRELIM)
	{
	  pswitch (0);
	  if (cpend)
	    abort_remote ((FILE *) NULL);
	}
      pswitch (1);
      if (ptabflg)
	code = -1;
      signal (SIGINT, oldintr);
      return;
    }
  if (cpend)
    abort_remote ((FILE *) NULL);
  pswitch (!proxy);
  if (!cpend && !secndflag)
    {				/* only if cmd = "RETR" (proxy=1) */
      if (command ("%s %s", cmd2, local) != PRELIM)
	{
	  pswitch (0);
	  if (cpend)
	    abort_remote ((FILE *) NULL);
	  pswitch (1);
	  if (ptabflg)
	    code = -1;
	  signal (SIGINT, oldintr);
	  return;
	}
    }
  if (cpend)
    abort_remote ((FILE *) NULL);
  pswitch (!proxy);
  if (cpend)
    {
      FD_ZERO (&mask);
      FD_SET (fileno (cin), &mask);
      if ((nfnd = empty (&mask, 10)) <= 0)
	{
	  if (nfnd < 0)
	    {
	      error (0, errno, "abort");
	    }
	  if (ptabflg)
	    code = -1;
	  lostpeer (0);
	}
      getreply (0);
      getreply (0);
    }
  if (proxy)
    pswitch (0);
  pswitch (1);
  if (ptabflg)
    code = -1;
  signal (SIGINT, oldintr);
}

void
reset (int argc _GL_UNUSED_PARAMETER, char **argv _GL_UNUSED_PARAMETER)
{
  fd_set mask;
  int nfnd = 1;

  FD_ZERO (&mask);
  while (nfnd > 0)
    {
      FD_SET (fileno (cin), &mask);
      if ((nfnd = empty (&mask, 0)) < 0)
	{
	  error (0, errno, "reset");
	  code = -1;
	  lostpeer (0);
	}
      else if (nfnd)
	{
	  getreply (0);
	}
    }
}

char *
gunique (char *local)
{
  static char *new = 0;
  char *cp;
  int count = 0;
  char ext = '1';

  free (new);
  new = malloc (strlen (local) + 1 + 3 + 1);	/* '.' + 100 + '\0' */
  if (!new)
    {
      printf ("gunique: malloc failed.\n");
      return 0;
    }
  strcpy (new, local);

  cp = new + strlen (new);
  *cp++ = '.';
  for (;;)
    {
      struct stat st;

      if (++count == 100)
	{
	  printf ("runique: can't find unique file name.\n");
	  return ((char *) 0);
	}
      *cp++ = ext;
      *cp = '\0';
      if (ext == '9')
	ext = '0';
      else
	ext++;

      if (stat (new, &st) != 0)
        {
          if (errno == ENOENT)
            return new;
          else
            return 0;
        }

      if (ext != '0')
	cp--;
      else if (*(cp - 2) == '.')
	*(cp - 1) = '1';
      else
	{
	  *(cp - 2) = *(cp - 2) + 1;
	  cp--;
	}
    }
}

void
abort_remote (FILE *din)
{
  char buf[BUFSIZ];
  int nfnd;
  fd_set mask;

  /*
   * send IAC in urgent mode instead of DM because 4.3BSD places oob mark
   * after urgent byte rather than before as is protocol now
   */
  sprintf (buf, "%c%c%c", IAC, IP, IAC);
  if (send (fileno (cout), buf, 3, MSG_OOB) != 3)
    error (0, errno, "abort");
  fprintf (cout, "%cABOR\r\n", DM);
  fflush (cout);
  FD_ZERO (&mask);
  FD_SET (fileno (cin), &mask);
  if (din)
    {
      FD_SET (fileno (din), &mask);
    }
  if ((nfnd = empty (&mask, 10)) <= 0)
    {
      if (nfnd < 0)
	{
	  error (0, errno, "abort");
	}
      if (ptabflg)
	code = -1;
      lostpeer (0);
    }
  if (din && FD_ISSET (fileno (din), &mask))
    {
      while (read (fileno (din), buf, sizeof (buf)) > 0)
	/* LOOP */ ;
    }
  if (getreply (0) == ERROR && code == 552)
    {
      /* 552 needed for nic style abort */
      getreply (0);
    }
  getreply (0);
}
