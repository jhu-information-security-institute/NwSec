/* shishi.c -- functions to use kerberos V with shishi
  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012,
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

#ifdef SHISHI
# include <string.h>
# include <stdlib.h>
# include <errno.h>
# include <shishi.h>
# include <syslog.h>
# include <unistd.h>
# include <shishi_def.h>

/* shishi authentication, client side */
int
shishi_auth (Shishi ** handle, int verbose, char **cname,
	     const char *sname, int sock, char *cmd,
	     unsigned short port, Shishi_key ** enckey,
	     const char *realm)
{
  Shishi_ap *ap;
  Shishi_tkt *tkt;
  Shishi_tkts_hint hint;
  Shishi *h;

  int rc;
  char *out, *p;
  size_t outlen;
  int krb5len, msglen;
  char *tmpserver;
  char auth;
  /* KERBEROS 5 SENDAUTH MESSAGE */
  char krb5sendauth[] = "KRB5_SENDAUTH_V1.0";
  /* PROTOCOL VERSION */
  char krb5sendclient[] = "KCMDV0.2";
  /* to store error msg sent by server */
  char errormsg[101];
  char cksumdata[101];

  if (!shishi_check_version (SHISHI_VERSION))
    {
      fprintf (stderr, "shishi_check_version() failed:\n"
	       "Header file incompatible with shared library.\n");
      return SHISHI_INVALID_ARGUMENT;
    }

  rc = shishi_init (handle);
  if (rc != SHISHI_OK)
    {
      fprintf (stderr,
	       "error initializing shishi: %s\n", shishi_strerror (rc));
      return rc;
    }

  if (realm)
    shishi_realm_default_set (*handle, realm);

  h = *handle;

  if (!*cname)
    *cname = (char *) shishi_principal_default (h);

  /* size of KRB5 auth message */
  krb5len = strlen (krb5sendauth) + 1;
  msglen = htonl (krb5len);
  write (sock, &msglen, sizeof (int));
  /* KRB5 authentication message */
  write (sock, krb5sendauth, krb5len);
  /* size of client message */
  krb5len = strlen (krb5sendclient) + 1;
  msglen = htonl (krb5len);
  write (sock, &msglen, sizeof (int));
  /* KRB5 client message */
  write (sock, krb5sendclient, krb5len);

  /* get answer from server 0 = ok, 1 = error with message */
  read (sock, &auth, 1);
  if (auth)
    {
      ssize_t n;

      errormsg[0] = '\0';
      n = read (sock, errormsg, sizeof (errormsg) - 1);

      if (n >= 0 && n < (ssize_t) sizeof (errormsg))
	errormsg[n] = '\0';
      else
	errormsg[sizeof (errormsg) -1] = '\0';

      fprintf (stderr, "Error during server authentication : %s\n", errormsg);
      return SHISHI_VERIFY_FAILED;
    }

  if (verbose)
    {
      printf ("Client: %s\n", *cname);
      printf ("Server: %s\n", sname);
    }

  /* Get a ticket for the server. */

  memset (&hint, 0, sizeof (hint));

  tmpserver = malloc (strlen (SERVICE) + strlen (sname) + 2);
  if (!tmpserver)
    {
      perror ("shishi_auth()");
      return SHISHI_TOO_SMALL_BUFFER;
    }

  p = strchr (sname, '/');
  if (p && (p != sname))
    strcpy (tmpserver, sname);	/* Non-empty prefix.  */
  else
    sprintf (tmpserver, "%s/%s", SERVICE, sname + (p ? 1 : 0));

  /* Retrieve realm assigned to this server as per configuration,
   * unless an explicit domain was passed in the call.
   */
  if (!realm)
    {
      if (!p)
	p = (char *) sname;
      else if (*p == '/')
	++p;

      shishi_realm_default_set (h, shishi_realm_for_server (h, p));
    }

  hint.client = (char *) *cname;
  hint.server = (char *) tmpserver;

  tkt = shishi_tkts_get (shishi_tkts_default (h), &hint);
  if (!tkt)
    {
      fprintf (stderr, "cannot find ticket for \"%s@%s\"\n",
	       tmpserver, shishi_realm_default (h));
      free (tmpserver);
      return SHISHI_INVALID_TICKET;
    }

  free (tmpserver);

  if (verbose)
    shishi_tkt_pretty_print (tkt, stderr);

  /* Create Authentication context */

  rc = shishi_ap_tktoptions (h, &ap, tkt, SHISHI_APOPTIONS_MUTUAL_REQUIRED);
  if (rc != SHISHI_OK)
    {
      fprintf (stderr, "cannot create authentication context\n");
      return rc;
    }


  /* checksum = port: terminal name */

  snprintf (cksumdata, sizeof (cksumdata) - 1,
	    "%u:%s%s", ntohs (port), cmd, *cname);

  /* add checksum to authenticator */

  shishi_ap_authenticator_cksumdata_set (ap, cksumdata, strlen (cksumdata));
  /* To be compatible with MIT rlogind */
  shishi_ap_authenticator_cksumtype_set (ap, SHISHI_RSA_MD5);

  /* create der encoded AP-REQ */

  rc = shishi_ap_req_der (ap, &out, &outlen);
  if (rc != SHISHI_OK)
    {
      fprintf (stderr, "cannot build authentication request: %s\n",
	       shishi_strerror (rc));

      return rc;
    }

  if (verbose)
    shishi_authenticator_print (h, stderr, shishi_ap_authenticator (ap));

  /* extract subkey if present from ap exchange for secure connection */

  if (enckey)
    shishi_authenticator_get_subkey (h, shishi_ap_authenticator (ap), enckey);

  /* send size of AP-REQ to the server */

  msglen = outlen;
  msglen = htonl (msglen);
  write (sock, (char *) &msglen, sizeof (int));

  /* send AP-REQ to the server */

  write (sock, out, outlen);

  /* read response from server - what ? */

  read (sock, &rc, sizeof (rc));
  if (rc)
    return SHISHI_APREP_VERIFY_FAILED;

  /* For mutual authentication, wait for server reply. */

  if (shishi_apreq_mutual_required_p (h, shishi_ap_req (ap)))
    {
      if (verbose)
	printf ("Waiting for server to authenticate itself...\n");

      /* read size of the AP-REP */

      read (sock, (char *) &msglen, sizeof (int));

      /* read AP-REP */
      outlen = ntohl (msglen);
      outlen = read (sock, out, outlen);

      rc = shishi_ap_rep_verify_der (ap, out, outlen);
      if (rc == SHISHI_OK)
	{
	  if (verbose)
	    printf ("AP-REP verification OK...\n");
	}
      else
	{
	  if (rc == SHISHI_APREP_VERIFY_FAILED)
	    fprintf (stderr, "AP-REP verification failed...\n");
	  else
	    fprintf (stderr, "AP-REP verification error: %s\n",
		     shishi_strerror (rc));
	  return rc;
	}

      /* The server is authenticated. */
      if (verbose)
	printf ("Server authenticated.\n");
    }

  /* We are now authenticated. */
  if (verbose)
    printf ("User authenticated.\n");

  return SHISHI_OK;

}

