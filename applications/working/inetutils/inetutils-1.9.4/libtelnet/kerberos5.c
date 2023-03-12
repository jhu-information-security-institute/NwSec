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

#include <config.h>

#ifdef KRB5
# include <stdlib.h>
# include <stdio.h>
# include <arpa/telnet.h>
# ifdef HAVE_KRB5_H
#  include <krb5.h>
# endif
# include <assert.h>

# ifdef HAVE_COM_ERR_H
#  include <com_err.h>
# endif
# include <netdb.h>
# include <ctype.h>
# include <syslog.h>
# include <string.h>
# include <unused-parameter.h>

# include "auth.h"
# include "misc.h"

# ifndef KRB5_ENV_CCNAME
#  define KRB5_ENV_CCNAME "KRB5CCNAME"
# endif

# ifdef  ENCRYPTION
#  include "encrypt.h"
# endif

# ifdef  FORWARD
/* FIXME: This is set directly from telnet/main.c */
int forward_flags = 0;
extern int net;
 /*FIXME*/ void kerberos5_forward ();
# endif	/* FORWARD */

static unsigned char str_data[2048] = { IAC, SB, TELOPT_AUTHENTICATION, 0,
  AUTHTYPE_KERBEROS_V5,
};

# define KRB_AUTH             0	/* Authentication data follows */
# define KRB_REJECT           1	/* Rejected (reason might follow) */
# define KRB_ACCEPT           2	/* Accepted */
# define KRB_RESPONSE         3	/* Response for mutual auth. */
# define KRB_FORWARD          4	/* Forwarded credentials follow */
# define KRB_FORWARD_ACCEPT   5	/* Forwarded credentials accepted */
# define KRB_FORWARD_REJECT   6	/* Forwarded credentials rejected */

krb5_auth_context auth_context = 0;
krb5_context telnet_context = 0;

static krb5_data auth;		/* session key for telnet */
static krb5_ticket *ticket = NULL;	/* telnet matches the AP_REQ and
					   AP_REP with this */

krb5_keyblock *session_key = 0;
char *telnet_srvtab = NULL;
char *dest_realm = NULL;

# define DEBUG(c) if (auth_debug_mode) printf c

/* Callback from consumer.  */
extern void printsub (char, unsigned char *, int);

