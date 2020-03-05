/*
  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
  2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012,
  2013, 2014, 2015 Free Software Foundation, Inc.

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

/* Implementation of PAM support for a service `rexec'
 * was done by Mats Erik Andersson.
 *
 * Sample PAM configuration with variations for different systems:
 *
 *   rexec auth     requisite  pam_nologin.so
 *   rexec auth     required   pam_unix.so
 *   # rexec auth   requisite  pam_authtok_get.so
 *   # rexec auth   required   pam_unix_cred.so
 *   # rexec auth   required   pam_unix_auth.so
 *   # rexec auth   required   pam_listfile.so item=user sense=allow \
 *                                 file=/etc/rexec.allow onerr=fail
 *
 *   rexec account  required   pam_unix.so
 *   rexec account  required   pam_time.so
 *   # rexec account requisite pam_roles.so
 *   # rexec account required  pam_unix_account.so
 *   # rexec account required  pam_list.so allow=/etc/rexec.allow
 *
 *   rexec password required   pam_deny.so
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>
#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif
#include <sys/select.h>
#include <stdarg.h>
#ifdef HAVE_SHADOW_H
# include <shadow.h>
#endif
#include <syslog.h>
#ifdef HAVE_SECURITY_PAM_APPL_H
# include <security/pam_appl.h>
#endif
#ifdef HAVE_GETPWNAM_R
# include <xalloc.h>
#endif

#include <progname.h>
#include <argp.h>
#include <error.h>
#include <unused-parameter.h>
#include "libinetutils.h"

void die (int code, const char *fmt, ...);
int doit (int f, struct sockaddr *fromp, socklen_t fromlen);

#ifdef WITH_PAM
static int pam_rc = PAM_AUTH_ERR;	/* doit() and die() */
static char *password_pam = NULL;	/* doit() and rexec_conv() */
static int pam_flags = PAM_SILENT | PAM_DISALLOW_NULL_AUTHTOK;

static pam_handle_t *pam_handle = NULL;		/* doit() and die() */
static int rexec_conv (int, const struct pam_message **,
		       struct pam_response **, void *);
static struct pam_conv pam_conv = { rexec_conv, NULL };
#endif /* WITH_PAM */

static int logging = 0;

