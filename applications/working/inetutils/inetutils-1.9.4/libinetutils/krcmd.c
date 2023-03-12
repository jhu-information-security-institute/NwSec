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
 * Copyright (c) 1989, 1993
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

# include <sys/types.h>
# ifdef ENCRYPTION
#  include <sys/socket.h>
# endif

# include <netinet/in.h>

# ifdef KRB5
#  ifdef HAVE_KRB5_H
#   include <krb5.h>
#  endif
#  include "kerberos5_def.h"

# elif defined(SHISHI) /* ! KRB5 */
#  include <shishi.h>
#  include "shishi_def.h"
#  ifdef HAVE_GETPWUID_R
#   include <stdlib.h>
#   include <unistd.h>
#   include <pwd.h>
#  endif /* HAVE_GETPWUID_R */
# endif /* SHISHI && !KRB5 */

# include <stdio.h>

# define SERVICE_NAME	"rcmd"

# if defined SHISHI
int kcmd (Shishi **, int *, char **, unsigned short, char *, char **,
	  char *, int *, char *, const char *, Shishi_key **,
	  struct sockaddr_storage *, struct sockaddr_storage *,
	  long, int);
# else /* KRB5 && !SHISHI */
int kcmd (krb5_context *, int *, char **, unsigned short, char *, char **,
	  char *, int *, char *, const char *, krb5_keyblock **,
	  struct sockaddr_in *, struct sockaddr_in *, long);
# endif /* !SHISHI */

/*
 * krcmd: simplified version of Athena's "kcmd"
 *	returns a socket attached to the destination, -1 or krb error on error
 *	if fd2p is non-NULL, another socket is filled in for it
 */

# if defined SHISHI
#  ifdef HAVE_GETPWUID_R
static int pwbuflen;
static char *pwbuf = NULL;	/* Reused after first allocation.  */
static struct passwd pwstor, *pwd;
#  endif /* HAVE_GETPWUID_R */

int
krcmd (Shishi ** h, char **ahost, unsigned short rport, char **remuser,
       char *cmd, int *fd2p, const char *realm, int af)
{
  int sock = -1, err = 0;
  long authopts = 0L;

#  ifdef HAVE_GETPWUID_R
  if (!pwbuf)
    {
      pwbuflen = sysconf (_SC_GETPW_R_SIZE_MAX);
      if (pwbuflen <= 0)
	pwbuflen = 1024;	/* Guessing only.  */

      pwbuf = malloc (pwbuflen);
    }

  if (pwbuf)
    (void) getpwuid_r (getuid (), &pwstor, pwbuf, pwbuflen, &pwd);
#  endif /* HAVE_GETPWUID_R */

  err = kcmd (h, &sock, ahost, rport,
#  ifdef HAVE_GETPWUID_R
	      pwd ? pwd->pw_name : *remuser,	/* locuser */
#  else /* !HAVE_GETPWUID_R */
	      NULL,		/* locuser not used */
#  endif
	      remuser, cmd, fd2p,
	      SERVICE_NAME, realm,
	      NULL,		/* key schedule not used */
	      NULL,		/* local addr not used */
	      NULL,		/* foreign addr not used */
	      authopts, af);

  if (err > SHISHI_OK)
    {
      fprintf (stderr, "krcmd: error %d, %s\n", err, shishi_strerror (err));
      return (-1);
    }
  if (err < 0)
    return (-1);
  return (sock);
}

# elif defined(KRB5) /* !SHISHI */
int
krcmd (krb5_context *ctx, char **ahost, unsigned short rport,
       char **remuser, char *cmd, int *fd2p, const char *realm)
{
  int sock = -1;
  krb5_error_code err = 0;
  long authopts = 0L;

  err = kcmd (ctx, &sock, ahost, rport,
	      NULL,	/* locuser not used */
	      remuser, cmd, fd2p,
	      SERVICE_NAME, realm,
	      (krb5_keyblock **) NULL,		/* key not used */
	      (struct sockaddr_in *) NULL,	/* local addr not used */
	      (struct sockaddr_in *) NULL,	/* foreign addr not used */
	      authopts);

  if (err > 0)
    {
      const char *text = krb5_get_error_message (*ctx, err);

      fprintf (stderr, "krcmd: %s\n", text);
      krb5_free_error_message (*ctx, text);
      return (-1);
    }
  if (err < 0)
    return (-1);
  return (sock);
}
# endif /* KRB5 && !SHISHI */

# ifdef ENCRYPTION

#  if defined SHISHI
int
krcmd_mutual (Shishi ** h, char **ahost, unsigned short rport, char **remuser,
	      char *cmd, int *fd2p, const char *realm, Shishi_key ** key, int af)
{
  int sock = -1, err = 0;
  struct sockaddr_storage laddr, faddr;
  long authopts = SHISHI_APOPTIONS_MUTUAL_REQUIRED;

#   ifdef HAVE_GETPWUID_R
  if (!pwbuf)
    {
      pwbuflen = sysconf (_SC_GETPW_R_SIZE_MAX);
      if (pwbuflen <= 0)
	pwbuflen = 1024;	/* Guessing only.  */

      pwbuf = malloc (pwbuflen);
    }

  if (pwbuf)
    (void) getpwuid_r (getuid (), &pwstor, pwbuf, pwbuflen, &pwd);
#   endif /* HAVE_GETPWUID_R */

  err = kcmd (h, &sock, ahost, rport,
#   ifdef HAVE_GETPWUID_R
	      pwd ? pwd->pw_name : *remuser,	/* locuser */
#   else /* !HAVE_GETPWUID_R */
	      NULL,		/* locuser not used */
#   endif
	      remuser, cmd, fd2p,
	      SERVICE_NAME, realm,
	      key,		/* filled in */
	      &laddr,		/* filled in */
	      &faddr,		/* filled in */
	      authopts, af);

  if (err > SHISHI_OK)
    {
      fprintf (stderr, "krcmd_mutual: error %d, %s\n",
	       err, shishi_strerror (err));
      return (-1);
    }

  if (err < 0)
    return (-1);
  return (sock);
}

#  elif defined(KRB5) /* !SHISHI */
int
krcmd_mutual (krb5_context *ctx, char **ahost, unsigned short rport,
	      char **remuser, char *cmd, int *fd2p, const char *realm,
	      krb5_keyblock **key)
{
  int sock;
  krb5_error_code err = 0;
  struct sockaddr_in laddr, faddr;
  long authopts = AP_OPTS_MUTUAL_REQUIRED | AP_OPTS_USE_SUBKEY;

  err = kcmd (ctx, &sock, ahost, rport,
	      NULL,		/* locuser not used */
	      remuser, cmd, fd2p,
	      SERVICE_NAME, realm,
	      key,		/* filled in */
	      &laddr,		/* filled in */
	      &faddr,		/* filled in */
	      authopts);

  if (err > 0)
    {
      const char *text = krb5_get_error_message (*ctx, err);

      fprintf (stderr, "krcmd_mutual: %s\n", text);
      krb5_free_error_message (*ctx, text);
      return (-1);
    }

  if (err < 0)
    return (-1);
  return (sock);
}
#  endif /* KRB5 && !SHISHI */
# endif /* ENCRYPTION  */
#endif /* KRB5 || SHISHI */
