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
 * Copyright (c) 1991, 1993
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

/*
 * Copyright (C) 1990 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America is assumed
 * to require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <config.h>

#include "unused-parameter.h"

#if defined AUTHENTICATION
# include <stdio.h>
# include <sys/types.h>
# include <signal.h>
# define AUTH_NAMES
# define AUTHTYPE_NAMES		/* Needed by Solaris.  */
# include <arpa/telnet.h>
# include <stdlib.h>
# ifdef	NO_STRING_H
#  include <strings.h>
# else
#  include <string.h>
# endif

# include <unistd.h>

# include "encrypt.h"
# include "auth.h"
# include "misc-proto.h"
# include "auth-proto.h"

# define typemask(x)		(1<<((x)-1))

/* Callback from consumer.  */
extern void printsub (char, unsigned char *, int);

# ifdef	KRB4_ENCPWD
extern krb4encpwd_init ();
extern krb4encpwd_send ();
extern krb4encpwd_is ();
extern krb4encpwd_reply ();
extern krb4encpwd_status ();
extern krb4encpwd_printsub ();
# endif

# ifdef	RSA_ENCPWD
extern rsaencpwd_init ();
extern rsaencpwd_send ();
extern rsaencpwd_is ();
extern rsaencpwd_reply ();
extern rsaencpwd_status ();
extern rsaencpwd_printsub ();
# endif

int auth_debug_mode = 0;
static char *Name = "Noname";
static int Server = 0;
static TN_Authenticator *authenticated = NULL;
static int authenticating = 0;
static int validuser = 0;
static unsigned char _auth_send_data[256];
static unsigned char *auth_send_data;
static int auth_send_cnt = 0;

/*
 * Authentication types supported.  Plese note that these are stored
 * in priority order, i.e. try the first one first.
 */
TN_Authenticator authenticators[] = {
# ifdef	SPX
  {AUTHTYPE_SPX, AUTH_WHO_CLIENT | AUTH_HOW_MUTUAL,
   spx_init,
   spx_send,
   spx_is,
   spx_reply,
   spx_status,
   spx_printsub},
  {AUTHTYPE_SPX, AUTH_WHO_CLIENT | AUTH_HOW_ONE_WAY,
   spx_init,
   spx_send,
   spx_is,
   spx_reply,
   spx_status,
   spx_printsub},
# endif
# ifdef	SHISHI
  {AUTHTYPE_KERBEROS_V5, AUTH_WHO_CLIENT | AUTH_HOW_MUTUAL,
   krb5shishi_init,
   krb5shishi_send,
   krb5shishi_is,
   krb5shishi_reply,
   krb5shishi_status,
   krb5shishi_printsub,
  },
  {AUTHTYPE_KERBEROS_V5, AUTH_WHO_CLIENT | AUTH_HOW_ONE_WAY,
   krb5shishi_init,
   krb5shishi_send,
   krb5shishi_is,
   krb5shishi_reply,
   krb5shishi_status,
   krb5shishi_printsub,
  },
# endif
# ifdef	KRB5
#  ifdef	ENCRYPTION
  {AUTHTYPE_KERBEROS_V5, AUTH_WHO_CLIENT | AUTH_HOW_MUTUAL,
   kerberos5_init,
   kerberos5_send,
   kerberos5_is,
   kerberos5_reply,
   kerberos5_status,
   kerberos5_printsub},
#  endif /* ENCRYPTION */
  {AUTHTYPE_KERBEROS_V5, AUTH_WHO_CLIENT | AUTH_HOW_ONE_WAY,
   kerberos5_init,
   kerberos5_send,
   kerberos5_is,
   kerberos5_reply,
   kerberos5_status,
   kerberos5_printsub},
# endif
# ifdef	KRB4
#  ifdef ENCRYPTION
  {AUTHTYPE_KERBEROS_V4, AUTH_WHO_CLIENT | AUTH_HOW_MUTUAL,
   kerberos4_init,
   kerberos4_send,
   kerberos4_is,
   kerberos4_reply,
   kerberos4_status,
   kerberos4_printsub},
#  endif /* ENCRYPTION */
  {AUTHTYPE_KERBEROS_V4, AUTH_WHO_CLIENT | AUTH_HOW_ONE_WAY,
   kerberos4_init,
   kerberos4_send,
   kerberos4_is,
   kerberos4_reply,
   kerberos4_status,
   kerberos4_printsub},
# endif
# ifdef	KRB4_ENCPWD
  {AUTHTYPE_KRB4_ENCPWD, AUTH_WHO_CLIENT | AUTH_HOW_MUTUAL,
   krb4encpwd_init,
   krb4encpwd_send,
   krb4encpwd_is,
   krb4encpwd_reply,
   krb4encpwd_status,
   krb4encpwd_printsub},
# endif
# ifdef	RSA_ENCPWD
  {AUTHTYPE_RSA_ENCPWD, AUTH_WHO_CLIENT | AUTH_HOW_ONE_WAY,
   rsaencpwd_init,
   rsaencpwd_send,
   rsaencpwd_is,
   rsaencpwd_reply,
   rsaencpwd_status,
   rsaencpwd_printsub},
# endif
  {0,},
};

