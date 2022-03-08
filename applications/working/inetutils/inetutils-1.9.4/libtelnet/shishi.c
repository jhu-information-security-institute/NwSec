/*
  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011,
  2012, 2013, 2014, 2015 Free Software Foundation, Inc.

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

/* Written by Simon Josefsson and Nicolas Pouvesle, based on
   kerberos5.c from GNU Inetutils. */

#include <config.h>

#ifdef SHISHI
# include <stdlib.h>
# include <stdio.h>
# include <arpa/telnet.h>
# include <shishi.h>

# include <netdb.h>
# include <ctype.h>
# include <syslog.h>
# include <string.h>
# include <unused-parameter.h>

# include "auth.h"
# include "misc.h"

# ifdef  ENCRYPTION
#  include "encrypt.h"
# endif

char *dest_realm = NULL;

Shishi_key *enckey = NULL;

static unsigned char str_data[2048] = { IAC, SB, TELOPT_AUTHENTICATION, 0,
  AUTHTYPE_KERBEROS_V5,
};

# define KRB_AUTH             0	/* Authentication data follows */
# define KRB_REJECT           1	/* Rejected (reason might follow) */
# define KRB_ACCEPT           2	/* Accepted */
# define KRB_RESPONSE         3	/* Response for mutual auth. */

Shishi *shishi_handle = NULL;
Shishi_ap *auth_handle;

# define DEBUG(c) if (auth_debug_mode) printf c

/* Callback from consumer.  */
extern void printsub (char, unsigned char *, int);

static int
Data (TN_Authenticator * ap, int type, void * d, int c)
{
  unsigned char *p = str_data + 4;
  unsigned char *cd = (unsigned char *) d;

  /* Submitted as test data.  */
  if (c == -1)
    c = strlen ((char *) cd);

  if (auth_debug_mode)
    {
      printf ("%s:%d: [%d] (%d)",
	      str_data[3] == TELQUAL_IS ? ">>>IS" : ">>>REPLY",
	      str_data[3], type, c);
      printd (d, c);
      printf ("\r\n");
    }

  *p++ = ap->type;
  *p++ = ap->way;
  *p++ = type;

  while (c-- > 0)
    {
      if ((*p++ = *cd++) == IAC)
	*p++ = IAC;
    }
  *p++ = IAC;
  *p++ = SE;
  if (str_data[3] == TELQUAL_IS)
    printsub ('>', &str_data[2], p - &str_data[2]);
  return (net_write (str_data, p - str_data));
}

Shishi *shishi_telnet = NULL;

/* FIXME: Reverse return code! */
int
krb5shishi_init (TN_Authenticator * ap _GL_UNUSED_PARAMETER,
		 int server)
{
  if (server)
    str_data[3] = TELQUAL_REPLY;
  else
    str_data[3] = TELQUAL_IS;

  if (!shishi_check_version (SHISHI_VERSION))
    return 0;

  return 1;
}

static int
delayed_shishi_init (void)
{
  if (shishi_handle)
    return 1;

  if (str_data[3] == TELQUAL_REPLY)
    {
      if (!shishi_handle && shishi_init_server (&shishi_handle) != SHISHI_OK)
	return 0;
    }
  else
    {
      if (!shishi_handle && shishi_init (&shishi_handle) != SHISHI_OK)
	return 0;
    }

  return 1;
}

void
krb5shishi_cleanup (TN_Authenticator * ap _GL_UNUSED_PARAMETER)
{
  if (shishi_handle == NULL)
    return;

  shishi_done (shishi_handle);
  shishi_handle = NULL;
}

