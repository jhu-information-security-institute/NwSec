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
 * Copyright (c) 1983, 1990, 1993, 1994
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
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#include <sys/file.h>
#include <sys/time.h>
#include <time.h>

#include <netinet/in.h>
#include <netdb.h>

#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/select.h>

#include <error.h>
#include <progname.h>
#include <xalloc.h>
#include <argp.h>
#include <libinetutils.h>
#include <unused-parameter.h>

#ifdef KRB5
# ifdef HAVE_KRB5_H
#  include <krb5.h>
# endif
# include "kerberos5_def.h"
#endif /* KRB5 */

#ifdef SHISHI
# include <shishi.h>
# include "shishi_def.h"
#endif

int debug_option = 0;
int null_input_option = 0;
char *user = NULL;
#if defined WITH_ORCMD_AF || defined WITH_RCMD_AF || defined SHISHI
sa_family_t family = AF_UNSPEC;
#endif

#if defined KRB5 || defined SHISHI
int use_kerberos = 1, doencrypt;
const char *dest_realm = NULL;

# ifdef KRB5
krb5_context ctx;
krb5_keyblock *keyblock;
krb5_principal server;

# elif defined(SHISHI)
Shishi *h;
Shishi_key *enckey;
shishi_ivector iv1, iv2, iv3, iv4;
shishi_ivector *ivtab[4];

int keytype;
int keylen;
int rc;
int wlen;
# endif /* SHISHI */
#endif /* KRB5 || SHISHI */

/*
 * rsh - remote shell
 */
int rfd2;
int end_of_pipe = 0;		/* Used by parent process.  */

char *copyargs (char **);
void sendsig (int);
void sigpipe (int);
void talk (int, sigset_t *, pid_t, int);
void warning (const char *, ...);


const char args_doc[] = "[USER@]HOST [COMMAND [ARG...]]";
const char doc[] = "remote shell";

static struct argp_option options[] = {
#define GRP 10
  { "debug", 'd', NULL, 0,
    "turns on socket debugging (see setsockopt(2))", GRP },
  { "user", 'l', "USER", 0,
    "run as USER on the remote system", GRP },
  { "escape", 'e', "CHAR", 0,
    "allows user specification of the escape "
    "character (``~'' by default)", GRP },
  { "8-bit", '8', NULL, 0,
    "allows an eight-bit input data path at all times", GRP },
  { "no-input", 'n', NULL, 0,
    "use /dev/null as input", GRP },
#if defined WITH_ORCMD_AF || defined WITH_RCMD_AF || defined SHISHI
  { "ipv4", '4', NULL, 0, "use only IPv4", GRP },
  { "ipv6", '6', NULL, 0, "use only IPv6", GRP },
#endif
#undef GRP
#if defined KRB5 || defined SHISHI
# define GRP 20
  { "kerberos", 'K', NULL, 0,
    "turns off all Kerberos authentication", GRP },
  { "realm", 'k', "REALM", 0,
    "obtain tickets for a remote host in REALM, "
    "instead of the remote host's default realm", GRP },
# ifdef ENCRYPTION
  { "encrypt", 'x', NULL, 0,
    "encrypt all data transfer", GRP },
# endif /* ENCRYPTION */
# undef GRP
#endif /* KRB5 || SHISHI */
  { NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state _GL_UNUSED_PARAMETER)
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
#endif
    case 'L':		/* -8Lew are ignored to allow rlogin aliases */
    case 'e':
    case 'w':
    case '8':
      break;

    case 'd':
      debug_option = 1;
      break;

    case 'l':
      user = arg;
      break;

#if defined KRB5 || defined SHISHI
    case 'K':
      use_kerberos = 0;
      break;

    case 'k':
      dest_realm = arg;
      break;

# ifdef ENCRYPTION
    case 'x':
      doencrypt = 1;
      break;
# endif
#endif /* KRB5 || SHISHI */