static int
Data (TN_Authenticator * ap, int type, krb5_pointer d, int c)
{
  unsigned char *p = str_data + 4;
  unsigned char *cd = (unsigned char *) d;

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

/* FIXME: Reverse return code! */
int
kerberos5_init (TN_Authenticator * ap _GL_UNUSED_PARAMETER, int server)
{
  str_data[3] = server ? TELQUAL_REPLY : TELQUAL_IS;
  if (telnet_context == 0 && krb5_init_context (&telnet_context))
    return 0;
  return 1;
}

void
kerberos5_cleanup ()
{
  krb5_ccache ccache;
  char *ccname;

  if (telnet_context == 0)
    return;

  ccname = getenv (KRB5_ENV_CCNAME);
  if (ccname)
    {
      if (!krb5_cc_resolve (telnet_context, ccname, &ccache))
	krb5_cc_destroy (telnet_context, ccache);
    }

  krb5_free_context (telnet_context);
  telnet_context = 0;
}

# ifdef  ENCRYPTION

void
encryption_init (krb5_creds * creds)
{
  krb5_keyblock *newkey = 0;

  krb5_auth_con_getsendsubkey (telnet_context, auth_context, &newkey);
  if (session_key)
    {
      krb5_free_keyblock (telnet_context, session_key);
      session_key = 0;
    }

  if (newkey)
    {
      switch (newkey->enctype)
	{
	case ENCTYPE_DES_CBC_CRC:
	case ENCTYPE_DES_CBC_MD5:
	  krb5_copy_keyblock (telnet_context, newkey, &session_key);
	  break;

	default:
	  switch (creds->keyblock.enctype)
	    {
	    case ENCTYPE_DES_CBC_CRC:
	    case ENCTYPE_DES_CBC_MD5:
	      krb5_copy_keyblock (telnet_context, &creds->keyblock,
				  &session_key);
	      break;

	    default:
	      DEBUG (("can't determine which keyblock to use"));
	      /*FIXME: abort? */
	    }
	}

      krb5_free_keyblock (telnet_context, newkey);
    }
}

# else
#  define encryption_init(c)
# endif

int
kerberos5_send (TN_Authenticator * ap)
{
  krb5_error_code r;
  krb5_ccache ccache;
  krb5_creds creds;
  krb5_creds *new_creds = 0;
  int ap_opts;
  char type_check[2];
  krb5_data check_data;

  if (!UserNameRequested)
    {
      DEBUG (("telnet: Kerberos V5: no user name supplied\r\n"));
      return 0;
    }

  if ((r = krb5_cc_default (telnet_context, &ccache)))
    {
      DEBUG (("telnet: Kerberos V5: could not get default ccache\r\n"));
      return 0;
    }

  memset (&creds, 0, sizeof (creds));
  if ((r = krb5_sname_to_principal (telnet_context, RemoteHostName,
				    "host", KRB5_NT_SRV_HST, &creds.server)))
    {
      DEBUG (("telnet: Kerberos V5: error while constructing service name: %s\r\n", error_message (r)));
      return 0;
    }

  if (dest_realm)
    {
      krb5_data rdata;

      rdata.length = strlen (dest_realm);
      rdata.data = malloc (rdata.length + 1);
      if (rdata.data == NULL)
	{
	  DEBUG (("telnet: Kerberos V5: could not allocate memory\r\n"));
	  return 0;
	}
      strcpy (rdata.data, dest_realm);
      krb5_princ_set_realm (telnet_context, creds.server, &rdata);
    }

  if ((r = krb5_cc_get_principal (telnet_context, ccache, &creds.client)))
    {
      DEBUG (("telnet: Kerberos V5: failure on principal (%s)\r\n",
	      error_message (r)));
      krb5_free_cred_contents (telnet_context, &creds);
      return 0;
    }

  creds.keyblock.enctype = ENCTYPE_DES_CBC_CRC;
  if ((r = krb5_get_credentials (telnet_context, 0,
				 ccache, &creds, &new_creds)))
    {
      DEBUG (("telnet: Kerberos V5: failure on credentials(%s)\r\n",
	      error_message (r)));
      krb5_free_cred_contents (telnet_context, &creds);
      return 0;
    }

  if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL)
    ap_opts = AP_OPTS_MUTUAL_REQUIRED;
  else
    ap_opts = 0;

# ifdef ENCRYPTION
  ap_opts |= AP_OPTS_USE_SUBKEY;
# endif

  if (auth_context)
    {
      krb5_auth_con_free (telnet_context, auth_context);
      auth_context = 0;
    }

  if ((r = krb5_auth_con_init (telnet_context, &auth_context)))
    {
      DEBUG (("Kerberos V5: failed to init auth_context (%s)\r\n",
	      error_message (r)));
      return 0;
    }

  krb5_auth_con_setflags (telnet_context, auth_context,
			  KRB5_AUTH_CONTEXT_RET_TIME);

  type_check[0] = ap->type;
  type_check[1] = ap->way;
  check_data.magic = KV5M_DATA;
  check_data.length = 2;
  check_data.data = (char *) &type_check;

  r = krb5_mk_req_extended (telnet_context, &auth_context, ap_opts,
			    &check_data, new_creds, &auth);

  encryption_init (new_creds);

  krb5_free_cred_contents (telnet_context, &creds);
  krb5_free_creds (telnet_context, new_creds);
  if (r)
    {
      DEBUG (("telnet: Kerberos V5: mk_req failed (%s)\r\n",
	      error_message (r)));
      return 0;
    }

  if (!auth_sendname (UserNameRequested, strlen (UserNameRequested)))
    {
      DEBUG (("telnet: Not enough room for user name\r\n"));
      return 0;
    }

  if (!Data (ap, KRB_AUTH, auth.data, auth.length))
    {
      DEBUG (("telnet: Not enough room for authentication data\r\n"));
      return 0;
    }

  DEBUG (("telnet: Sent Kerberos V5 credentials to server\r\n"));

  return 1;
}

# ifdef ENCRYPTION
void
telnet_encrypt_key (Session_Key * skey)
{
  if (session_key)
    {
      skey->type = SK_DES;
      skey->length = 8;
      skey->data = session_key->contents;
      encrypt_session_key (skey, 0);
    }
}
# else
#  define telnet_encrypt_key(s)
# endif