int
krb5shishi_send (TN_Authenticator * ap)
{
  int ap_opts;
  char type_check[2];
  Shishi_tkt *tkt;
  Shishi_tkts_hint hint;
  int rc;
  char *tmp;
  char *apreq;
  size_t apreq_len;

  if (!UserNameRequested)
    {
      DEBUG (("telnet: Kerberos V5: no user name supplied\r\n"));
      return 0;
    }

  if (!delayed_shishi_init ())
    {
      DEBUG (("telnet: Kerberos V5: shishi initialization failed\r\n"));
      return 0;
    }

  tmp = malloc (strlen ("host/") + strlen (RemoteHostName) + 1);
  if (tmp == NULL)
    {
      DEBUG (("telnet: Kerberos V5: shishi memory allocation failed\r\n"));
      return 0;
    }

  /* Check for Kerberos prefix in principal name.  */
  if (strchr (RemoteHostName, '/'))
    strcpy (tmp, RemoteHostName);
  else
    sprintf (tmp, "host/%s", RemoteHostName);

  memset (&hint, 0, sizeof (hint));
  hint.server = tmp;
  hint.client = UserNameRequested;

  if (dest_realm && *dest_realm)
    shishi_realm_default_set (shishi_handle, dest_realm);
  else
    {
      /* Retrieve realm assigned to this server as per configuration.  */
      char *p = strchr (RemoteHostName, '/');

      if (p)
	++p;
      else
	p = RemoteHostName;

      shishi_realm_default_set (shishi_handle,
				shishi_realm_for_server (shishi_handle, p));
    }

  tkt = shishi_tkts_get (shishi_tkts_default (shishi_handle), &hint);
  free (tmp);
  if (!tkt)
    {
      DEBUG (("telnet: Kerberos V5: no shishi ticket for server\r\n"));
      return 0;
    }

  if (auth_debug_mode)
    shishi_tkt_pretty_print (tkt, stdout);

  if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL)
    ap_opts = SHISHI_APOPTIONS_MUTUAL_REQUIRED;
  else
    ap_opts = 0;

  type_check[0] = ap->type;
  type_check[1] = ap->way;

# ifndef DONT_ALWAYS_USE_DES
  /* Even if we are not using a DES key, we can still try a DES
     session-key.  Then we can support DES_?FB64 encryption with 3DES
     or AES keys against non-RFC 2952 implementations.  Of course, it
     would be better to follow RFC 2952, but enc_des.c does not
     support this (see comment in that file). */
  if (shishi_tkt_keytype_fast (tkt) != SHISHI_DES_CBC_CRC &&
      shishi_tkt_keytype_fast (tkt) != SHISHI_DES_CBC_MD4 &&
      shishi_tkt_keytype_fast (tkt) != SHISHI_DES_CBC_MD5)
    rc = shishi_ap_etype_tktoptionsdata (shishi_handle, &auth_handle,
					 SHISHI_DES_CBC_MD5,
					 tkt, ap_opts,
					 (char *) &type_check, 2);
  else
# endif
    rc = shishi_ap_tktoptionsdata (shishi_handle, &auth_handle,
				   tkt, ap_opts, (char *) &type_check, 2);
  if (rc != SHISHI_OK)
    {
      DEBUG (("telnet: Kerberos V5: Could not make AP-REQ (%s)\r\n",
	      shishi_strerror (rc)));
      return 0;
    }

# ifdef ENCRYPTION
  if (enckey)
    {
      shishi_key_done (enckey);
      enckey = NULL;
    }

  rc = shishi_authenticator_get_subkey
    (shishi_handle, shishi_ap_authenticator (auth_handle), &enckey);
  if (rc != SHISHI_OK)
    {
      DEBUG (("telnet: Kerberos V5: could not get encryption key (%s)\r\n",
	      shishi_strerror (rc)));
      return 0;
    }
# endif

  rc = shishi_ap_req_der (auth_handle, &apreq, &apreq_len);
  if (rc != SHISHI_OK)
    {
      DEBUG (("telnet: Kerberos V5: could not DER encode (%s)\r\n",
	      shishi_strerror (rc)));
      return 0;
    }

  if (auth_debug_mode)
    {
      shishi_authenticator_print (shishi_handle, stdout,
				  shishi_ap_authenticator (auth_handle));
      shishi_apreq_print (shishi_handle, stdout, shishi_ap_req (auth_handle));
    }

  if (!auth_sendname (UserNameRequested, strlen (UserNameRequested)))
    {
      DEBUG (("telnet: Not enough room for user name\r\n"));
      return 0;
    }

  if (!Data (ap, KRB_AUTH, apreq, apreq_len))
    {
      DEBUG (("telnet: Not enough room for authentication data\r\n"));
      return 0;
    }

  DEBUG (("telnet: Sent Kerberos V5 credentials to server\r\n"));

  return 1;
}

