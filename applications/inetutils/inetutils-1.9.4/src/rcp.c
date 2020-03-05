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
 * Copyright (c) 1983, 1990, 1992, 1993, 2002
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
#include <sys/time.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>		/* intmax_t */
#include <string.h>
#include <string.h>
#include <unistd.h>
#ifndef HAVE_UTIMES
# include <utime.h>		/* If we don't have utimes(), use utime(). */
#endif
#include <progname.h>
#include <unused-parameter.h>
#include <libinetutils.h>
#include <argp.h>
#include <error.h>
#include <xalloc.h>

typedef struct {
  int cnt;
  char *buf;
} BUF;

BUF *allocbuf (BUF *, int, int);
char *colon (char *);
void lostconn (int);
void nospace (void);
int okname (char *);
void run_err (const char *, ...);
int susystem (char *, int);
void verifydir (char *);

#ifdef SHISHI
# include <shishi.h>
# include "shishi_def.h"
#endif /* SHISHI */

#ifdef KERBEROS
# ifdef HAVE_KERBEROSIV_DES_H
#  include <kerberosIV/des.h>
# endif
# ifdef HAVE_KERBEROSIV_KRB_H
#  include <kerberosIV/krb.h>
# endif
#endif /* KERBEROS */

#if defined KERBEROS || defined SHISHI
const char *dest_realm = NULL;
int use_kerberos = 1;
int doencrypt = 0;

# ifdef KERBEROS
CREDENTIALS cred;
Key_schedule schedule;
extern char *krb_realmofhost ();

# elif defined SHISHI /* !KERBEROS  */
Shishi *h;
Shishi_key *enckey;
shishi_ivector iv1, iv2, iv3, iv4;
shishi_ivector *ivtab[4];

int keytype;
int keylen;
int rc;
int wlen;
# endif /* SHISHI */
#endif /* KERBEROS || SHISHI */

const char doc[] = "Remote copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.";
const char arg_doc[] = "SOURCE DEST\n"
                       "SOURCE... DIRECTORY\n"
                       "--target-directory=DIRECTORY SOURCE...";

char *target = NULL;
int preserve_option;
int from_option, to_option;
int iamremote, iamrecursive, targetshouldbedirectory;
#if defined WITH_ORCMD_AF || defined WITH_RCMD_AF || defined SHISHI
sa_family_t family = AF_UNSPEC;
#endif