void
kerberos5_reply (TN_Authenticator * ap, unsigned char *data, int cnt)
{
# ifdef ENCRYPTION
  Session_Key skey;
# endif
  static int mutual_complete = 0;

  if (cnt-- < 1)
    return;

  switch (*data++)
    {
    case KRB_REJECT:
      if (cnt > 0)
	printf ("[ Kerberos V5 refuses authentication because %.*s ]\r\n",
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
	      printf
		("[ Kerberos V5 accepted you, but didn't provide mutual authentication! ]\r\n");
	      auth_send_retry ();
	      break;
	    }
	  telnet_encrypt_key (&skey);
	}

      if (cnt)
	printf ("[ Kerberos V5 accepts you as ``%.*s''%s ]\r\n", cnt, data,
		mutual_complete ?
		" (server authenticated)" : " (server NOT authenticated)");
      else
	printf ("[ Kerberos V5 accepts you ]\r\n");
      auth_finished (ap, AUTH_USER);
# ifdef  FORWARD
      if (forward_flags & OPTS_FORWARD_CREDS)
	kerberos5_forward (ap);
# endif
      break;

    case KRB_RESPONSE:
      if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL)
	{
	  krb5_ap_rep_enc_part *reply;
	  krb5_data inbuf;
	  krb5_error_code r;

	  inbuf.length = cnt;
	  inbuf.data = (char *) data;

	  if ((r = krb5_rd_rep (telnet_context, auth_context, &inbuf,
				&reply)))
	    {
	      printf ("[ Mutual authentication failed: %s ]\r\n",
		      error_message (r));
	      auth_send_retry ();
	      break;
	    }

	  krb5_free_ap_rep_enc_part (telnet_context, reply);
	  telnet_encrypt_key (&skey);
	  mutual_complete = 1;
	}
      break;

# ifdef  FORWARD
    case KRB_FORWARD_ACCEPT:
      printf ("[ Kerberos V5 accepted forwarded credentials ]\r\n");
      break;

    case KRB_FORWARD_REJECT:
      printf
	("[ Kerberos V5 refuses forwarded credentials because %.*s ]\r\n",
	 cnt, data);
      break;
# endif	/* FORWARD */

    default:
      DEBUG (("Unknown Kerberos option %d\r\n", data[-1]));
    }
}

int
kerberos5_status (TN_Authenticator * ap _GL_UNUSED_PARAMETER,
		  char *name, size_t len, int level)
{
  if (level < AUTH_USER)
    return level;

  if (UserNameRequested
      && krb5_kuserok (telnet_context, ticket->enc_part2->client,
		       UserNameRequested))
    {
      /* FIXME: Check buffer length */
      strncpy (name, UserNameRequested, len);
      return AUTH_VALID;
    }
  return AUTH_USER;
}