/*
 * XXX: Is this ever needed?
 *
static void
senderror (int s, char type, char *buf)
{
  write (s, &type, sizeof (char));
  write (s, buf, strlen (buf));
}
*/

/* shishi authentication, server side */
int
get_auth (int infd, Shishi ** handle, Shishi_ap ** ap,
	  Shishi_key ** enckey, const char **err_msg, int *protoversion,
	  int *cksumtype, char **cksum, size_t *cksumlen, char *srvname)
{
  Shishi_key *key;
  char *out;
  size_t outlen;
  char *buf;
  int buflen;
  int len;
  int rc;
  int error;
  /* KERBEROS 5 SENDAUTH MESSAGE */
  char krb5sendauth[] = "KRB5_SENDAUTH_V1.0";
  /* PROTOCOL VERSION */
  char krb5kcmd1[] = "KCMDV0.1";
  char krb5kcmd2[] = "KCMDV0.2";
  char *servername, *server = NULL, *realm = NULL;

  *err_msg = NULL;
  /* Get key for the server. */

  if (!shishi_check_version (SHISHI_VERSION))
    {
      *err_msg =
	"shishi_check_version() failed: header file incompatible with shared library.";
      return SHISHI_INVALID_ARGUMENT;
    }

  rc = shishi_init_server (handle);
  if (rc != SHISHI_OK)
    return rc;

  if (srvname && *srvname)
    {
      rc = shishi_parse_name (*handle, srvname, &server, &realm);
      if (rc != SHISHI_OK)
	{
	  *err_msg = shishi_strerror (rc);
	  return rc;
	}
    }

  if (server && *server)
    {
      char *p;

      servername = malloc (sizeof (SERVICE) + strlen (server) + 2);
      if (!servername)
	{
	  *err_msg = "Not enough memory";
	  return SHISHI_TOO_SMALL_BUFFER;
	}

      p = strchr (server, '/');
      if (p && (p != server))
	sprintf (servername, "%s", server);	/* Non-empty prefix.  */
      else
	sprintf (servername, "%s/%s", SERVICE,
		 server + (p ? 1 : 0));	/* Remove initial slash.  */
    }
  else
    servername = shishi_server_for_local_service (*handle, SERVICE);

  if (realm && *realm)
    shishi_realm_default_set (*handle, realm);

  free (server);
  free (realm);

  /* Enable use of `~/.k5login'.  */
  if (shishi_check_version ("1.0.2"))	/* Faulty in version 1.0.1.  */
    {
      rc = shishi_cfg_authorizationtype_set (*handle, "k5login basic");
      if (rc != SHISHI_OK)
	{
	  *err_msg = shishi_error (*handle);
	  return rc;
	}
    }

  key = shishi_hostkeys_for_serverrealm (*handle, servername,
					 shishi_realm_default (*handle));
  free (servername);
  if (!key)
    {
      *err_msg = shishi_error (*handle);
      return SHISHI_INVALID_KEY;
    }

  /* Read Kerberos 5 sendauth message */
  rc = read (infd, &len, sizeof (int));
  if (rc != sizeof (int))
    {
      *err_msg = "Error reading message size";
      return SHISHI_IO_ERROR;
    }

  buflen = ntohl (len);
  buf = malloc (buflen);
  if (!buf)
    {
      *err_msg = "Not enough memory";
      return SHISHI_TOO_SMALL_BUFFER;
    }

  rc = read (infd, buf, buflen);
  if (rc != buflen)
    {
      *err_msg = "Error reading authentication message";
      return SHISHI_IO_ERROR;
    }

  len = strlen (krb5sendauth);
  rc = strncmp (buf, krb5sendauth, buflen >= len ? len : buflen);
  if (rc)
    {
      *err_msg = "Invalid authentication type";
      /* Authentication type is wrong.  */
      write (infd, "\001", 1);
      return SHISHI_VERIFY_FAILED;
    }

  free (buf);

  /* Read protocol version */
  rc = read (infd, &len, sizeof (int));
  if (rc != sizeof (int))
    {
      *err_msg = "Error reading protocol message size";
      return SHISHI_IO_ERROR;
    }
  buflen = ntohl (len);
  buf = malloc (buflen);
  if (!buf)
    {
      *err_msg = "Not enough memory";
      return SHISHI_TOO_SMALL_BUFFER;
    }

  rc = read (infd, buf, buflen);
  if (rc != buflen)
    {
      *err_msg = "Error reading protocol message";
      return SHISHI_IO_ERROR;
    }

  len = strlen (krb5kcmd1);
  rc = strncmp (buf, krb5kcmd1, buflen >= len ? len : buflen);
  if (rc)
    {
      len = strlen (krb5kcmd2);
      rc = strncmp (buf, krb5kcmd2, buflen >= len ? len : buflen);
      if (rc)
	{
	  *err_msg = "Protocol version not supported";
	  /* Protocol version is wrong.  */
	  write (infd, "\002", 1);
	  return SHISHI_VERIFY_FAILED;
	}
      *protoversion = 2;
    }
  else
    *protoversion = 1;

  free (buf);

  /* Authentication type is ok */

  write (infd, "\0", 1);

  /* Read Authentication request from client */

  rc = read (infd, &len, sizeof (int));
  if (rc != sizeof (int))
    {
      *err_msg = "Error reading authentication request size";
      return SHISHI_IO_ERROR;
    }

  buflen = ntohl (len);
  buf = malloc (buflen);
  if (!buf)
    {
      *err_msg = "Not enough memory";
      return SHISHI_TOO_SMALL_BUFFER;
    }

  rc = read (infd, buf, buflen);
  if (rc != buflen)
    {
      *err_msg = "Error reading authentication request";
      return SHISHI_IO_ERROR;
    }

  /* Create Authentication context */

  rc = shishi_ap_nosubkey (*handle, ap);
  if (rc != SHISHI_OK)
    return rc;

  /* Store request in context */

  rc = shishi_ap_req_der_set (*ap, buf, buflen);
  if (rc != SHISHI_OK)
    return rc;

  free (buf);

  /* Process authentication request */

  rc = shishi_ap_req_process (*ap, key);
  if (rc != SHISHI_OK)
    return rc;

# ifdef ENCRYPTION

  /* extract subkey if present from ap exchange for secure connection */
  if (*protoversion == 2)
    {
      *enckey = NULL;
      shishi_authenticator_get_subkey (*handle,
				       shishi_ap_authenticator (*ap), enckey);
    }

# endif

  /* Get authenticator checksum */
  rc = shishi_authenticator_cksum (*handle,
				   shishi_ap_authenticator (*ap),
				   cksumtype, cksum, cksumlen);
  if (rc != SHISHI_OK)
    return rc;

  /* User is authenticated.  */
  error = 0;
  write (infd, &error, sizeof (int));

  /* Authenticate ourself to client, if requested.  */

  if (shishi_apreq_mutual_required_p (*handle, shishi_ap_req (*ap)))
    {
      int len;

      rc = shishi_ap_rep_der (*ap, &out, &outlen);
      if (rc != SHISHI_OK)
	return rc;

      len = outlen;
      len = htonl (len);
      rc = write (infd, &len, sizeof (len));
      if (rc != sizeof (int))
	{
	  *err_msg = "Error sending AP-REP";
	  free (out);
	  return SHISHI_IO_ERROR;
	}

      rc = write (infd, out, ntohl (len));
      if (rc != (int) ntohl (len))
	{
	  *err_msg = "Error sending AP-REP";
	  free (out);
	  return SHISHI_IO_ERROR;
	}

      free (out);

      /* We are authenticated to client */
    }

# ifdef ENCRYPTION
  if (*protoversion == 1)
    {
      Shishi_tkt *tkt;

      tkt = shishi_ap_tkt (*ap);
      if (tkt == NULL)
	{
	  *err_msg = "Could not get tkt from AP-REQ";
	  return SHISHI_INVALID_TICKET;
	}

      rc = shishi_encticketpart_get_key (*handle,
					 shishi_tkt_encticketpart (tkt),
					 enckey);
      if (rc != SHISHI_OK)
	return rc;
    }
# endif

  return SHISHI_OK;
}