static struct argp_option options[] = {
#define GRID 0
  { "recursive", 'r', NULL, 0,
    "if any of the source files are directories, copies"
    " each subtree rooted at that name; in this case the"
    " destination must be a directory",
    GRID+1 },
  { "preserve", 'p', NULL, 0,
    "attempt to preserve (duplicate) in its copies the"
    " modification times and modes of the source files",
    GRID+1 },
  { "target-directory", 'd', "DIRECTORY", OPTION_ARG_OPTIONAL,
    "copy all SOURCE arguments into DIRECTORY",
    GRID+1 },
  { "from", 'f', NULL, 0,
    "copying from remote host (server use only)",
    GRID+1 },
  { "to", 't', NULL, 0,
    "copying to remote host (server use only)",
    GRID+1 },
#if defined WITH_ORCMD_AF || defined WITH_RCMD_AF || defined SHISHI
  { "ipv4", '4', NULL, 0,
    "use only IPv4",
    GRID+1 },
  { "ipv6", '6', NULL, 0,
    "use only IPv6",
    GRID+1 },
#endif /* WITH_ORCMD_AF || WITH_RCMD_AF || SHISHI */
#undef GRID
#if defined KERBEROS || defined SHISHI
# define GRID 10
  { "kerberos", 'K', NULL, 0,
    "turns off all Kerberos authentication",
    GRID+1 },
  { "realm", 'k', "REALM", 0,
    "obtain tickets for a remote host in REALM instead of the remote host's realm",
    GRID+1 },
# ifdef ENCRYPTION
  { "encrypt", 'x', NULL, 0,
    "encrypt all data transfer",
    GRID+1 },
# endif
# undef GRID
#endif
  { NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state _GL_UNUSED_PARAMETER)
{
  switch (key)
    {
#if defined WITH_ORCMD_AF || defined WITH_RCMD_AF || defined SHISHI
    case '4':
      family = AF_INET;
      break;
    case '6':
      family = AF_INET6;
      break;
#endif /* WITH_ORCMD_AF || WITH_RCMD_AF || SHISHI */

#if defined KERBEROS || defined SHISHI
    case 'K':
      use_kerberos = 0;
      break;

    case 'k':
      dest_realm = arg;
      break;

# ifdef ENCRYPTION
    case 'x':
      doencrypt = 1;
#  ifdef KERBEROS
      des_set_key(cred.session, schedule);
#  endif
      break;
# endif /* ENCRYPTION */
#endif /* KERBEROS || SHISHI */

    case 'p':
      preserve_option = 1;
      break;

    case 'r':
      iamrecursive = 1;
      break;

      /* Server options. */
    case 'd':
      targetshouldbedirectory = 1;
      if (arg && strlen (arg))
	target = xstrdup (arg);	/* Client side use.  */
      break;

    case 'f':		/* "from" */
      iamremote = 1;
      from_option = 1;
      break;

    case 't':		/* "to" */
      iamremote = 1;
      to_option = 1;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp = {
  options,
  parse_opt,
  arg_doc,
  doc,
  NULL,
  NULL,
  NULL
};

struct passwd *pwd;
unsigned short port;
uid_t userid, effuid;
int errs, rem;

char *command;

#if defined KERBEROS || defined SHISHI
int kerberos (char **, char *, char *, char *);
void oldw (const char *, ...);
#endif /* KERBEROS || SHISHI */

int response (void);
void rsource (char *, struct stat *);
void sink (int, char *[]);
void source (int, char *[]);
void tolocal (int, char *[]);
void toremote (char *, int, char *[]);

int
main (int argc, char *argv[])
{
  struct servent *sp;
  char *targ;
  const char *shell;
  int index, rc;

  set_program_name (argv[0]);

  from_option = to_option = 0;
  iu_argp_init ("rcp", default_program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);
  argc -= index;
  argv += index;

#if defined KERBEROS || defined SHISHI
  if (use_kerberos)
    {
# if defined ENCRYPTION && defined KERBEROS
      shell = doencrypt ? "ekshell" : "kshell";
# else /* SHISHI */
      shell = "kshell";		/* Libshishi uses a single service.  */
# endif

      sp = getservbyname (shell, "tcp");
      if (sp == NULL)
	{
	  use_kerberos = 0;
	  oldw ("can't get entry for %s/tcp service", shell);
	  sp = getservbyname (shell = "shell", "tcp");
	}
    }
  else
    sp = getservbyname (shell = "shell", "tcp");
#else /* !KERBEROS && !SHISHI */
  sp = getservbyname (shell = "shell", "tcp");
#endif
  if (sp == NULL)
    error (EXIT_FAILURE, 0, "%s/tcp: unknown service", shell);
  port = sp->s_port;

  effuid = geteuid ();
  userid = getuid ();
  pwd = getpwuid (userid);
  if (pwd == NULL)
    error (EXIT_FAILURE, 0, "unknown user %d", (int) userid);

  rem = STDIN_FILENO;		/* XXX */

  if (from_option)
    {				/* Follow "protocol", send data. */
      response ();
      setuid (userid);
      source (argc, argv);
      exit (errs);
    }

  if (to_option)
    {				/* Receive data. */
      setuid (userid);
      sink (argc, argv);
      exit (errs);
    }

  if (argc < 1 || (argc < 2 && !(target && strlen (target))))
    error (EXIT_FAILURE, 0, "not enough arguments");

  if (argc > 2)
    targetshouldbedirectory = 1;

#if defined KERBEROS || defined SHISHI
  if (doencrypt && !use_kerberos)
    error (EXIT_FAILURE, 0, "encryption must use Kerberos");
#endif

  /* Command to be executed on remote system using "rsh". */
  rc = asprintf (&command, "rcp%s%s%s",
		 iamrecursive ? " -r" : "", preserve_option ? " -p" : "",
		 targetshouldbedirectory ? " -d" : "");

  if (rc < 0)
    xalloc_die ();

  rem = -1;
  signal (SIGPIPE, lostconn);

  /* Without a specified target, the last argument
   * is extracted to serve as target.
   */
  if (!target || !strlen (target))
    {
      target = xstrdup (argv[argc - 1]);
      argc--;			/* Do not count target directory.  */
    }

  targ = colon (target);
  if (targ)			/* Dest is remote host. */
    toremote (targ, argc, argv);
  else
    {
      tolocal (argc, argv);	/* Dest is local host. */
      if (targetshouldbedirectory)
	verifydir (target);
    }
  exit (errs);
}

void
toremote (char *targ, int argc, char *argv[])
{
  int i, tos;
  char *bp, *host, *src, *suser, *thost, *tuser;
#if defined IP_TOS && defined IPPROTO_IP && defined IPTOS_THROUGHPUT
  struct sockaddr_storage ss;
  socklen_t sslen;
#endif

  *targ++ = 0;
  if (*targ == 0)
    targ = ".";

  thost = strchr (target, '@');
  if (thost)
    {
      /* user@host */
      *thost++ = 0;
      tuser = target;
      if (*tuser == '\0')
	tuser = NULL;
      else if (!okname (tuser))
	exit (EXIT_FAILURE);
    }
  else
    {
      thost = target;
      tuser = NULL;
    }

  for (i = 0; i < argc; i++)
    {
      src = colon (argv[i]);
      if (src)
	{			/* remote to remote */
	  *src++ = 0;
	  if (*src == 0)
	    src = ".";
	  host = strchr (argv[i], '@');

	  if (host)
	    {
	      *host++ = 0;
	      suser = argv[i];
	      if (*suser == '\0')
		suser = pwd->pw_name;
	      else if (!okname (suser))
		continue;
	      if (asprintf (&bp,
#if defined ENCRYPTION && (defined KERBEROS || defined SHISHI)
			    "%s%s -l %s -n %s %s %s '%s%s%s:%s'",
#else
			    "%s -l %s -n %s %s %s '%s%s%s:%s'",
#endif
			    PATH_RSH,
#if ENCRYPTION && (defined KERBEROS || defined SHISHI)
			    doencrypt ? " -x" : "",
#endif
			    suser, host, command, src,
			    tuser ? tuser : "", tuser ? "@" : "",
			    thost, targ) < 0)
		xalloc_die ();
	    }
	  else
	    {
	      if (asprintf (&bp,
#if defined ENCRYPTION && (defined KERBEROS || defined SHISHI)
			    "exec %s%s -n %s %s %s '%s%s%s:%s'",
#else
			    "exec %s -n %s %s %s '%s%s%s:%s'",
#endif
			    PATH_RSH,
#if ENCRYPTION && (defined KERBEROS || defined SHISHI)
			    doencrypt ? " -x" : "",
#endif
			    argv[i], command, src,
			    tuser ? tuser : "", tuser ? "@" : "",
			    thost, targ) < 0)
		xalloc_die ();
	    }
	  susystem (bp, userid);
	  free (bp);
	}
      else
	{			/* local to remote */
	  if (rem == -1)
	    {
	      if (asprintf (&bp, "%s -t %s", command, targ) < 0)
		xalloc_die ();
	      host = thost;
#if defined KERBEROS || defined SHISHI
	      if (use_kerberos)
		rem = kerberos (&host, bp, pwd->pw_name,
				tuser ? tuser : pwd->pw_name);
	      else
#endif /* KERBEROS || SHISHI */
#ifdef WITH_ORCMD_AF
		rem = orcmd_af (&host, port, pwd->pw_name,
				tuser ? tuser : pwd->pw_name,
				bp, 0, family);
#elif defined WITH_RCMD_AF
		rem = rcmd_af (&host, port, pwd->pw_name,
			       tuser ? tuser : pwd->pw_name,
			       bp, 0, family);
#elif defined WITH_ORCMD
		rem = orcmd (&host, port, pwd->pw_name,
			     tuser ? tuser : pwd->pw_name, bp, 0);
#else /* !WITH_ORCMD_AF && !WITH_RCMD_AF && !WITH_ORCMD */
		rem = rcmd (&host, port, pwd->pw_name,
			    tuser ? tuser : pwd->pw_name, bp, 0);
#endif
	      if (rem < 0)
		{
		  /* rcmd() provides its own error messages,
		   * but we add a vital addition, caused by
		   * insufficient capabilites.
		   */
		  if (errno == EACCES)
		    error (EXIT_FAILURE, 0,
			   "No access to privileged ports.");

		  exit (EXIT_FAILURE);
		}
#if defined IP_TOS && defined IPPROTO_IP && defined IPTOS_THROUGHPUT
	      sslen = sizeof (ss);
	      (void) getpeername (rem, (struct sockaddr *) &ss, &sslen);
	      tos = IPTOS_THROUGHPUT;
	      if (ss.ss_family == AF_INET &&
		  setsockopt (rem, IPPROTO_IP, IP_TOS,
			      (char *) &tos, sizeof (int)) < 0)
		if (errno != ENOPROTOOPT)
		  error (0, errno, "TOS (ignored)");
#endif
	      if (response () < 0)
		exit (EXIT_FAILURE);
	      free (bp);
	      setuid (userid);
	    }
	  source (1, argv + i);
	  close (rem);
	  rem = -1;
#ifdef SHISHI
	  if (use_kerberos)
	    {
	      shishi_done (h);
# ifdef ENCRYPTION
	      if (doencrypt)
		{
		  shishi_key_done (enckey);
		  for (i = 0; i < 4; i++)
		    {
		      shishi_crypto_close (ivtab[i]->ctx);
		      free (ivtab[i]->iv);
		    }
		}
# endif /* ENCRYPTION */
	    }
#endif /* SHISHI */
	}
    }
}

void
tolocal (int argc, char *argv[])
{
  int i, len, tos;
  char *bp, *host, *src, *suser, *vect[1];
#if defined IP_TOS && defined IPPROTO_IP && defined IPTOS_THROUGHPUT
  struct sockaddr_storage ss;
  socklen_t sslen;
#endif

  for (i = 0; i < argc; i++)
    {
      src = colon (argv[i]);
      if (!src)
	{			/* Local to local. */
	  len = strlen (PATH_CP) + strlen (argv[i]) +
		strlen (target) + 20;
	  if (asprintf (&bp, "exec %s%s%s %s %s",
			PATH_CP,
			iamrecursive ? " -R" : "",
			preserve_option ? " -p" : "",
			argv[i], target) < 0)
	    xalloc_die ();
	  if (susystem (bp, userid))
	    ++errs;
	  free (bp);
	  continue;
	}
      *src++ = 0;
      if (*src == 0)
	src = ".";

      host = strchr (argv[i], '@');
      if (host == NULL)
	{
	  host = argv[i];
	  suser = pwd->pw_name;
	}
      else
	{
	  *host++ = 0;
	  suser = argv[i];
	  if (*suser == '\0')
	    suser = pwd->pw_name;
	  else if (!okname (suser))
	    continue;
	}
      if (asprintf (&bp, "%s -f %s", command, src) < 0)
	xalloc_die ();

#if defined KERBEROS || defined SHISHI
      if (use_kerberos)
	rem = kerberos (&host, bp, pwd->pw_name, suser);
      else
#elif defined WITH_ORCMD_AF
	rem = orcmd_af (&host, port, pwd->pw_name, suser, bp, 0, family);
#elif defined WITH_RCMD_AF
	rem = rcmd_af (&host, port, pwd->pw_name, suser, bp, 0, family);
#elif defined WITH_ORCMD
	rem = orcmd (&host, port, pwd->pw_name, suser, bp, 0);
#else /* !WITH_ORCMD_AF && !WITH_RCMD_AF && !WITH_ORCMD */
	rem = rcmd (&host, port, pwd->pw_name, suser, bp, 0);
#endif
      free (bp);
      if (rem < 0)
	{
	  ++errs;
	  continue;
	}
      seteuid (userid);
#if defined IP_TOS && defined IPPROTO_IP && defined IPTOS_THROUGHPUT
      sslen = sizeof (ss);
      (void) getpeername (rem, (struct sockaddr *) &ss, &sslen);
      tos = IPTOS_THROUGHPUT;
      if (ss.ss_family == AF_INET &&
	  setsockopt (rem, IPPROTO_IP, IP_TOS,
		      (char *) &tos, sizeof (int)) < 0)
	if (errno != ENOPROTOOPT)
	  error (0, errno, "TOS (ignored)");
#endif
      vect[0] = target;
      sink (1, vect);
      seteuid (effuid);
      close (rem);
      rem = -1;
#ifdef SHISHI
      if (use_kerberos)
	shishi_done (h);
	{
# ifdef ENCRYPTION
	  if (doencrypt)
	    {
	      shishi_key_done (enckey);
	      for (i = 0; i < 4; i++)
		{
		  shishi_crypto_close (ivtab[i]->ctx);
		  free (ivtab[i]->iv);
		}
	    }
# endif /* ENCRYPTION */
	}
#endif /* SHISHI */
    }
}

static int
write_stat_time (int fd, struct stat *stat)
{
  char buf[4 * sizeof (long) * 3 + 2];
  time_t a_sec, m_sec;
  long a_usec = 0, m_usec = 0;

  a_sec = stat->st_atime;
#ifdef HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC
  a_usec = stat->st_atim.tv_nsec / 1000;
#elif defined HAVE_STRUCT_STAT_ST_ATIM_TV_USEC
  a_usec = stat->st_atim.tv_usec;
#endif

  m_sec = stat->st_mtime;
#ifdef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
  m_usec = stat->st_mtim.tv_nsec / 1000;
#elif defined HAVE_STRUCT_STAT_ST_MTIM_TV_USEC
  m_usec = stat->st_mtim.tv_usec;
#endif

  snprintf (buf, sizeof (buf), "T%ld %ld %ld %ld\n",
	    m_sec, m_usec, a_sec, a_usec);
  return write (fd, buf, strlen (buf));
}

void
source (int argc, char *argv[])
{
  struct stat stb;
  static BUF buffer;
  BUF *bp;
  off_t i;
  int amt, fd, haderr, indx, result;
  char *last, *name, buf[BUFSIZ];

  for (indx = 0; indx < argc; ++indx)
    {
      name = argv[indx];
      fd = open (name, O_RDONLY, 0);
      if (fd < 0)
	goto syserr;
      if (fstat (fd, &stb))
	{
	syserr:
	  run_err ("%s: %s", name, strerror (errno));
	  goto next;
	}
      switch (stb.st_mode & S_IFMT)
	{
	case S_IFREG:
	  break;

	case S_IFDIR:
	  if (iamrecursive)
	    {
	      rsource (name, &stb);
	      goto next;
	    }
	default:
	  run_err ("%s: not a regular file", name);
	  goto next;
	}
      last = strrchr (name, '/');
      if (last == NULL)
	last = name;
      else
	++last;
      if (preserve_option)
	{
	  write_stat_time (rem, &stb);
	  if (response () < 0)
	    goto next;
	}
#define RCP_MODEMASK	(S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)
      snprintf (buf, sizeof buf, "C%04o %jd %s\n",
		stb.st_mode & RCP_MODEMASK, (intmax_t) stb.st_size, last);
      write (rem, buf, strlen (buf));
      if (response () < 0)
	goto next;

      bp = allocbuf (&buffer, fd, BUFSIZ);
      if (bp == NULL)
	{
	next:
	  close (fd);
	  continue;
	}

      /* Keep writing after an error so that we stay sync'd up. */
      for (haderr = i = 0; i < stb.st_size; i += bp->cnt)
	{
	  amt = bp->cnt;
	  if (i + amt > stb.st_size)
	    amt = stb.st_size - i;
	  if (!haderr)
	    {
	      result = read (fd, bp->buf, amt);
	      if (result != amt)
		haderr = result >= 0 ? EIO : errno;
	    }
	  if (haderr)
	    write (rem, bp->buf, amt);
	  else
	    {
	      result = write (rem, bp->buf, amt);
	      if (result != amt)
		haderr = result >= 0 ? EIO : errno;
	    }
	}
      if (close (fd) && !haderr)
	haderr = errno;
      if (!haderr)
	write (rem, "", 1);
      else
	run_err ("%s: %s", name, strerror (haderr));
      response ();
    }
}

void
rsource (char *name, struct stat *statp)
{
  DIR *dirp;
  struct dirent *dp;
  char *last, *vect[1];
  char *buf;
  int buf_len;

  dirp = opendir (name);
  if (!dirp)
    {
      run_err ("%s: %s", name, strerror (errno));
      return;
    }
  last = strrchr (name, '/');
  if (last == 0)
    last = name;
  else
    last++;

  if (preserve_option)
    {
      write_stat_time (rem, statp);
      if (response () < 0)
	{
	  closedir (dirp);
	  return;
	}
    }

  buf_len =
    1 + sizeof (int) * 3 + 1 + sizeof (int) * 3 + 1 + strlen (last) + 2;
  buf = malloc (buf_len);
  if (!buf)
    {
      run_err ("malloc failed for %d bytes", buf_len);
      closedir (dirp);
      return;
    }

  sprintf (buf, "D%04o %d %s\n", statp->st_mode & RCP_MODEMASK, 0, last);
  write (rem, buf, strlen (buf));
  free (buf);

  if (response () < 0)
    {
      closedir (dirp);
      return;
    }

  while ((dp = readdir (dirp)))
    {
      if (!strcmp (dp->d_name, ".") || !strcmp (dp->d_name, ".."))
	continue;

      buf_len = strlen (name) + 1 + strlen (dp->d_name) + 1;
      buf = malloc (buf_len);
      if (!buf)
	{
	  run_err ("malloc_failed for %d bytes", buf_len);
	  continue;
	}

      sprintf (buf, "%s/%s", name, dp->d_name);
      vect[0] = buf;

      source (1, vect);

      free (buf);
    }

  closedir (dirp);
  write (rem, "E\n", 2);
  response ();
}

void
sink (int argc, char *argv[])
{
  static BUF buffer;
  struct stat stb;
  struct timeval tv[2];
  enum
  { YES, NO, DISPLAYED } wrerr;
  BUF *bp;
  off_t i, j;
  int amt, count, exists, first, mask, mode, ofd, omode;
  int setimes, size, targisdir, wrerrno;
  char ch, *cp, *np, *targ, *vect[1], buf[BUFSIZ];
  const char *why;

#define atime	tv[0]
#define mtime	tv[1]
#define SCREWUP(str)	{ why = str; goto screwup; }

  setimes = targisdir = 0;
  mask = umask (0);
  if (!preserve_option)
    umask (mask);
  if (argc != 1)
    {
      run_err ("ambiguous target");
      exit (EXIT_FAILURE);
    }
  targ = *argv;
  if (targetshouldbedirectory)
    verifydir (targ);
  write (rem, "", 1);
  if (stat (targ, &stb) == 0 && S_ISDIR (stb.st_mode))
    targisdir = 1;
  for (first = 1;; first = 0)
    {
      cp = buf;
      if (read (rem, cp, 1) <= 0)
	return;
      if (*cp++ == '\n')
	SCREWUP ("unexpected <newline>");
      do
	{
	  if (read (rem, &ch, sizeof ch) != sizeof ch)
	    SCREWUP ("lost connection");
	  *cp++ = ch;
	}
      while (cp < &buf[BUFSIZ - 1] && ch != '\n');
      *cp = 0;

      if (buf[0] == '\01' || buf[0] == '\02')
	{
	  if (iamremote == 0)
	    write (STDERR_FILENO, buf + 1, strlen (buf + 1));
	  if (buf[0] == '\02')
	    exit (EXIT_FAILURE);
	  ++errs;
	  continue;
	}
      if (buf[0] == 'E')
	{
	  write (rem, "", 1);
	  return;
	}

      if (ch == '\n')
	*--cp = 0;

#define getnum(t) (t) = 0; while (isdigit(*cp)) (t) = (t) * 10 + (*cp++ - '0');
      cp = buf;
      if (*cp == 'T')
	{
	  setimes++;
	  cp++;
	  getnum (mtime.tv_sec);
	  if (*cp++ != ' ')
	    SCREWUP ("mtime.sec not delimited");
	  getnum (mtime.tv_usec);
	  if (*cp++ != ' ')
	    SCREWUP ("mtime.usec not delimited");
	  getnum (atime.tv_sec);
	  if (*cp++ != ' ')
	    SCREWUP ("atime.sec not delimited");
	  getnum (atime.tv_usec);
	  if (*cp++ != '\0')
	    SCREWUP ("atime.usec not delimited");
	  write (rem, "", 1);
	  continue;
	}
      if (*cp != 'C' && *cp != 'D')
	{
	  /*
	   * Check for the case "rcp remote:foo\* local:bar".
	   * In this case, the line "No match." can be returned
	   * by the shell before the rcp command on the remote is
	   * executed so the ^Aerror_message convention isn't
	   * followed.
	   */
	  if (first)
	    {
	      run_err ("%s", cp);
	      exit (EXIT_FAILURE);
	    }
	  SCREWUP ("expected control record");
	}
      mode = 0;
      for (++cp; cp < buf + 5; cp++)
	{
	  if (*cp < '0' || *cp > '7')
	    SCREWUP ("bad mode");
	  mode = (mode << 3) | (*cp - '0');
	}
      if (*cp++ != ' ')
	SCREWUP ("mode not delimited");

      for (size = 0; isdigit (*cp);)
	size = size * 10 + (*cp++ - '0');
      if (*cp++ != ' ')
	SCREWUP ("size not delimited");
      if (targisdir)
	{
	  static char *namebuf = NULL;
	  static size_t cursize = 0;
	  size_t need;

	  need = strlen (targ) + strlen (cp) + 250;
	  if (need > cursize)
	    {
	      free (namebuf);
	      namebuf = malloc (need);
	      if (namebuf)
		cursize = need;
	      else
		{
		  run_err ("%s", strerror (errno));
		  cursize = 0;
		  continue;
		}
	    }
	  snprintf (namebuf, cursize, "%s%s%s", targ, *targ ? "/" : "", cp);
	  np = namebuf;
	}
      else
	np = targ;
      exists = stat (np, &stb) == 0;
      if (buf[0] == 'D')
	{
	  int mod_flag = preserve_option;
	  if (exists)
	    {
	      if (!S_ISDIR (stb.st_mode))
		{
		  errno = ENOTDIR;
		  goto bad;
		}
	      if (preserve_option)
		chmod (np, mode);
	    }
	  else
	    {
	      /* Handle copying from a read-only directory */
	      mod_flag = 1;
	      if (mkdir (np, mode | S_IRWXU) < 0)
		goto bad;
	    }
	  vect[0] = np;
	  sink (1, vect);
	  if (setimes)
	    {
#ifndef HAVE_UTIMES
	      struct utimbuf utbuf;
	      utbuf.actime = atime.tv_sec;
	      utbuf.modtime = mtime.tv_sec;
#endif
	      setimes = 0;
#ifdef HAVE_UTIMES
	      if (utimes (np, tv) < 0)
#else
	      if (utime (np, &utbuf) < 0)
#endif
		run_err ("%s: set times: %s", np, strerror (errno));
	    }
	  if (mod_flag)
	    chmod (np, mode);
	  continue;
	}
      omode = mode;
      mode |= S_IWRITE;
      ofd = open (np, O_WRONLY | O_CREAT, mode);
      if (ofd < 0)
	{
	bad:
	  run_err ("%s: %s", np, strerror (errno));
	  continue;
	}
      write (rem, "", 1);
      bp = allocbuf (&buffer, ofd, BUFSIZ);
      if (bp == NULL)
	{
	  close (ofd);
	  continue;
	}
      cp = bp->buf;
      wrerr = NO;
      for (count = i = 0; i < size; i += BUFSIZ)
	{
	  amt = BUFSIZ;
	  if (i + amt > size)
	    amt = size - i;
	  count += amt;
	  do
	    {
	      j = read (rem, cp, amt);
	      if (j <= 0)
		{
		  run_err ("%s", j ? strerror (errno) : "dropped connection");
		  exit (EXIT_FAILURE);
		}
	      amt -= j;
	      cp += j;
	    }
	  while (amt > 0);
	  if (count == bp->cnt)
	    {
	      /* Keep reading so we stay sync'd up. */
	      if (wrerr == NO)
		{
		  j = write (ofd, bp->buf, count);
		  if (j != count)
		    {
		      wrerr = YES;
		      wrerrno = j >= 0 ? EIO : errno;
		    }
		}
	      count = 0;
	      cp = bp->buf;
	    }
	}
      if (count != 0 && wrerr == NO
	  && (j = write (ofd, bp->buf, count)) != count)
	{
	  wrerr = YES;
	  wrerrno = j >= 0 ? EIO : errno;
	}
      if (ftruncate (ofd, size))
	{
	  run_err ("%s: truncate: %s", np, strerror (errno));
	  wrerr = DISPLAYED;
	}
      if (preserve_option)
	{
	  if (exists || omode != mode)
	    if (fchmod (ofd, omode))
	      run_err ("%s: set mode: %s", np, strerror (errno));
	}
      else
	{
	  if (!exists && omode != mode)
	    if (fchmod (ofd, omode & ~mask))
	      run_err ("%s: set mode: %s", np, strerror (errno));
	}
      close (ofd);
      response ();
      if (setimes && wrerr == NO)
	{
#ifndef HAVE_UTIMES
	  struct utimbuf utbuf;
	  utbuf.actime = atime.tv_sec;
	  utbuf.modtime = mtime.tv_sec;
#endif
	  setimes = 0;
#ifdef HAVE_UTIMES
	  if (utimes (np, tv) < 0)
	    {
#else
	  if (utime (np, &utbuf) < 0)
	    {
#endif
	      run_err ("%s: set times: %s", np, strerror (errno));
	      wrerr = DISPLAYED;
	    }
	}
      switch (wrerr)
	{
	case YES:
	  run_err ("%s: %s", np, strerror (wrerrno));
	  break;

	case NO:
	  write (rem, "", 1);
	  break;

	case DISPLAYED:
	  break;
	}
    }
screwup:
  run_err ("protocol error: %s", why);
  exit (EXIT_FAILURE);
}

#if defined KERBEROS || defined SHISHI
int
kerberos (char **host, char *bp, char *locuser, char *user)
{
  int krb_errno = 0;
  struct servent *sp;

again:
  if (use_kerberos)
    {
      errno = 0;
# ifdef KERBEROS
      rem = KSUCCESS;
      if (dest_realm == NULL)
	dest_realm = krb_realmofhost (*host);
# elif defined SHISHI
      rem = SHISHI_OK;
# endif
# ifdef ENCRYPTION
      if (doencrypt)
	{
#  ifdef KERBEROS
	  rem = krcmd_mutual (host, port, user, bp, 0, dest_realm,
			      &cred, schedule) :
	  krb_errno = errno;
#  elif defined SHISHI
	  int i;
	  char *xbp = NULL;

	  xbp = xmalloc (strlen (bp) + sizeof ("-x "));
	  sprintf (xbp, "%s%s", "-x ", bp);
	  rem = krcmd_mutual (&h, host, port, &user, xbp, NULL,
			      dest_realm, &enckey, family);
	  krb_errno = errno;
	  if (rem > 0)
	    {
	      keytype = shishi_key_type (enckey);
	      keylen = shishi_cipher_blocksize (keytype);

	      ivtab[0] = &iv1;
	      ivtab[1] = &iv2;
	      ivtab[2] = &iv3;
	      ivtab[3] = &iv4;

	      for (i = 0; i < 4; i++)
		{
		  ivtab[i]->ivlen = keylen;

		  switch (keytype)
		    {
		    case SHISHI_DES_CBC_CRC:
		    case SHISHI_DES_CBC_MD4:
		    case SHISHI_DES_CBC_MD5:
		    case SHISHI_DES_CBC_NONE:
		    case SHISHI_DES3_CBC_HMAC_SHA1_KD:
		      ivtab[i]->keyusage = SHISHI_KEYUSAGE_KCMD_DES;
		      ivtab[i]->iv = xmalloc (ivtab[i]->ivlen);
		      memset (ivtab[i]->iv,
			      2 * i + 1 * (i < 2) - 4 * (i >= 2),
			      ivtab[i]->ivlen);
		      ivtab[i]->ctx =
			shishi_crypto (h, enckey, ivtab[i]->keyusage,
				       shishi_key_type (enckey), ivtab[i]->iv,
				       ivtab[i]->ivlen);
		      break;
		    case SHISHI_ARCFOUR_HMAC:
		    case SHISHI_ARCFOUR_HMAC_EXP:
		      ivtab[i]->keyusage =
			SHISHI_KEYUSAGE_KCMD_DES + 2 + 4 * i;
		      ivtab[i]->ctx =
			shishi_crypto (h, enckey, ivtab[i]->keyusage,
				       shishi_key_type (enckey), NULL, 0);
		      break;
		    default:
		      ivtab[i]->keyusage =
			SHISHI_KEYUSAGE_KCMD_DES + 2 + 4 * i;
		      ivtab[i]->iv = xmalloc (ivtab[i]->ivlen);
		      memset (ivtab[i]->iv, 0, ivtab[i]->ivlen);
		      ivtab[i]->ctx =
			shishi_crypto (h, enckey, ivtab[i]->keyusage,
				       shishi_key_type (enckey), ivtab[i]->iv,
				       ivtab[i]->ivlen);
		    }
		}
	    }
	  free (xbp);
#  endif
	}
      else
# endif /* ENCRYPTION */
	{
# ifdef KERBEROS
	  rem = krcmd (host, port, user, bp, 0, dest_realm);
# else /* SHISHI */
	  rem = krcmd (&h, host, port, &user, bp, NULL, dest_realm, family);
# endif
	  krb_errno = errno;
	}

      if (rem < 0)
	{
	  use_kerberos = 0;
	  if (krb_errno == ECONNREFUSED)
	    oldw ("remote host doesn't support Kerberos");
	  else if (krb_errno == ENOENT)
	    error (EXIT_FAILURE, 0, "Can't provide Kerberos authentication data.");
	  else
	    error (EXIT_FAILURE, 0, "Kerberos authentication failed.");

	  sp = getservbyname ("shell", "tcp");
	  if (sp == NULL)
	    error (EXIT_FAILURE, 0, "unknown service shell/tcp");
	  port = sp->s_port;
	  goto again;
	}
    }
  else
    {
      char *p = strchr (*host, '/');

# ifdef ENCRYPTION
      if (doencrypt)
	error (EXIT_FAILURE, 0, "the -x option requires Kerberos authentication");
# endif
      if (p)
	*host = ++p;	/* Skip prefix like `host/'.  */
# ifdef WITH_ORCMD_AF
      rem = orcmd_af (host, port, locuser, user, bp, 0, family);
# elif defined WITH_RCMD_AF
      rem = rcmd_af (host, port, locuser, user, bp, 0, family);
# elif defined WITH_ORCMD
      rem = orcmd (host, port, locuser, user, bp, 0);
# else /* !WITH_ORCMD_AF && !WITH_RCMD_AF && !WITH_ORCMD */
      rem = rcmd (host, port, locuser, user, bp, 0);
# endif
    }
  return rem;
}
#endif /* KERBEROS || SHISHI */

int
response (void)
{
  char ch, *cp, resp, rbuf[BUFSIZ];

  if (read (rem, &resp, sizeof resp) != sizeof resp)
    lostconn (0);

  cp = rbuf;
  switch (resp)
    {
    case 0:			/* ok */
      return 0;

    default:
      *cp++ = resp;
    case 1:			/* error, followed by error msg */
    case 2:			/* fatal error, "" */
      do
	{
	  if (read (rem, &ch, sizeof (ch)) != sizeof (ch))
	    lostconn (0);
	  *cp++ = ch;
	}
      while (cp < &rbuf[BUFSIZ] && ch != '\n');

      if (!iamremote)
	write (STDERR_FILENO, rbuf, cp - rbuf);
      ++errs;
      if (resp == 1)
	return -1;
      exit (EXIT_FAILURE);
    }
}

#if defined KERBEROS || defined SHISHI
void
oldw (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  fprintf (stderr, "rcp: ");
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, ", using standard rcp\n");
  va_end (ap);
}
#endif /* KERBEROS || SHISHI */

void
run_err (const char *fmt, ...)
{
  static FILE *fp;
  va_list ap;

  va_start (ap, fmt);

  ++errs;
  if (fp == NULL && !(fp = fdopen (rem, "w")))
    return;
  fprintf (fp, "%c", 0x01);
  fprintf (fp, "rcp: ");
  vfprintf (fp, fmt, ap);
  va_end (ap);
  fprintf (fp, "\n");
  fflush (fp);

  if (!iamremote)
    {
      fprintf (stderr, "%s: ", program_invocation_name);
      va_start (ap, fmt);
      vfprintf (stderr, fmt, ap);
      va_end (ap);
      fprintf (stderr, "\n");
    }
}

char *
colon (char *cp)
{
  if (*cp == ':')		/* Leading colon is part of file name. */
    return (0);

  for (; *cp; ++cp)
    {
      if (*cp == ':')
	return (cp);
      if (*cp == '/')
	return (0);
    }
  return (0);
}

void
verifydir (char *cp)
{
  struct stat stb;

  if (!stat (cp, &stb))
    {
      if (S_ISDIR (stb.st_mode))
	return;
      errno = ENOTDIR;
    }
  run_err ("%s: %s", cp, strerror (errno));
  exit (EXIT_FAILURE);
}

int
okname (char *cp0)
{
  int c;
  char *cp;

  cp = cp0;
  do
    {
      c = *cp;
      if (c & 0200)
	goto bad;
      if (!isalpha (c) && !isdigit (c) && c != '_' && c != '-')
	goto bad;
    }
  while (*++cp);
  return (1);

bad:
  error (0, 0, "%s: invalid user name", cp0);
  return (0);
}

int
susystem (char *s, int userid)
{
  sighandler_t istat, qstat;
  int status;
  pid_t pid;

  pid = vfork ();
  switch (pid)
    {
    case -1:
      return (127);

    case 0:
      setuid (userid);
      execl (PATH_BSHELL, "sh", "-c", s, NULL);
      _exit (127);
    }
  istat = signal (SIGINT, SIG_IGN);
  qstat = signal (SIGQUIT, SIG_IGN);
  if (waitpid (pid, &status, 0) < 0)
    status = -1;
  signal (SIGINT, istat);
  signal (SIGQUIT, qstat);
  return (status);
}

BUF *
allocbuf (BUF * bp, int fd, int blksize)
{
  struct stat stb;
  size_t size;

  if (fstat (fd, &stb) < 0)
    {
      run_err ("fstat: %s", strerror (errno));
      return (0);
    }
#ifndef roundup
# define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif
  size = roundup (BUFSIZ, blksize);
  if (size == 0)
    size = blksize;
  if ((size_t) bp->cnt >= size)
    return (bp);

  bp->buf = realloc (bp->buf, size);
  if (bp->buf == NULL)
    {
      bp->cnt = 0;
      run_err ("%s", strerror (errno));
      return (0);
    }
  bp->cnt = size;
  return (bp);
}

void
lostconn (int signo _GL_UNUSED_PARAMETER)
{
  if (!iamremote)
    error (0, 0, "lost connection");
  exit (EXIT_FAILURE);
}