static int
kerberos5_is_auth (TN_Authenticator * ap, unsigned char *data, int cnt,
		   char *errbuf, int errbuflen)
{
  int r = 0;
  krb5_keytab keytabid = 0;
  krb5_authenticator *authenticator;
  char *name;
  krb5_data outbuf;
  krb5_keyblock *newkey = NULL;
  krb5_principal server;

# ifdef ENCRYPTION
  Session_Key skey;
# endif

  auth.data = (char *) data;
  auth.length = cnt;

  if (!r && !auth_context)
    r = krb5_auth_con_init (telnet_context, &auth_context);
  if (!r)
    {
      krb5_rcache rcache;

      r = krb5_auth_con_getrcache (telnet_context, auth_context, &rcache);
      if (!r && !rcache)
	{
	  r = krb5_sname_to_principal (telnet_context, 0, 0,
				       KRB5_NT_SRV_HST, &server);
	  if (!r)
	    {
	      r = krb5_get_server_rcache (telnet_context,
					  krb5_princ_component
					  (telnet_context, server, 0),
					  &rcache);
	      krb5_free_principal (telnet_context, server);
	    }
	}
      if (!r)
	r = krb5_auth_con_setrcache (telnet_context, auth_context, rcache);
    }

  if (!r && telnet_srvtab)
    r = krb5_kt_resolve (telnet_context, telnet_srvtab, &keytabid);
  if (!r)
    r = krb5_rd_req (telnet_context, &auth_context, &auth,
		     NULL, keytabid, NULL, &ticket);
  if (r)
    {
      snprintf (errbuf, errbuflen, "krb5_rd_req failed: %s",
		error_message (r));
      return r;
    }

  /* 256 bytes should be much larger than any reasonable
     first component of a service name especially since
     the default is of length 4. */
  if (krb5_princ_component (telnet_context, ticket->server, 0)->length < 256)
    {
      char princ[256];
      strncpy (princ,
	       krb5_princ_component (telnet_context, ticket->server, 0)->data,
	       krb5_princ_component (telnet_context, ticket->server,
				     0)->length);
      princ[krb5_princ_component (telnet_context, ticket->server, 0)->
	    length] = '\0';
      if (strcmp ("host", princ))
	{
	  snprintf (errbuf, errbuflen,
		    "incorrect service name: \"%s\" != \"host\"", princ);
	  return 1;
	}
    }
  else
    {
      strncpy (errbuf, "service name too long", errbuflen);
      return 1;
    }

  r = krb5_auth_con_getauthenticator (telnet_context,
				      auth_context, &authenticator);
  if (r)
    {
      snprintf (errbuf, errbuflen,
		"krb5_auth_con_getauthenticator failed: %s",
		error_message (r));
      return 1;
    }

# ifdef AUTH_ENCRYPT_MASK
  if ((ap->way & AUTH_ENCRYPT_MASK) == AUTH_ENCRYPT_ON
      && !authenticator->checksum)
    {
      snprintf (errbuf, errbuflen,
		"authenticator is missing required checksum");
      return 1;
    }
# endif

  if (authenticator->checksum)
    {
      char type_check[2];
      krb5_checksum *cksum = authenticator->checksum;
      krb5_keyblock *key;
      krb5_boolean valid;

      type_check[0] = ap->type;
      type_check[1] = ap->way;

      r = krb5_auth_con_getkey (telnet_context, auth_context, &key);
      if (r)
	{
	  snprintf (errbuf, errbuflen,
		    "krb5_auth_con_getkey failed: %s", error_message (r));
	  return 1;
	}

#  if 1
      /* XXX: Obsolete interface.  Remove after investigation.  */
      r = krb5_verify_checksum (telnet_context,
				cksum->checksum_type, cksum,
				&type_check, 2, key->contents, key->length);
      krb5_free_keyblock (telnet_context, key);

      if (r)
	{
	  snprintf (errbuf, errbuflen,
		    "checksum verification failed: %s", error_message (r));
	  return 1;
	}
#else
      /* Incomplete call!
       *
       * XXX: Establish replacement for the preceding call.
       *      It is no longer present in all implementations.
       */
      r = krb5_c_verify_checksum (telnet_context, key,
				  /* usage */, /* data */,
				  cksum, &valid);
      krb5_free_keyblock (telnet_context, key);

      if (r || !valid)
	{
	  snprintf (errbuf, errbuflen,
		    "checksum verification failed: %s", error_message (r));
	  return 1;
	}
#endif
    }

  krb5_free_authenticator (telnet_context, authenticator);
  if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL)
    {
      if ((r = krb5_mk_rep (telnet_context, auth_context, &outbuf)))
	{
	  snprintf (errbuf, errbuflen, "Make reply failed: %s",
		    error_message (r));
	  return 1;
	}

      Data (ap, KRB_RESPONSE, outbuf.data, outbuf.length);
    }

  if (krb5_unparse_name (telnet_context, ticket->enc_part2->client, &name))
    name = 0;

  Data (ap, KRB_ACCEPT, name, name ? -1 : 0);
  DEBUG (("telnetd: Kerberos5 identifies him as ``%s''\r\n",
	  name ? name : ""));
  auth_finished (ap, AUTH_USER);

  free (name);
  krb5_auth_con_getrecvsubkey (telnet_context, auth_context, &newkey);

  if (session_key)
    {
      krb5_free_keyblock (telnet_context, session_key);
      session_key = 0;
    }

  if (newkey)
    {
      krb5_copy_keyblock (telnet_context, newkey, &session_key);
      krb5_free_keyblock (telnet_context, newkey);
    }
  else
    {
      krb5_copy_keyblock (telnet_context, ticket->enc_part2->session,
			  &session_key);
    }
  telnet_encrypt_key (&skey);
  return 0;
}

# ifdef FORWARD
static int
kerberos5_is_forward (TN_Authenticator * ap, unsigned char *data, int cnt,
		      char *errbuf, int errbuflen)
{
  int r = 0;
  krb5_data inbuf;

  inbuf.length = cnt;
  inbuf.data = (char *) data;
  if ((r = krb5_auth_con_genaddrs (telnet_context, auth_context,
				   net,
				   KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR))
      || (r = rd_and_store_for_creds (telnet_context, auth_context,
				      &inbuf, ticket)))
    {
      snprintf (errbuf, errbuflen, "Read forwarded creds failed: %s",
		error_message (r));
      Data (ap, KRB_FORWARD_REJECT, errbuf, -1);
      DEBUG (("Could not read forwarded credentials\r\n"));
    }
  else
    {
      Data (ap, KRB_FORWARD_ACCEPT, 0, 0);
      DEBUG (("Forwarded credentials obtained\r\n"));
    }
  return r;
}
# else
#  define kerberos5_is_forward(ap,data,cnt,errbuf,errbuflen) 0
# endif

