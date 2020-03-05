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

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pwd.h>
#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif
#ifdef HAVE_SHADOW_H
# include <shadow.h>
#endif
#include <sys/time.h>
#include <time.h>

#include "extern.h"


/* If name is "ftp" or "anonymous", the name is not in
   PATH_FTPUSERS, and ftp account exists, set cred, then just return.
   If account doesn't exist, ask for passwd anyway.  Otherwise, check user
   requesting login privileges.  Disallow anyone who does not have a standard
   shell as returned by getusershell().  Disallow anyone mentioned in the file
   PATH_FTPUSERS to allow people such as root and uucp to be avoided.  */

int
auth_user (const char *name, struct credentials *pcred)
{
  int err = 0;		/* Never remove initialisation!  */

  pcred->guest = 0;
  pcred->expired = AUTH_EXPIRED_NOT;

  switch (pcred->auth_type)
    {
#ifdef WITH_LINUX_PAM
    case AUTH_TYPE_PAM:
      err = pam_user (name, pcred);
      break;
#endif
#ifdef WITH_KERBEROS
    case AUTH_TYPE_KERBEROS:
      err = -1;
      break;
#endif
#ifdef WITH_KERBEROS5
    case AUTH_TYPE_KERBEROS5:
      err = -1;
      break;
#endif
#ifdef WITH_OPIE
    case AUTH_TYPE_OPIE:
      err = -1;
      break;
#endif
    case AUTH_TYPE_PASSWD:
    default:
      {
	size_t len;
	free (pcred->message);
	len = (size_t) (64 + strlen (name));
	pcred->message = malloc (len);
	if (pcred->message == NULL)
	  return -1;

	/* Check for anonymous log in.
	 *
	 * This code simulates part of `pam_ftp.so'
	 * for PAM variants that are not Linux-PAM,
	 * in addition to perform the original
	 * default authentication checks.
	 */
	if (strcmp (name, "ftp") == 0 || strcmp (name, "anonymous") == 0)
	  {
	    if (checkuser (PATH_FTPUSERS, "ftp")
		|| checkuser (PATH_FTPUSERS, "anonymous"))
	      {
		snprintf (pcred->message, len, "User %s access denied.",
			  name);
		err = 1;
	      }
	    else if (sgetcred ("ftp", pcred) == 0)
	      {
		pcred->guest = 1;
		strcpy (pcred->message,
			"Guest login ok, type your name as password.");
	      }
	    else
	      {
		snprintf (pcred->message, len, "User %s unknown.", name);
		err = 1;
	      }
	    return err;
	  }

	if (sgetcred (name, pcred) == 0)
	  {
	    const char *cp;
	    const char *shell;

	    /* Check if the shell is allowed */
	    shell = pcred->shell;
	    if (shell == NULL || *shell == 0)
	      shell = PATH_BSHELL;
	    setusershell ();
	    while ((cp = getusershell ()) != NULL)
	      if (strcmp (cp, shell) == 0)
		break;
	    endusershell ();

	    if (cp == NULL || checkuser (PATH_FTPUSERS, name))
	      {
		sprintf (pcred->message, "User %s access denied.", name);
		return 1;
	      }
	  }
	else
	  {
	    free (pcred->message);
	    pcred->message = NULL;
	    return 1;
	  }
	snprintf (pcred->message, len,
		  "Password required for %s.", pcred->name);
	err = 0;
      }
    }

  if (err == 0)
    {
      pcred->dochroot = checkuser (PATH_FTPCHROOT, pcred->name);

#if defined WITH_PAM && !defined WITH_LINUX_PAM
      if (pcred->auth_type == AUTH_TYPE_PAM)
	err = pam_user (name, pcred);
#endif /* WITH_PAM && !WITH_LINUX_PAM */
    }

  return err;
}