# ifdef ENCRYPTION
static void
shishi_init_key (Session_Key * skey, int type)
{
  int32_t etype = shishi_key_type (enckey);

  if (etype == SHISHI_DES_CBC_CRC || etype == SHISHI_DES_CBC_MD4
      || etype == SHISHI_DES_CBC_MD5)
    skey->type = SK_DES;
  else
    skey->type = SK_OTHER;
  skey->length = shishi_key_length (enckey);
  skey->data = (unsigned char *) shishi_key_value (enckey);

  encrypt_session_key (skey, type);
}
# endif

void
krb5shishi_reply (TN_Authenticator * ap, unsigned char *data, int cnt)
{
  static int mutual_complete = 0;
# ifdef ENCRYPTION
  Session_Key skey;
# endif

  if (cnt-- < 1)
    return;

  switch (*data++)
    {
    case KRB_REJECT:
      if (cnt > 0)
	printf ("[ Kerberos V5 rejects authentication: %.*s ]\r\n",
		cnt, data);
      else
	printf ("[ Kerberos V5 refuses authentication ]\r\n");
      auth_send_retry ();
      return;

    case KRB_ACCEPT:
      if (!mutual_complete)
	{
	  if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL)
	    {
	      printf ("[ Kerberos V5 accepted you, "
		      "but didn't provide mutual authentication! ]\r\n");
	      auth_send_retry ();
	      break;
	    }
# ifdef ENCRYPTION
	  shishi_init_key (&skey, 0);
# endif
	}

      if (cnt)
	printf ("[ Kerberos V5 accepts you as ``%.*s''%s ]\r\n", cnt, data,
		mutual_complete ?
		" (server authenticated)" : " (server NOT authenticated)");
      else
	printf ("[ Kerberos V5 accepts you ]\r\n");

      auth_finished (ap, AUTH_USER);
      /* This was last access to handle on behalf of the client.  */
      shishi_done (shishi_handle);
      shishi_handle = NULL;
      break;

    case KRB_RESPONSE:
      if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL)
	{
	  if (shishi_ap_rep_verify_der (auth_handle, (char *) data, cnt)
	      != SHISHI_OK)
	    {
	      printf ("[ Mutual authentication failed ]\r\n");
	      auth_send_retry ();
	      break;
	    }

	  if (auth_debug_mode)
	    {
	      shishi_aprep_print (shishi_handle, stdout,
				  shishi_ap_rep (auth_handle));
	      shishi_encapreppart_print (shishi_handle, stdout,
					 shishi_ap_encapreppart
					 (auth_handle));
	    }

# ifdef ENCRYPTION
	  shishi_init_key (&skey, 0);
# endif
	  mutual_complete = 1;
	}
      break;

    default:
      DEBUG (("Unknown Kerberos option %d\r\n", data[-1]));
    }
}

int
krb5shishi_status (TN_Authenticator * ap _GL_UNUSED_PARAMETER,
		   char *name, size_t len, int level)
{
  int status;

  if (level < AUTH_USER)
    return level;

  if (UserNameRequested
      && shishi_authorized_p (shishi_handle,
			      shishi_ap_tkt (auth_handle),
			      UserNameRequested))
    {
      /* FIXME: Check buffer length */
      strncpy (name, UserNameRequested, len);
      status = AUTH_VALID;
    }
  else
    status = AUTH_USER;

  return status;
}