# ifdef ENCRYPTION

/* read encrypted data on socket */
int
readenc (Shishi * h, int sock, char *buf, int *len, shishi_ivector * iv,
	 Shishi_key * enckey, int proto)
{
  char *out;
  char *outbis;

  int rc;
  int val;
  size_t outlen;
  int dlen = 0, blocksize, enctype, hashsize;

  /* read size of message */
  read (sock, &dlen, sizeof (int));

  dlen = ntohl (dlen);
  /* if 0 put read size to 0 */
  if (!dlen)
    {
      *len = dlen;
      return SHISHI_OK;
    }

  if (proto == 1)
    *len = dlen;

  /* convert size to encryption size */
  enctype = shishi_key_type (enckey);

  blocksize = shishi_cipher_blocksize (enctype);
  hashsize =
    shishi_checksum_cksumlen (shishi_cipher_defaultcksumtype (enctype));

  switch (enctype)
    {
    case SHISHI_AES128_CTS_HMAC_SHA1_96:
    case SHISHI_AES256_CTS_HMAC_SHA1_96:
      dlen += 4 + hashsize + blocksize;
      break;
    case SHISHI_ARCFOUR_HMAC:
    case SHISHI_ARCFOUR_HMAC_EXP:
      dlen += 4 + 8 + blocksize - 1;
      dlen /= blocksize;
      dlen *= blocksize;
      dlen += hashsize;
      break;
    case SHISHI_DES3_CBC_HMAC_SHA1_KD:
      dlen += 4 + 2 * blocksize - 1;
      dlen /= blocksize;
      dlen *= blocksize;
      dlen += hashsize;
      break;
    case SHISHI_DES_CBC_CRC:
      dlen += 2 * blocksize - 1;
      if (proto == 2)
	dlen += 4;
      dlen += hashsize;
      dlen /= blocksize;
      dlen *= blocksize;
      break;
    default:
      dlen += blocksize - 1;
      if (proto == 2)
	dlen += 4;
      dlen += hashsize;
      dlen /= blocksize;
      dlen *= blocksize;
      break;
    }

  /* read encrypted data */
  outbis = malloc (dlen);
  if (outbis == NULL)
    {
      perror ("readenc()");
      return 1;
    }

  rc = read (sock, outbis, dlen);
  if (rc != dlen)
    {
      fprintf (stderr, "Error during read socket\n");
      free (outbis);
      return 1;
    }

  if (proto == 1)
    {
      rc =
	shishi_decrypt (h, enckey, iv->keyusage, outbis, dlen, &out, &outlen);
      if (rc != SHISHI_OK)
	{
	  fprintf (stderr, "decryption error\n");
	  free (outbis);
	  return 1;
	}

      val = 0;
    }
  else
    {
      rc = shishi_crypto_decrypt (iv->ctx, outbis, dlen, &out, &outlen);
      if (rc != SHISHI_OK)
	{
	  fprintf (stderr, "decryption error\n");
	  free (outbis);
	  return 1;
	}

      /* in KCMDV0.2 first 4 bytes of decrypted data = len of data */
      *len = ntohl (*((int *) out));
      val = sizeof (int);
    }

  memset (buf, 0, SHISHI_ENCRYPT_BUFLEN);

  /* copy decrypted data to output */
  memcpy (buf, out + val, outlen - val);

  free (out);
  free (outbis);

  return SHISHI_OK;
}