static TN_Authenticator NoAuth = { 0 };

static int i_support = 0;
static int i_wont_support = 0;

TN_Authenticator *
findauthenticator (int type, int way)
{
  TN_Authenticator *ap = authenticators;

  while (ap->type && (ap->type != type || ap->way != way))
    ++ap;
  return (ap->type ? ap : 0);
}

void
auth_init (char *name, int server)
{
  TN_Authenticator *ap = authenticators;

  Server = server;
  Name = name;

  i_support = 0;
  authenticated = NULL;
  authenticating = 0;
  while (ap->type)
    {
      if (!ap->init || (*ap->init) (ap, server))
	{
	  i_support |= typemask (ap->type);
	  if (auth_debug_mode)
	    printf (">>>%s: I support auth type %s (%d) %s (%d)\r\n",
		    Name,
		    (AUTHTYPE_NAME_OK (ap->type) && AUTHTYPE_NAME (ap->type))
		    ? AUTHTYPE_NAME (ap->type) : "unknown",
		    ap->type,
		    (ap->way & AUTH_HOW_MASK & AUTH_HOW_MUTUAL)
		    ? "MUTUAL" : "ONEWAY", ap->way);
	}
      else if (auth_debug_mode)
	printf (">>>%s: Init failed: auth type %d %d\r\n",
		Name, ap->type, ap->way);
      ++ap;
    }
}

void
auth_disable_name (char *name)
{
  int x;

  for (x = 0; x < AUTHTYPE_CNT; ++x)
    {
      if (AUTHTYPE_NAME (x) && !strcasecmp (name, AUTHTYPE_NAME (x)))
	{
	  i_wont_support |= typemask (x);
	  break;
	}
    }
}

int
getauthmask (char *type, int *maskp)
{
  register int x;

  if (AUTHTYPE_NAME (0) && !strcasecmp (type, AUTHTYPE_NAME (0)))
    {
      *maskp = -1;
      return (1);
    }

  for (x = 1; x < AUTHTYPE_CNT; ++x)
    {
      if (AUTHTYPE_NAME (x) && !strcasecmp (type, AUTHTYPE_NAME (x)))
	{
	  *maskp = typemask (x);
	  return (1);
	}
    }
  return (0);
}

int auth_onoff (char *, int);

int
auth_enable (char *type)
{
  return (auth_onoff (type, 1));
}

int
auth_disable (char *type)
{
  return (auth_onoff (type, 0));
}