static int
krb5shishi_is_auth (TN_Authenticator * a, unsigned char *data, int cnt,
		    char *errbuf, int errbuflen)
{
  Shishi_key *key;
  int rc;
  char *cnamerealm, *server = NULL, *realm = NULL;
  size_t cnamerealmlen;
# ifdef ENCRYPTION
  Session_Key skey;
# endif

  if (!delayed_shishi_init ())
    {
      DEBUG (("telnet: Kerberos V5: shishi initialization failed\r\n"));
      return 0;
    }

  /* Enable use of `~/.k5login'.  */
  if (shishi_check_version ("1.0.2"))	/* Faulty in version 1.0.1.  */
    {
      rc = shishi_cfg_authorizationtype_set (shishi_handle, "k5login basic");
      if (rc != SHISHI_OK)
	{
	  snprintf (errbuf, errbuflen,
		    "Cannot initiate authorization types: %s",
		    shishi_error (shishi_handle));
	  return rc;
	}
    }

  if (ServerPrincipal && *ServerPrincipal)
    {
      rc = shishi_parse_name (shishi_handle, ServerPrincipal,
			      &server, &realm);
      if (rc != SHISHI_OK)
	{
	  snprintf (errbuf, errbuflen,
		    "Cannot parse server principal name: %s",
		    shishi_strerror (rc));
	  return 1;
	}
      if (realm)
	shishi_realm_default_set (shishi_handle, realm);

      /* Reclaim an empty server part.  */
      if (server && !*server)
	{
	  free (server);
	  server = NULL;
	}
    }

  if (!server)
    {
      server = malloc (strlen ("host/") + strlen (LocalHostName) + 1);
      if (server)
	sprintf (server, "host/%s", LocalHostName);
    }

  if (server)
    {
      /* Two possible action on `server':
       *   "srv.local"    :  rewrite as "host/srv.local"
       *   "tn/srv.local" :  accept as is
       */
      char *p = strchr (server, '/');

      if (!p)
	{
	  p = server;
	  server = malloc (strlen ("host/") + strlen (p) + 1);
	  if (!server)
	    {
	      free (p);		/* This old `server'.  */
	      snprintf (errbuf, errbuflen,
			"Cannot allocate memory for server name");
	      return 1;
	    }
	  sprintf (server, "host/%s", p);
	}

      if (realm)
	key = shishi_hostkeys_for_serverrealm (shishi_handle,
					       server, realm);
      else
	/* Enforce a search with the known default realm.  */
	key = shishi_hostkeys_for_serverrealm (shishi_handle,
			server, shishi_realm_default (shishi_handle));

      free (server);
    }
  else
    key = shishi_hostkeys_for_localservicerealm (shishi_handle,
						 "host", realm);

  free (realm);

  if (key == NULL)
    {
      snprintf (errbuf, errbuflen, "Could not find key: %s",
		shishi_error (shishi_handle));
      return 1;
    }

  rc = shishi_ap (shishi_handle, &auth_handle);
  if (rc != SHISHI_OK)
    {
      snprintf (errbuf, errbuflen,
		"Cannot allocate authentication structures: %s",
		shishi_strerror (rc));
      return 1;
    }

  rc = shishi_ap_req_der_set (auth_handle, (char *) data, cnt);
  if (rc != SHISHI_OK)
    {
      snprintf (errbuf, errbuflen,
		"Cannot parse authentication information: %s",
		shishi_strerror (rc));
      return 1;
    }

  rc = shishi_ap_req_process (auth_handle, key);
  if (rc != SHISHI_OK)
    {
      snprintf (errbuf, errbuflen, "Could not process AP-REQ: %s",
		shishi_strerror (rc));
      return 1;
    }

  if (shishi_apreq_mutual_required_p (shishi_handle,
				      shishi_ap_req (auth_handle)))
    {
      char *der;
      size_t derlen;

      rc = shishi_ap_rep_der (auth_handle, &der, &derlen);
      if (rc != SHISHI_OK)
	{
	  snprintf (errbuf, errbuflen, "Error DER encoding aprep: %s",
		    shishi_strerror (rc));
	  return 1;
	}

      Data (a, KRB_RESPONSE, der, derlen);
      free (der);
    }

  rc = shishi_encticketpart_clientrealm (
		shishi_handle,
		shishi_tkt_encticketpart (shishi_ap_tkt (auth_handle)),
		&cnamerealm, &cnamerealmlen);
  if (rc != SHISHI_OK)
    {
      snprintf (errbuf, errbuflen, "Error getting authenticator name: %s",
		shishi_strerror (rc));
      return 1;
    }
  Data (a, KRB_ACCEPT, cnamerealm, cnamerealm ? -1 : 0);
  DEBUG (("telnetd: Kerberos5 identifies him as ``%s''\r\n",
	  cnamerealm ? cnamerealm : ""));
  free (cnamerealm);
  auth_finished (a, AUTH_USER);

  /* Make sure that shishi_handle is still valid,
   * it must not be released in auth_finish()!
   * The server side will make reference to it
   * later on.  */

# ifdef ENCRYPTION
  if (enckey)
    {
      shishi_key_done (enckey);
      enckey = NULL;
    }

  rc = shishi_authenticator_get_subkey (shishi_handle,
					shishi_ap_authenticator (auth_handle),
					&enckey);
  if (rc != SHISHI_OK)
    {
      Shishi_tkt *tkt;

      tkt = shishi_ap_tkt (auth_handle);
      if (tkt)
	{
	  rc = shishi_encticketpart_get_key (shishi_handle,
					     shishi_tkt_encticketpart (tkt),
					     &enckey);
	  if (rc != SHISHI_OK)
	    enckey = NULL;

	  shishi_tkt_done (tkt);
	}
    }

  if (enckey == NULL)
    {
      snprintf (errbuf, errbuflen,
		"telnet: Kerberos V5: could not get encryption key (%s)\r\n",
		shishi_strerror (rc));
      return 1;
    }

  shishi_init_key (&skey, 1);
# endif

  return 0;
}