static struct argp_option options[] = {
  { "logging", 'l', NULL, 0, "logging of requests and errors", 1 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
parse_opt (int key, char *arg _GL_UNUSED_PARAMETER,
	   struct argp_state *state _GL_UNUSED_PARAMETER)
{
  switch (key)
    {
    case 'l':
      ++logging;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

const char doc[] =
#ifdef WITH_PAM
		   "Remote execution daemon, using PAM module 'rexec'.";
#else /* !WITH_PAM */
		   "Remote execution daemon.";
#endif

static struct argp argp = {
  options,
  parse_opt,
  NULL,
  doc,
  NULL,
  NULL,
  NULL
};

/*
 * remote execute server:
 *	username\0
 *	password\0
 *	command\0
 *	data
 */
int
main (int argc, char **argv)
{
  struct sockaddr_storage from;
  socklen_t fromlen;
  int sockfd = STDIN_FILENO;
  int index;

  set_program_name (argv[0]);

  iu_argp_init ("rexecd", default_program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  openlog ("rexecd", LOG_PID | LOG_ODELAY | LOG_NOWAIT, LOG_DAEMON);

  if (argc > index)
    /* Record this complaint locally.  */
    syslog (LOG_NOTICE, "%d extra arguments", argc - index);

  fromlen = sizeof (from);
  if (getpeername (sockfd, (struct sockaddr *) &from, &fromlen) < 0)
    {
      syslog (LOG_ERR, "getpeername: %m");
      error (EXIT_FAILURE, errno, "getpeername");
    }

  doit (sockfd, (struct sockaddr *) &from, fromlen);
  exit (EXIT_SUCCESS);
}

/* Set lengths of USER and LOGNAME longer than
 * the REXEC protocol prescribes.  */
char username[32 + sizeof ("USER=")] = "USER=";
char logname[32 + sizeof ("LOGNAME=")] = "LOGNAME=";	/* Identical to USER.  */
char homedir[256 + sizeof ("HOME=")] = "HOME=";
char shell[256 + sizeof ("SHELL=")] = "SHELL=";
char path[sizeof (PATH_DEFPATH) + sizeof ("PATH=")] = "PATH=";
char remotehost[128 + sizeof ("RHOST=")] = "RHOST=";
#ifndef WITH_PAM
char *envinit[] = { homedir, shell, path, username,
		    logname, remotehost, NULL };
#endif
extern char **environ;

char *getstr (const char *);

#ifndef WITH_PAM
static char *
get_user_password (struct passwd *pwd)
{
  char *pw_text = pwd->pw_passwd;
#ifdef HAVE_SHADOW_H
  struct spwd *spwd = getspnam (pwd->pw_name);
  if (spwd)
    pw_text = spwd->sp_pwdp;
#endif
  return pw_text;
}
#endif /* !WITH_PAM */

int
doit (int f, struct sockaddr *fromp, socklen_t fromlen)
{
  char *cmdbuf, *cp;
  char *user, *pass;
#ifndef WITH_PAM
  char *namep, *pw_password;
#endif
#ifdef HAVE_GETPWNAM_R
  char *pwbuf;
  int pwbuflen;
  struct passwd *pwd, pwstor;
#else /* !HAVE_GETPWNAM_R */
  struct passwd *pwd;
#endif
  char rhost[INET6_ADDRSTRLEN];
  int s, ret;
  in_port_t port;
  int pv[2], pid, cc;
  fd_set readfrom, ready;
  char buf[BUFSIZ], sig;
  int one = 1;

#ifdef HAVE_GETPWNAM_R
  pwbuflen = sysconf (_SC_GETPW_R_SIZE_MAX);
  if (pwbuflen <= 0)
    pwbuflen = 1024;	/* Guessing only.  */

  pwbuf = xmalloc (pwbuflen);
#endif /* HAVE_GETPWNAM_R */

  signal (SIGINT, SIG_DFL);
  signal (SIGQUIT, SIG_DFL);
  signal (SIGTERM, SIG_DFL);
#ifdef DEBUG
  {
    int t = open (PATH_TTY, O_RDWR);
    if (t >= 0)
      {
	ioctl (t, TIOCNOTTY, (char *) 0);
	close (t);
      }
  }
#endif
  if (f != STDIN_FILENO)
    {
      dup2 (f, STDIN_FILENO);
      dup2 (f, STDOUT_FILENO);
      dup2 (f, STDERR_FILENO);
    }

  ret = getnameinfo (fromp, fromlen, rhost, sizeof (rhost),
		     NULL, 0, NI_NUMERICHOST);
  if (ret)
    {
      syslog (LOG_WARNING, "getnameinfo: %m");
      strncpy (rhost, "(unknown)", sizeof (rhost));
    }

  if (logging > 1)
    syslog (LOG_INFO, "request from \"%s\"", rhost);

  alarm (60);
  port = 0;
  for (;;)
    {
      char c;
      if (read (f, &c, 1) != 1)
	{
	  if (logging)
	    syslog (LOG_ERR, "main socket: %m");
	  exit (EXIT_FAILURE);
	}
      if (c == 0)
	break;
      port = port * 10 + c - '0';
    }
  alarm (0);

  if (port != 0)
    {
      s = socket (fromp->sa_family, SOCK_STREAM, 0);
      if (s < 0)
	{
	  if (logging)
	    syslog (LOG_ERR, "stderr socket: %m");
	  exit (EXIT_FAILURE);
	}
      setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));
      alarm (60);
      switch (fromp->sa_family)
	{
	case AF_INET:
	  ((struct sockaddr_in *) fromp)->sin_port = htons (port);
	  break;
	case AF_INET6:
	  ((struct sockaddr_in6 *) fromp)->sin6_port = htons (port);
	  break;
	default:
	  syslog (LOG_ERR, "unknown address family %d", fromp->sa_family);
	  exit (EXIT_FAILURE);
	}
      if (connect (s, fromp, fromlen) < 0)
	{
	  /* Use LOG_NOTICE since the remote part might cause
	   * this error by blocking.  We are less probable.
	   */
	  if (logging)
	    syslog (LOG_NOTICE, "connect: %m");
	  exit (EXIT_FAILURE);
	}
      alarm (0);
    }

  user = getstr ("username");
  pass = getstr ("password");
  cmdbuf = getstr ("command");

  setpwent ();

#ifdef HAVE_GETPWNAM_R
  ret = getpwnam_r (user, &pwstor, pwbuf, pwbuflen, &pwd);
  if (ret || pwd == NULL)
#else /* !HAVE_GETPWNAM_R */
  pwd = getpwnam (user);
  if (pwd == NULL)
#endif /* HAVE_GETPWNAM_R */
    {
      if (logging)
	syslog (LOG_WARNING | LOG_AUTH, "no user named \"%s\"", user);
      die (EXIT_FAILURE, "Login incorrect.");
    }

  endpwent ();

#ifndef WITH_PAM
  /* Last need of elevated privilege.  */
  pw_password = get_user_password (pwd);
  if (*pw_password != '\0')
    {
      namep = crypt (pass, pw_password);
      if (strcmp (namep, pw_password))
	{
	  if (logging)
	    syslog (LOG_WARNING | LOG_AUTH, "password failure for \"%s\"", user);
	  die (EXIT_FAILURE, "Password incorrect.");
	}
    }
#else /* WITH_PAM */
  /* Failure at this stage should not disclose server side causes,
   * but only fail almost silently.  Use "Try again" for now.
   */
  password_pam = pass;		/* Needed by pam_conv().  */

  pam_rc = pam_start ("rexec", user, &pam_conv, &pam_handle);
  if (pam_rc != PAM_SUCCESS)
    die (EXIT_FAILURE, "Try again.");

  pam_rc = pam_set_item (pam_handle, PAM_RHOST, rhost);
  if (pam_rc != PAM_SUCCESS)
    die (EXIT_FAILURE, "Try again.");

  pam_rc = pam_authenticate (pam_handle, pam_flags);
  if (pam_rc != PAM_SUCCESS)
    {
      switch (pam_rc)
	{
	case PAM_ABORT:
	  /* This is potentially severe.  No communication!  */
	  pam_end (pam_handle, pam_rc);
	  exit (EXIT_FAILURE);
	  break;
	case PAM_AUTHINFO_UNAVAIL:
	case PAM_CRED_INSUFFICIENT:
	case PAM_USER_UNKNOWN:
	  die (EXIT_FAILURE, "Login incorrect.");
	  break;
	case PAM_AUTH_ERR:
	case PAM_MAXTRIES:
	default:
	  die (EXIT_FAILURE, "Password incorrect.");
	  break;
	}
    }

  pam_rc = pam_acct_mgmt (pam_handle, pam_flags);
  if (pam_rc != PAM_SUCCESS)
    {
      switch (pam_rc)
	{
	case PAM_NEW_AUTHTOK_REQD:
	case PAM_PERM_DENIED:
	  die (EXIT_FAILURE, "Password incorrect.");
	  break;
	case PAM_ACCT_EXPIRED:
	case PAM_AUTH_ERR:
	case PAM_USER_UNKNOWN:
	default:
	  die (EXIT_FAILURE, "Login incorrect.");
	  break;
	}
    }
#endif /* WITH_PAM */

#ifdef HAVE_SETLOGIN
  if (setlogin (pwd->pw_name) < 0)
    syslog (LOG_ERR, "setlogin() failed: %m");
#endif

  /* Step down from superuser personality.
   *
   * The changing of group membership will seldomly
   * fail, but a relevant message is passed just in
   * case.  These messages are non-standard.
   */
  if (setgid ((gid_t) pwd->pw_gid) < 0)
    {
#ifdef WITH_PAM
      pam_rc = PAM_ABORT;
#endif
      syslog (LOG_DEBUG | LOG_AUTH, "setgid(gid = %d): %m", pwd->pw_gid);
      die (EXIT_FAILURE, "Failed group protections.");
    }

#ifdef HAVE_INITGROUPS
  if (initgroups (pwd->pw_name, pwd->pw_gid) < 0)
    {
# ifdef WITH_PAM
      pam_rc = PAM_ABORT;
# endif
      syslog (LOG_DEBUG | LOG_AUTH, "initgroups(%s, %d): %m",
	      pwd->pw_name, pwd->pw_gid);
      die (EXIT_FAILURE, "Failed group protections.");
    }
#endif

#ifdef WITH_PAM
  pam_rc = pam_setcred (pam_handle, PAM_SILENT | PAM_ESTABLISH_CRED);
  if (pam_rc != PAM_SUCCESS)
    {
      syslog (LOG_ERR | LOG_AUTH, "pam_setcred: %s",
	      pam_strerror (pam_handle, pam_rc));
      pam_rc = PAM_SUCCESS;	/* Only report the above anomaly.  */
    }
#endif /* WITH_PAM */

  if (setuid ((uid_t) pwd->pw_uid) < 0)
    {
#ifdef WITH_PAM
      pam_rc = PAM_ABORT;
#endif
      syslog (LOG_DEBUG | LOG_AUTH, "setuid(uid = %d): %m", pwd->pw_uid);
      die (EXIT_FAILURE, "Failed user identity.");
    }

  if (port)
    {
      pipe (pv);
      pid = fork ();
      if (pid == -1)
	{
	  if (logging)
	    syslog (LOG_ERR, "forking for \"%s\": %m", user);
#ifdef WITH_PAM
	  pam_rc = PAM_ABORT;
#endif
	  die (EXIT_FAILURE, "Try again.");
	}

      if (pid)
	{
#ifdef WITH_PAM
	  /* This process steps down from PAM now,
	   * the child continues communication.  */
	  pam_handle = NULL;
# ifdef PAM_DATA_SILENT
	  pam_rc |= PAM_DATA_SILENT;
# endif
#endif /* WITH_PAM */
	  close (STDIN_FILENO);
	  close (STDOUT_FILENO);
	  close (STDERR_FILENO);
	  close (f);
	  close (pv[1]);
	  FD_ZERO (&readfrom);
	  FD_SET (s, &readfrom);
	  FD_SET (pv[0], &readfrom);
	  ioctl (pv[1], FIONBIO, (char *) &one);
	  /* should set s nbio! */
	  do
	    {
	      int maxfd = s;
	      ready = readfrom;
	      if (pv[0] > maxfd)
		maxfd = pv[0];
	      select (maxfd + 1, (fd_set *) & ready,
		      (fd_set *) NULL, (fd_set *) NULL,
		      (struct timeval *) NULL);
	      if (FD_ISSET (s, &ready))
		{
		  if (read (s, &sig, 1) <= 0)
		    FD_CLR (s, &readfrom);
		  else
		    killpg (pid, sig);
		}
	      if (FD_ISSET (pv[0], &ready))
		{
		  cc = read (pv[0], buf, sizeof (buf));
		  if (cc <= 0)
		    {
		      shutdown (s, 1 + 1);
		      FD_CLR (pv[0], &readfrom);
		    }
		  else
		    write (s, buf, cc);
		}
	    }
	  while (FD_ISSET (pv[0], &readfrom) || FD_ISSET (s, &readfrom));
	  exit (EXIT_SUCCESS);
	} /* Parent process.  */

#ifdef HAVE_SETLOGIN
      /* Not sufficient to call setpgid() on BSD systems.  */
      if (setsid () < 0)
	syslog (LOG_ERR, "setsid() failed: %m");
#elif defined HAVE_SETPGID /* !HAVE_SETLOGIN */
      setpgid (0, getpid ());
#endif

      close (s);
      close (pv[0]);
      dup2 (pv[1], STDERR_FILENO);
      close (pv[1]);
    }

  if (f > 2)
    close (f);

  /* Last point of failure due to incorrect user settings.  */
  if (chdir (pwd->pw_dir) < 0)
    {
      if (logging)
	syslog (LOG_NOTICE | LOG_AUTH, "\"%s\" uses invalid \"%s\"",
		user, pwd->pw_dir);
#ifdef WITH_PAM
      pam_rc = PAM_ABORT;
#endif
      die (EXIT_FAILURE, "No remote directory.");
    }

#ifdef WITH_PAM
  /* Refresh knowledge of user, which might have been
   * remapped by the PAM stack during conversation.
   */
  free (user);
  pam_rc = pam_get_item (pam_handle, PAM_USER, (const void **) &user);
  if (pam_rc != PAM_SUCCESS)
    die (EXIT_FAILURE, "Try again.");

# ifdef HAVE_GETPWNAM_R
  ret = getpwnam_r (user, &pwstor, pwbuf, pwbuflen, &pwd);
  if (ret || pwd == NULL)
# else /* !HAVE_GETPWNAM_R */
  pwd = getpwnam (user);
  if (pwd == NULL)
# endif /* HAVE_GETPWNAM_R */
    {
      syslog (LOG_ERR | LOG_AUTH, "no user named \"%s\"", user);
      die (EXIT_FAILURE, "Login incorrect.");
    }
#endif /* WITH_PAM */

  strcat (path, PATH_DEFPATH);
  strncat (homedir, pwd->pw_dir, sizeof (homedir) - sizeof ("HOME=") - 1);
  strncat (shell, pwd->pw_shell, sizeof (shell) - sizeof ("SHELL=") - 1);
  strncat (username, pwd->pw_name, sizeof (username) - sizeof ("USER=") - 1);
  strncat (logname, pwd->pw_name, sizeof (logname) - sizeof ("LOGNAME=") - 1);
  strncat (remotehost, rhost, sizeof (remotehost) - sizeof ("RHOST=") - 1);

#ifdef WITH_PAM
  if (pam_getenv (pam_handle, "PATH") == NULL)
    (void) pam_putenv (pam_handle, path);
  if (pam_getenv (pam_handle, "HOME") == NULL)
    (void) pam_putenv (pam_handle, homedir);
  if (pam_getenv (pam_handle, "SHELL") == NULL)
    (void) pam_putenv (pam_handle, shell);
  if (pam_getenv (pam_handle, "USER") == NULL)
    (void) pam_putenv (pam_handle, username);
  if (pam_getenv (pam_handle, "LOGNAME") == NULL)
    (void) pam_putenv (pam_handle, logname);
  if (pam_getenv (pam_handle, "RHOST") == NULL)
    (void) pam_putenv (pam_handle, remotehost);

  environ = pam_getenvlist (pam_handle);
#else /* !WITH_PAM */
  environ = envinit;
#endif /* !WITH_PAM */

  if (*pwd->pw_shell == '\0')
    pwd->pw_shell = PATH_BSHELL;

  cp = strrchr (pwd->pw_shell, '/');
  if (cp)
    cp++;
  else
    cp = pwd->pw_shell;

  /* This is the 7th step in the protocol standard.
   * All authentication has been successful, and the
   * execution can be handed over to the requested shell.
   */
  write (STDERR_FILENO, "\0", 1);
  if (logging)
    syslog (LOG_INFO, "accepted user \"%s\" from %s", user, rhost);

  execl (pwd->pw_shell, cp, "-c", cmdbuf, NULL);
  if (logging)
    syslog (LOG_ERR, "execl fails for \"%s\": %m", user);
#ifdef WITH_PAM
  pam_end (pam_handle, PAM_SUCCESS);
#endif
  error (EXIT_FAILURE, errno, "executing %s", pwd->pw_shell);

  return -1;
}

void
die (int code, const char *fmt, ...)
{
  va_list ap;
  char buf[BUFSIZ];
  int n;

  va_start (ap, fmt);
  buf[0] = 1;		/* Error condition.  */
  n = vsnprintf (buf + 1, sizeof buf - 1, fmt, ap);
  va_end (ap);
  if (n + 1 > (int) sizeof buf)
    n = sizeof buf - 1;
  buf[n++] = '\n';
  write (STDERR_FILENO, buf, n);
#ifdef WITH_PAM
  if (pam_handle != NULL)
    pam_end (pam_handle, pam_rc);
#endif
  exit (code);
}

char *
getstr (const char *err)
{
  size_t buf_len = 100;
  char *buf = malloc (buf_len), *end = buf;

  if (!buf)
    die (EXIT_FAILURE, "Out of space reading %s", err);

  do
    {
      /* Oh this is efficient, oh yes.  [But what can be done?] */
      int rd = read (STDIN_FILENO, end, 1);
      if (rd <= 0)
	{
	  if (rd == 0)
	    die (EXIT_FAILURE, "EOF reading %s", err);
	  else
	    error (EXIT_FAILURE, 0, "%s", err);
	}

      end += rd;
      if ((buf + buf_len - end) < (ssize_t) (buf_len >> 3))
	{
	  /* Not very much room left in our buffer, grow it. */
	  size_t end_offs = end - buf;
	  buf_len += buf_len;
	  buf = realloc (buf, buf_len);
	  if (!buf)
	    die (EXIT_FAILURE, "Out of space reading %s", err);
	  end = buf + end_offs;
	}
    }
  while (*(end - 1));

  return buf;
}

#ifdef WITH_PAM
/* Call back function for passing user's password
 * to any PAM module requesting this information.
 */
static int
rexec_conv (int num, const struct pam_message **pam_msg,
	    struct pam_response **pam_resp,
	    void *data _GL_UNUSED_PARAMETER)
{
  struct pam_response *resp;

  /* Reject composite call-backs.  */
  if (num <= 0 || num > 1)
    return PAM_CONV_ERR;

  /* We only accept password reporting.  */
  if (pam_msg[0]->msg_style != PAM_PROMPT_ECHO_OFF)
    return PAM_CONV_ERR;

  /* Allocate a single response structure,
   * as we are ignoring composite calls.
   *
   * This is an external call-back, so we check
   * for successful allocation ourselves.
   */
  resp = malloc (sizeof (*resp));
  if (resp == NULL)
    return PAM_BUF_ERR;

  /* Prepare response to a single PAM_PROMPT_ECHO_OFF.  */
  resp->resp_retcode = 0;
  resp->resp = strdup (password_pam);
  if (resp->resp == NULL)
    {
      free (resp);
      return PAM_BUF_ERR;
    }

  *pam_resp = resp;

  return PAM_SUCCESS;
}
#endif /* WITH_PAM */