int
auth_onoff (char *type, int on)
{
  int i, mask = -1;
  TN_Authenticator *ap;

  if (!strcasecmp (type, "?") || !strcasecmp (type, "help"))
    {
      printf ("auth %s 'type'\n", on ? "enable" : "disable");
      printf ("Where 'type' is one of:\n");
      printf ("\t%s\n", AUTHTYPE_NAME (0));
      mask = 0;
      for (ap = authenticators; ap->type; ap++)
	{
	  i = typemask (ap->type);
	  if ((mask & i) != 0)
	    continue;
	  mask |= i;
	  printf ("\t%s\n", AUTHTYPE_NAME (ap->type));
	}
      return (0);
    }

  if (!getauthmask (type, &mask))
    {
      printf ("%s: invalid authentication type\n", type);
      return (0);
    }
  if (on)
    i_wont_support &= ~mask;
  else
    i_wont_support |= mask;
  return (1);
}

int
auth_togdebug (int on)
{
  if (on < 0)
    auth_debug_mode ^= 1;
  else
    auth_debug_mode = on;
  printf ("auth debugging %s\n", auth_debug_mode ? "enabled" : "disabled");
  return (1);
}

int
auth_status ()
{
  TN_Authenticator *ap;
  int i, mask;

  if (i_wont_support == -1)
    printf ("Authentication disabled\n");
  else
    printf ("Authentication enabled\n");

  mask = 0;
  for (ap = authenticators; ap->type; ap++)
    {
      i = typemask (ap->type);
      if ((mask & i) != 0)
	continue;
      mask |= i;
      printf ("%s: %s\n", AUTHTYPE_NAME (ap->type),
	      (i_wont_support & typemask (ap->type)) ?
	      "disabled" : "enabled");
    }
  return (1);
}

/*
 * This routine is called by the server to start authentication
 * negotiation.
 */
void
auth_request ()
{
  static unsigned char str_request[64] = { IAC, SB,
    TELOPT_AUTHENTICATION,
    TELQUAL_SEND,
  };
  TN_Authenticator *ap = authenticators;
  unsigned char *e = str_request + 4;

  if (!authenticating)
    {
      authenticating = 1;
      while (ap->type)
	{
	  if (i_support & ~i_wont_support & typemask (ap->type))
	    {
	      if (auth_debug_mode)
		{
		  printf (">>>%s: Sending type %d %d\r\n",
			  Name, ap->type, ap->way);
		}
	      *e++ = ap->type;
	      *e++ = ap->way;
	    }
	  ++ap;
	}
      *e++ = IAC;
      *e++ = SE;
      net_write (str_request, e - str_request);
      printsub ('>', &str_request[2], e - str_request - 2);
    }
}

/*
 * This is called when an AUTH SEND is received.
 * It should never arrive on the server side (as only the server can
 * send an AUTH SEND).
 * You should probably respond to it if you can...
 *
 * If you want to respond to the types out of order (i.e. even
 * if he sends  LOGIN KERBEROS and you support both, you respond
 * with KERBEROS instead of LOGIN (which is against what the
 * protocol says)) you will have to hack this code...
 */