void
kerberos5_is (TN_Authenticator * ap, unsigned char *data, int cnt)
{
  int r = 0;
  char errbuf[512];

  if (cnt-- < 1)
    return;
  errbuf[0] = 0;
  switch (*data++)
    {
    case KRB_AUTH:
      r = kerberos5_is_auth (ap, data, cnt, errbuf, sizeof errbuf);
      break;

    case KRB_FORWARD:
      r = kerberos5_is_forward (ap, data, cnt, errbuf, sizeof errbuf);
      break;

    default:
      DEBUG (("Unknown Kerberos option %d\r\n", data[-1]));
      Data (ap, KRB_REJECT, 0, 0);
      break;
    }

  if (r)
    {
      if (!errbuf[0])
	snprintf (errbuf, sizeof errbuf,
		  "kerberos_is: %s", error_message (r));
      Data (ap, KRB_REJECT, errbuf, -1);
      DEBUG (("%s\r\n", errbuf));
      syslog (LOG_ERR, "%s", errbuf);
      if (auth_context)
	{
	  krb5_auth_con_free (telnet_context, auth_context);
	  auth_context = 0;
	}
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

    case KRB_FORWARD:
      return "FORWARD";

    case KRB_FORWARD_ACCEPT:
      return "FORWARD_ACCEPT";

    case KRB_FORWARD_REJECT:
      return "FORWARD_REJECT";

    }
  return NULL;
}

# define ADDC(p,l,c) if ((l) > 0) {*(p)++ = (c); --(l);}

void
kerberos5_printsub (unsigned char *data, int cnt,
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
    case KRB_FORWARD:
    case KRB_FORWARD_ACCEPT:
    case KRB_FORWARD_REJECT:
      for (i = 4; buflen > 0 && i < cnt; i++)
	{
	  int l = snprintf (buf, buflen, " %d", data[i]);
	  buf += l;
	  buflen -= l;
	}
    }
}

# ifdef FORWARD

void
kerberos5_forward (TN_Authenticator * ap)
{
  krb5_error_code r;
  krb5_ccache ccache;
  krb5_principal client = 0;
  krb5_principal server = 0;
  krb5_data forw_creds;

  forw_creds.data = 0;

  if ((r = krb5_cc_default (telnet_context, &ccache)))
    {
      DEBUG (("Kerberos V5: could not get default ccache - %s\r\n",
	      error_message (r)));
      return;
    }

  for (;;)			/* Fake loop */
    {
      if ((r = krb5_cc_get_principal (telnet_context, ccache, &client)))
	{
	  DEBUG (("Kerberos V5: could not get default principal - %s\r\n",
		  error_message (r)));
	  break;
	}
      if ((r =
	   krb5_sname_to_principal (telnet_context, RemoteHostName, "host",
				    KRB5_NT_SRV_HST, &server)))
	{
	  DEBUG (("Kerberos V5: could not make server principal - %s\r\n",
		  error_message (r)));
	  break;
	}
      if ((r = krb5_auth_con_genaddrs (telnet_context, auth_context, net,
				       KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR)))
	{
	  DEBUG (("Kerberos V5: could not gen local full address - %s\r\n",
		  error_message (r)));
	  break;
	}
      if ((r = krb5_fwd_tgt_creds (telnet_context, auth_context, 0, client,
				   server, ccache,
				   forward_flags & OPTS_FORWARDABLE_CREDS,
				   &forw_creds)))
	{
	  DEBUG (("Kerberos V5: error getting forwarded creds - %s\r\n",
		  error_message (r)));
	  break;
	}

      /* Send forwarded credentials */
      if (!Data (ap, KRB_FORWARD, forw_creds.data, forw_creds.length))
	{
	  DEBUG (("Not enough room for authentication data\r\n"));
	}
      else
	{
	  DEBUG (("Forwarded local Kerberos V5 credentials to server\r\n"));
	}
      break;
    }

  if (client)
    krb5_free_principal (telnet_context, client);
  if (server)
    krb5_free_principal (telnet_context, server);
  free (forw_creds.data);
  krb5_cc_close (telnet_context, ccache);
}
# endif

#endif /* KRB5 */