int
auth_pass (const char *passwd, struct credentials *pcred)
{
  switch (pcred->auth_type)
    {
#ifdef WITH_PAM
    case AUTH_TYPE_PAM:
      return pam_pass (passwd, pcred);
#endif
#ifdef WITH_KERBEROS
    case AUTH_TYPE_KERBEROS:
      return -1;
#endif
#ifdef WITH_KERBEROS5
    case AUTH_TYPE_KERBEROS5:
      return -1;
#endif
#ifdef WITH_OPIE
    case AUTH_TYPE_OPIE:
      return -1;
#endif
    case AUTH_TYPE_PASSWD:
    default:
      {
	char *xpasswd;
	char *salt = pcred->passwd;
	/* Try to authenticate the user.  */
	if (pcred->passwd == NULL || *pcred->passwd == '\0')
	  return 1;		/* Failed. */
	xpasswd = crypt (passwd, salt);
	return (!xpasswd || strcmp (xpasswd, pcred->passwd) != 0);
      }
    }				/* switch (auth_type) */
  return -1;
}

int
sgetcred (const char *name, struct credentials *pcred)
{
  struct passwd *p;

  p = getpwnam (name);
  if (p == NULL)
    return 1;

  free (pcred->name);
  free (pcred->passwd);
  free (pcred->homedir);
  free (pcred->rootdir);
  free (pcred->shell);

#if defined HAVE_GETSPNAM && defined HAVE_SHADOW_H
  if (p->pw_passwd == NULL || strlen (p->pw_passwd) == 1)
    {
      struct spwd *spw;

      setspent ();
      spw = getspnam (p->pw_name);
      if (spw != NULL)
	{
	  time_t now;
	  long today;
	  now = time ((time_t *) 0);
	  today = now / (60 * 60 * 24);

	  if (spw->sp_expire > 0 && spw->sp_expire < today)
	    {
	      p->pw_passwd = NULL;
	      pcred->expired |= AUTH_EXPIRED_ACCT;
	    }
	  if (spw->sp_max > 0 && spw->sp_lstchg > 0
		   && (spw->sp_lstchg + spw->sp_max < today))
	    {
	      p->pw_passwd = NULL;
	      pcred->expired |= AUTH_EXPIRED_PASS;
	    }

	  if (pcred->expired == AUTH_EXPIRED_NOT)
	    p->pw_passwd = spw->sp_pwdp;
	}
      endspent ();
    }
#elif defined HAVE_STRUCT_PASSWD_PW_EXPIRE	/* !HAVE_SHADOW_H */
  /* BSD systems provide pw_expire as epoch time,
   * and the password is exposed in pw_passwd for
   * a caller with euid 0.
   *
   * NetBSD allows -1 for 'pw_change', meaning that immediate
   * change is required.  Let us deny access in that case..
   */
  if (p->pw_expire > 0
# ifdef HAVE_STRUCT_PASSWD_PW_CHANGE
      || p->pw_change
# endif
     )
    {
      time_t now = time ((time_t *) 0);

      if (p->pw_expire > 0 && difftime (p->pw_expire, now) < 0)
	{
	  p->pw_passwd = NULL;
	  pcred->expired |= AUTH_EXPIRED_ACCT;
	}
# ifdef HAVE_STRUCT_PASSWD_PW_CHANGE
      if (p->pw_change && difftime (p->pw_change, now) < 0)
	{
	  p->pw_passwd = NULL;
	  pcred->expired |= AUTH_EXPIRED_PASS;
	}
# endif
    }
#endif /* !HAVE_STRUCT_PASSWD_PW_EXPIRE */

  pcred->uid = p->pw_uid;
  pcred->gid = p->pw_gid;
  pcred->name = sgetsave (p->pw_name);
  pcred->passwd = sgetsave (p->pw_passwd);
  pcred->rootdir = sgetsave (p->pw_dir);
  pcred->homedir = sgetsave ("/");
  pcred->shell = sgetsave (p->pw_shell);

  return 0;
}