void
auth_send (unsigned char *data, int cnt)
{
  TN_Authenticator *ap;
  static unsigned char str_none[] = { IAC, SB, TELOPT_AUTHENTICATION,
    TELQUAL_IS, AUTHTYPE_NULL, 0,
    IAC, SE
  };
  if (Server)
    {
      if (auth_debug_mode)
	{
	  printf (">>>%s: auth_send called!\r\n", Name);
	}
      return;
    }

  if (auth_debug_mode)
    {
      printf (">>>%s: auth_send got:", Name);
      printd (data, cnt);
      printf ("\r\n");
    }

  /*
   * Save the data, if it is new, so that we can continue looking
   * at it if the authorization we try doesn't work
   */
  if (data < _auth_send_data ||
      data > _auth_send_data + sizeof (_auth_send_data))
    {
      auth_send_cnt = (cnt > (int) sizeof (_auth_send_data))
		      ? (int) sizeof (_auth_send_data) : cnt;
      memmove ((void *) _auth_send_data, (void *) data, auth_send_cnt);
      auth_send_data = _auth_send_data;
    }
  else
    {
      /*
       * This is probably a no-op, but we just make sure
       */
      auth_send_data = data;
      auth_send_cnt = cnt;
    }
  while ((auth_send_cnt -= 2) >= 0)
    {
      if (auth_debug_mode)
	printf (">>>%s: He supports %s (%d) %s (%d)\r\n",
		Name,
		(AUTHTYPE_NAME_OK (auth_send_data[0])
		 && AUTHTYPE_NAME (auth_send_data[0]))
		? AUTHTYPE_NAME (auth_send_data[0]) : "unknown",
		auth_send_data[0],
		(auth_send_data[1] & AUTH_HOW_MASK & AUTH_HOW_MUTUAL)
		? "MUTUAL" : "ONEWAY",
		auth_send_data[1]);
      if ((i_support & ~i_wont_support) & typemask (*auth_send_data))
	{
	  ap = findauthenticator (auth_send_data[0], auth_send_data[1]);
	  if (ap && ap->send)
	    {
	      if (auth_debug_mode)
		printf (">>>%s: Trying %s (%d) %s (%d)\r\n",
			Name,
			(AUTHTYPE_NAME_OK (auth_send_data[0])
			 && AUTHTYPE_NAME (auth_send_data[0]))
			? AUTHTYPE_NAME (auth_send_data[0]) : "unknown",
			auth_send_data[0],
			(auth_send_data[1] & AUTH_HOW_MASK & AUTH_HOW_MUTUAL)
			? "MUTUAL" : "ONEWAY",
			auth_send_data[1]);
	      if ((*ap->send) (ap))
		{
		  /*
		   * Okay, we found one we like
		   * and did it.
		   * we can go home now.
		   */
		  if (auth_debug_mode)
		    printf (">>>%s: Using type %s (%d)\r\n",
			    Name,
			    (AUTHTYPE_NAME_OK (*auth_send_data)
			     && AUTHTYPE_NAME (*auth_send_data))
			    ? AUTHTYPE_NAME (*auth_send_data) : "unknown",
			    *auth_send_data);
		  auth_send_data += 2;
		  return;
		}
	    }
	  /* else
	   *      just continue on and look for the
	   *      next one if we didn't do anything.
	   */
	}
      auth_send_data += 2;
    }
  net_write (str_none, sizeof (str_none));
  printsub ('>', &str_none[2], sizeof (str_none) - 2);
  if (auth_debug_mode)
    printf (">>>%s: Sent failure message\r\n", Name);
  auth_finished (0, AUTH_REJECT);
# ifdef KANNAN
  /*
   *  We requested strong authentication, however no mechanisms worked.
   *  Therefore, exit on client end.
   */
  printf ("Unable to securely authenticate user ... exit\n");
  exit (EXIT_SUCCESS);
# endif	/* KANNAN */
}

void
auth_send_retry ()
{
  /*
   * if auth_send_cnt <= 0 then auth_send will end up rejecting
   * the authentication and informing the other side of this.
   */
  auth_send (auth_send_data, auth_send_cnt);
}

void
auth_is (unsigned char *data, int cnt)
{
  TN_Authenticator *ap;

  if (cnt < 2)
    return;

  if (data[0] == AUTHTYPE_NULL)
    {
      auth_finished (0, AUTH_REJECT);
      return;
    }

  ap = findauthenticator (data[0], data[1]);
  if (ap)
    {
      if (ap->is)
	(*ap->is) (ap, data + 2, cnt - 2);
    }
  else if (auth_debug_mode)
    printf (">>>%s: Invalid authentication in IS: %d\r\n", Name, *data);
}