/* write encrypted data to socket */
int
writeenc (Shishi * h, int sock, char *buf, int wlen, int *len,
	  shishi_ivector * iv, Shishi_key * enckey, int proto)
{
  char *out;
  char *bufbis;

  int rc;
  int dlen;
  size_t outlen;

  dlen = wlen;
  dlen = htonl (dlen);

  /* data to encrypt = size + data */
  if (proto == 2)
    {
      bufbis = malloc (wlen + sizeof (int));
      if (!bufbis)
	{
	  perror ("writeenc");
	  return 1;
	}
      memcpy (bufbis, (char *) &dlen, sizeof (int));
      memcpy (bufbis + sizeof (int), buf, wlen);

      /* encrypt it */
      rc =
	shishi_crypto_encrypt (iv->ctx, bufbis, wlen + sizeof (int), &out,
			       &outlen);
    }
  else
    {
      bufbis = malloc (wlen);
      if (!bufbis)
	{
	  perror ("bufbis");
	  return 1;
	}
      memcpy (bufbis, buf, wlen);

      /* data to encrypt = size + data */
      rc =
	shishi_encrypt (h, enckey, iv->keyusage, bufbis, wlen, &out, &outlen);
    }

  if (rc != SHISHI_OK)
    {
      fprintf (stderr, "decryption error\n");
      free (bufbis);
      return 1;
    }

  free (bufbis);

  /* data to send = original size + encrypted data */
  /* send it */
  write (sock, &dlen, sizeof (int));
  write (sock, out, outlen);

  *len = wlen;

  free (out);

  return SHISHI_OK;


}
# endif	/* ENCRYPTION */
#endif