    case 'n':
      null_input_option = 1;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp =
  { options, parse_opt, args_doc, doc, NULL, NULL, NULL };


int
main (int argc, char **argv)
{
  int index;
  struct passwd *pw;
  struct servent *sp;
  sigset_t sigs, osigs;
  int asrsh, rem;
#if defined KRB5 || defined SHISHI
  int krb_errno;
#endif
  pid_t pid = 0;
  uid_t uid;
  char *args, *host;

  set_program_name (argv[0]);

  asrsh = 0;
  host = user = NULL;

  /* If called as something other than "rsh", use it as the host name */
  {
    char *p = strrchr (argv[0], '/');
    if (p)
      ++p;
    else
      p = argv[0];
    if (strcmp (p, "rsh"))
      host = p;
    else
      asrsh = 1;
  }

  /* Parse command line */
  iu_argp_init ("rsh", default_program_authors);
  argp_parse (&argp, argc, argv, ARGP_IN_ORDER, &index, NULL);

  if (index < argc)
    host = argv[index++];

  /* To few args.  */
  if (!host)
    error (EXIT_FAILURE, 0, "host not specified");

  /* If no further arguments, must have been called as rlogin. */
  if (!argv[index])
    {
      if (asrsh)
	*argv = (char *) "rlogin";
      seteuid (getuid ());
      setuid (getuid ());
      execv (PATH_RLOGIN, argv);
      error (EXIT_FAILURE, errno, "cannot execute %s", PATH_RLOGIN);
    }

  argc -= index;
  argv += index;

  uid = getuid ();
  pw = getpwuid (uid);
  if (!pw)
    error (EXIT_FAILURE, 0, "unknown user id");

  /* Accept user1@host format, though "-l user2" overrides user1 */
  {
    char *p = strchr (host, '@');
    if (p)
      {
	*p = '\0';
	if (!user && p > host)
	  user = host;
	host = p + 1;
	if (*host == '\0')
	  error (EXIT_FAILURE, 0, "empty host name");
      }
  }

#if defined KRB5 || defined SHISHI
# ifdef ENCRYPTION
  /* -x turns off -n */
  if (doencrypt)
    null_input_option = 0;
# endif
#endif

  args = copyargs (argv);

  sp = NULL;
#if defined KRB5 || defined SHISHI
  if (use_kerberos)
    {
      sp = getservbyname ("kshell", "tcp");
      if (sp == NULL)
	{
	  use_kerberos = 0;
	  warning ("can't get entry for %s/tcp service", "kshell");
	}
    }
#endif
  if (sp == NULL)
    sp = getservbyname ("shell", "tcp");
  if (sp == NULL)
    error (EXIT_FAILURE, 0, "shell/tcp: unknown service");

#if defined KRB5
  if (use_kerberos)
    {
      rem = krb5_init_context (&ctx);
      if (rem)
	error (EXIT_FAILURE, errno, "Error initializing krb5");
    }
#endif /* KRB5 */

#if defined KRB5 || defined SHISHI
try_connect:
  if (use_kerberos)
    {
# if defined KRB5
      struct hostent *hp;

      /* Get fully qualify hostname for realm determination.  */
      hp = gethostbyname (host);
      if (hp != NULL && !(host = strdup (hp->h_name)))
	error (EXIT_FAILURE, errno, "strdup");

      rem = 0;
      krb_errno = 0;

      if (dest_realm == NULL)
	{
	  krb_errno = krb5_sname_to_principal (ctx, host, SERVICE,
					       KRB5_NT_SRV_HST,
					       &server);
	  if (krb_errno)
	    warning ("cannot assign principal to host %s", host);
	  else
	    dest_realm = krb5_principal_get_realm (ctx, server);
	}
# elif defined SHISHI
      rem = SHISHI_OK;
      krb_errno = 0;
# endif

# ifdef ENCRYPTION
      if (doencrypt)
	{
	  int i;
#  if defined KRB5 || defined SHISHI
	  char *term;

	  term = xmalloc (strlen (args) + 4);
	  strcpy (term, "-x ");
	  strcat (term, args);

#   ifdef SHISHI
	  rem = krcmd_mutual (&h, &host, sp->s_port, &user, term, &rfd2,
			      dest_realm, &enckey, family);
#   else /* KRB5 && !SHISHI */
	  rem = krcmd_mutual (&ctx, &host, sp->s_port, &user, args,
			      &rfd2, dest_realm, &keyblock);
#   endif
	  krb_errno = errno;
	  free (term);

#   ifdef SHISHI
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
#   endif /* SHISHI */
#  endif /* KRB5 || SHISHI */
	}
      else
# endif /* ENCRYPTION */
	{
# if defined SHISHI
	  rem = krcmd (&h, &host, sp->s_port, &user, args, &rfd2,
		       dest_realm, family);
# else /* KRB5 && !SHISHI */
	  rem = krcmd (&ctx, &host, sp->s_port, &user, args,
		       &rfd2, dest_realm);
# endif /* KRB5 */
	  krb_errno = errno;
	}

# ifdef KRB5
      /* No more use of dest_realm.  */
      krb5_free_principal (ctx, server);
# endif

      if (rem < 0)
	{
	  use_kerberos = 0;
	  if (krb_errno == ECONNREFUSED)
	    warning ("remote host doesn't support Kerberos");
	  else if (krb_errno == ENOENT)
	    error (EXIT_FAILURE, 0, "Can't provide Kerberos auth data.");
	  else
	    error (EXIT_FAILURE, 0, "Kerberos authentication failed.");

	  sp = getservbyname ("shell", "tcp");
	  if (sp == NULL)
	    error (EXIT_FAILURE, 0, "shell/tcp: unknown service");
	  goto try_connect;
	}
    }
  else
    {
      char *p = strchr (host, '/');

      if (!user)
	user = pw->pw_name;
      if (doencrypt)
	error (EXIT_FAILURE, 0, "the -x flag requires Kerberos authentication");
      if (p)
	host = ++p;	/* Skip prefix like `host/'.  */

# ifdef WITH_ORCMD_AF
      rem = orcmd_af (&host, sp->s_port, pw->pw_name, user, args, &rfd2, family);
# elif defined WITH_RCMD_AF
      rem = rcmd_af (&host, sp->s_port, pw->pw_name, user, args, &rfd2, family);
# elif defined WITH_ORCMD
      rem = orcmd (&host, sp->s_port, pw->pw_name, user, args, &rfd2);
# else /* !WITH_ORCMD_AF && !WITH_RCMD_AF && !WITH_ORCMD */
      rem = rcmd (&host, sp->s_port, pw->pw_name, user, args, &rfd2);
# endif
    }
#else /* !KRB5 && !SHISHI */
  if (!user)
    user = pw->pw_name;
# ifdef WITH_ORCMD_AF
  rem = orcmd_af (&host, sp->s_port, pw->pw_name, user, args, &rfd2, family);
# elif defined WITH_RCMD_AF
  rem = rcmd_af (&host, sp->s_port, pw->pw_name, user, args, &rfd2, family);
# elif defined WITH_ORCMD
  rem = orcmd (&host, sp->s_port, pw->pw_name, user, args, &rfd2);
# else /* !WITH_ORCMD_AF && !WITH_RCMD_AF && !WITH_ORCMD */
  rem = rcmd (&host, sp->s_port, pw->pw_name, user, args, &rfd2);
# endif
#endif /* !KRB5 && !SHISHI */

  if (rem < 0)
    {
      /* rcmd() provides its own error messages,
       * but we add a vital addition, caused by
       * insufficient capabilites.
       */
      if (errno == EACCES)
	error (EXIT_FAILURE, 0, "No access to privileged ports.");

      exit (EXIT_FAILURE);
    }

  if (rfd2 < 0)
    error (EXIT_FAILURE, 0, "can't establish stderr");

  if (debug_option)
    {
      int one = 1;
      if (setsockopt (rem, SOL_SOCKET, SO_DEBUG, (char *) &one,
		      sizeof one) < 0)
	error (0, errno, "setsockopt DEBUG (ignored)");
      if (setsockopt (rfd2, SOL_SOCKET, SO_DEBUG, (char *) &one,
		      sizeof one) < 0)
	error (0, errno, "setsockopt DEBUG (ignored)");
    }

  seteuid (uid);
  setuid (uid);
#ifdef HAVE_SIGACTION
  sigemptyset (&sigs);
  sigaddset (&sigs, SIGINT);
  sigaddset (&sigs, SIGQUIT);
  sigaddset (&sigs, SIGTERM);
  sigprocmask (SIG_BLOCK, &sigs, &osigs);
#else
  sigs = sigmask (SIGINT) | sigmask (SIGQUIT) | sigmask (SIGTERM);
  osigs = sigblock (sigs);
#endif
  if (signal (SIGINT, SIG_IGN) != SIG_IGN)
    signal (SIGINT, sendsig);
  if (signal (SIGQUIT, SIG_IGN) != SIG_IGN)
    signal (SIGQUIT, sendsig);
  if (signal (SIGTERM, SIG_IGN) != SIG_IGN)
    signal (SIGTERM, sendsig);

  if (null_input_option)
    /* Nothing from stdin will be written to the socket,
     * but we still expect response from the server.
     */
    shutdown (rem, SHUT_WR);
  else
    {
      pid = fork ();
      if (pid < 0)
	error (EXIT_FAILURE, errno, "fork");
    }

#if defined KRB5 || defined SHISHI
# ifdef ENCRYPTION
  if (!doencrypt)
# endif
#endif
    {
      int one = 1;
      ioctl (rfd2, FIONBIO, &one);
      ioctl (rem, FIONBIO, &one);
    }

  talk (null_input_option, &osigs, pid, rem);

#ifdef SHISHI
  if (use_kerberos)
    {
      int i;

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
# endif
    }
#endif

  if (!null_input_option)
    kill (pid, SIGKILL);
  return 0;
}

void
talk (int null_input_option, sigset_t * osigs, pid_t pid, int rem)
{
  int cc, wc;
  fd_set readfrom, ready, rembits;
  char *bp, buf[BUFSIZ];
#ifdef HAVE_SIGACTION
  struct sigaction sa;
#endif

  if (!null_input_option && pid == 0)
    {
      close (rfd2);

    reread:
      errno = 0;
      cc = read (STDIN_FILENO, buf, sizeof buf);
      if (cc <= 0)
	goto done;
      bp = buf;

    rewrite:
      FD_ZERO (&rembits);
      FD_SET (rem, &rembits);
      if (select (rem + 1, 0, &rembits, 0, 0) < 0)
	{
	  if (errno != EINTR)
	    error (EXIT_FAILURE, errno, "select");
	  goto rewrite;
	}
      if (!FD_ISSET (rem, &rembits))
	goto rewrite;
#ifdef ENCRYPTION
# ifdef KERBEROS
      if (doencrypt)
	wc = des_write (rem, bp, cc);
      else
# elif defined(SHISHI)
      if (doencrypt)
	writeenc (h, rem, bp, cc, &wc, &iv3, enckey, 2);
      else
# endif
#endif
	wc = write (rem, bp, cc);
      if (wc < 0)
	{
	  if (errno == EWOULDBLOCK)
	    goto rewrite;
	  goto done;
	}
      bp += wc;
      cc -= wc;
      if (cc == 0)
	goto reread;
      goto rewrite;
    done:
      shutdown (rem, SHUT_WR);
      exit (EXIT_SUCCESS);
    }

#ifdef HAVE_SIGACTION
  sigprocmask (SIG_SETMASK, osigs, NULL);
#else /* !HAVE_SIGACTION */
  sigsetmask (*osigs);
#endif

  /* The access to SIGPIPE is neeeded to kill the child process
   * in an orderly fashion, for example when a command line
   * pipe fails.  Otherwise the child lives on eternally.
   */
#ifdef HAVE_SIGACTION
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = sigpipe;
  (void) sigaction (SIGPIPE, &sa, NULL);
#else /* !HAVE_SIGACTION */
  signal (SIGPIPE, sigpipe);
#endif

  FD_ZERO (&readfrom);
  FD_SET (rfd2, &readfrom);
  FD_SET (rem, &readfrom);
  do
    {
      int maxfd = rem;
      if (rfd2 > maxfd)
	maxfd = rfd2;
      ready = readfrom;
      if (select (maxfd + 1, &ready, 0, 0, 0) < 0)
	{
	  if (errno != EINTR)
	    error (EXIT_FAILURE, errno, "select");
	  continue;
	}
      if (FD_ISSET (rfd2, &ready))
	{
	  errno = 0;
#ifdef KERBEROS
# ifdef ENCRYPTION
	  if (doencrypt)
	    cc = des_read (rfd2, buf, sizeof buf);
	  else
# endif
#elif defined(SHISHI) && defined(ENCRYPTION)
	  if (doencrypt)
	    readenc (h, rfd2, buf, &cc, &iv2, enckey, 2);
	  else
#endif
	    cc = read (rfd2, buf, sizeof buf);
	  if (cc <= 0)
	    {
	      if (errno != EWOULDBLOCK)
		FD_CLR (rfd2, &readfrom);
	    }
	  else
	    write (STDERR_FILENO, buf, cc);
	}
      if (FD_ISSET (rem, &ready))
	{
	  errno = 0;
#ifdef KERBEROS
# ifdef ENCRYPTION
	  if (doencrypt)
	    cc = des_read (rem, buf, sizeof buf);
	  else
# endif
#elif defined(SHISHI) && defined(ENCRYPTION)
	  if (doencrypt)
	    readenc (h, rem, buf, &cc, &iv1, enckey, 2);
	  else
#endif
	    cc = read (rem, buf, sizeof buf);
	  if (cc <= 0)
	    {
	      if (errno != EWOULDBLOCK)
		FD_CLR (rem, &readfrom);
	    }
	  else
	    write (STDOUT_FILENO, buf, cc);
	}
    }
  while ((FD_ISSET (rfd2, &readfrom) || FD_ISSET (rem, &readfrom))
	 && !end_of_pipe);
}

void
sendsig (int sig)
{
  char signo;

#if defined SHISHI && defined ENCRYPTION
  int n;
#endif

  signo = sig;
#ifdef KERBEROS
# ifdef ENCRYPTION
  if (doencrypt)
    des_write (rfd2, &signo, 1);
  else
# endif
#elif defined(SHISHI) && defined (ENCRYPTION)
  if (doencrypt)
    writeenc (h, rfd2, &signo, 1, &n, &iv4, enckey, 2);
  else
#endif

    write (rfd2, &signo, 1);
}

void
sigpipe (int sig _GL_UNUSED_PARAMETER)
{
  ++end_of_pipe;
}

#if defined KERBEROS || defined SHISHI
void
warning (const char *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "%s: warning, using standard rsh: ", program_name);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fprintf (stderr, ".\n");
}
#endif

char *
copyargs (char **argv)
{
  int cc;
  char **ap, *args, *p;

  cc = 0;
  for (ap = argv; *ap; ++ap)
    cc += strlen (*ap) + 1;
  args = malloc ((u_int) cc);
  if (!args)
    error (EXIT_FAILURE, errno, "copyargs");
  for (p = args, ap = argv; *ap; ++ap)
    {
      strcpy (p, *ap);
      for (p = strcpy (p, *ap); *p; ++p);
      if (ap[1])
	*p++ = ' ';
    }
  return args;
}