void
auth_reply (unsigned char *data, int cnt)
{
  TN_Authenticator *ap;

  if (cnt < 2)
    return;

  ap = findauthenticator (data[0], data[1]);
  if (ap)
    {
      if (ap->reply)
	(*ap->reply) (ap, data + 2, cnt - 2);
    }
  else if (auth_debug_mode)
    printf (">>>%s: Invalid authentication in SEND: %d\r\n", Name, *data);
}

void
auth_name (unsigned char *data, int cnt)
{
  char savename[256];

  if (cnt < 1)
    {
      if (auth_debug_mode)
	printf (">>>%s: Empty name in NAME\r\n", Name);
      return;
    }
  if (cnt + 1 > (int) sizeof (savename))
    {
      if (auth_debug_mode)
	printf (">>>%s: Name in NAME (len %d) overflows buffer (len %zu).\r\n",
		Name, cnt, sizeof (savename) - 1);
      return;
    }
  memmove ((void *) savename, (void *) data, cnt);
  savename[cnt] = '\0';		/* Null terminate */
  if (auth_debug_mode)
    printf (">>>%s: Got NAME [%s]\r\n", Name, savename);
  auth_encrypt_user (savename);
}

int
auth_sendname (char *name, int len)
{
  static unsigned char str_request[256 + 6]
    = { IAC, SB, TELOPT_AUTHENTICATION, TELQUAL_NAME, };
  register unsigned char *e = str_request + 4;
  register unsigned char *ee = &str_request[sizeof (str_request) - 2];
  unsigned char *cp = (unsigned char *) name;

  while (--len >= 0)
    {
      if ((*e++ = *cp++) == IAC)
	*e++ = IAC;
      if (e >= ee)
	return (0);
    }
  *e++ = IAC;
  *e++ = SE;
  net_write (str_request, e - str_request);
  printsub ('>', &str_request[2], e - &str_request[2]);
  return (1);
}

void
auth_finished (TN_Authenticator * ap, int result)
{
  if (ap && ap->cleanup)
    (*ap->cleanup) (ap);

  authenticated = ap;
  if (!authenticated)
    authenticated = &NoAuth;

  validuser = result;
}

static void
auth_intr (int sig _GL_UNUSED_PARAMETER)
{
  auth_finished (0, AUTH_REJECT);
}

int
auth_wait (char *name, size_t len)
{
  if (auth_debug_mode)
    printf (">>>%s: in auth_wait.\r\n", Name);

  if (Server && !authenticating)
    return (0);

  signal (SIGALRM, auth_intr);
  alarm (30);
  while (!authenticated)
    if (telnet_spin ())
      break;
  alarm (0);
  signal (SIGALRM, SIG_DFL);

  /*
   * Now check to see if the user is valid or not
   */
  if (!authenticated || authenticated == &NoAuth)
    return (AUTH_REJECT);

  if (validuser == AUTH_VALID)
    validuser = AUTH_USER;

  if (authenticated->status)
    validuser = (*authenticated->status) (authenticated, name, len,
					  validuser);
  return (validuser);
}

void
auth_debug (int mode)
{
  auth_debug_mode = mode;
}

static void
auth_gen_printsub (unsigned char *data, int cnt, char *buf,
		   int buflen)
{
  register char *cp;
  char tbuf[16];

  cnt -= 3;
  data += 3;
  buf[buflen - 1] = '\0';
  buf[buflen - 2] = '*';
  buflen -= 2;
  for (; cnt > 0; cnt--, data++)
    {
      sprintf (tbuf, " %d", *data);
      for (cp = tbuf; *cp && buflen > 0; --buflen)
	*buf++ = *cp++;
      if (buflen <= 0)
	return;
    }
  *buf = '\0';
}

void
auth_printsub (unsigned char *data, int cnt, char *buf, int buflen)
{
  TN_Authenticator *ap;

  ap = findauthenticator (data[1], data[2]);
  if (ap && ap->printsub)
    (*ap->printsub) (data, cnt, buf, buflen);
  else
    auth_gen_printsub (data, cnt, buf, buflen);
}
#endif