void
krb5shishi_is (TN_Authenticator * ap, unsigned char *data, int cnt)
{
  int r = 0;
  char errbuf[512];

  if (cnt-- < 1)
    return;
  errbuf[0] = 0;
  switch (*data++)
    {
    case KRB_AUTH:
      r = krb5shishi_is_auth (ap, data, cnt, errbuf, sizeof errbuf);
      break;

    default:
      DEBUG (("Unknown Kerberos option %d\r\n", data[-1]));
      Data (ap, KRB_REJECT, 0, 0);
      break;
    }

  if (r)
    {
      if (!errbuf[0])
	snprintf (errbuf, sizeof errbuf, "kerberos_is: error");
      Data (ap, KRB_REJECT, errbuf, -1);
      DEBUG (("%s\r\n", errbuf));
      syslog (LOG_ERR, "%s", errbuf);
    }
}

static char *
req_type_str (int type)
{
  switch (type)
    {
    case KRB_REJECT:
      return "REJECT";

    case KRB_ACCEPT:
      return "ACCEPT";

    case KRB_AUTH:
      return "AUTH";

    case KRB_RESPONSE:
      return "RESPONSE";

    }
  return NULL;
}

# define ADDC(p,l,c) if ((l) > 0) {*(p)++ = (c); --(l);}

void
krb5shishi_printsub (unsigned char *data, int cnt,
		     char *buf, int buflen)
{
  char *p;
  int i;

  buf[buflen - 1] = '\0';	/* make sure its NULL terminated */
  buflen -= 1;

  p = req_type_str (data[3]);
  if (!p)
    {
      int l = snprintf (buf, buflen, " %d (unknown)", data[3]);
      buf += l;
      buflen -= l;
    }
  else
    {
      while (buflen > 0 && (*buf++ = *p++) != 0)
	buflen--;
    }

  switch (data[3])
    {
    case KRB_REJECT:		/* Rejected (reason might follow) */
    case KRB_ACCEPT:		/* Accepted (username might follow) */
      if (cnt <= 4)
	break;
      ADDC (buf, buflen, '"');
      for (i = 4; i < cnt; i++)
	ADDC (buf, buflen, data[i]);
      ADDC (buf, buflen, '"');
      ADDC (buf, buflen, '\0');
      break;

    case KRB_AUTH:
    case KRB_RESPONSE:
      for (i = 4; buflen > 0 && i < cnt; i++)
	{
	  int l = snprintf (buf, buflen, " %d", data[i]);
	  buf += l;
	  buflen -= l;
	}
    }
}

#endif /* SHISHI */
