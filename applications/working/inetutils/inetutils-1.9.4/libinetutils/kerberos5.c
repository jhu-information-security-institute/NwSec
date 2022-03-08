/* shishi.c -- functions to use Heimdal's and MIT's Kerberos V
  Copyright (C) 2014, 2015 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef KRB5
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>

# include <krb5.h>
# include "kerberos5_def.h"

/* authentication, client side */
int
kerberos_auth (krb5_context *ctx, int verbose, char **cname,
	       const char *sname, int sock, char *cmd,
	       unsigned short port, krb5_keyblock **key,
	       const char *realm)
{
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

  krb5_data cksum_data;
  krb5_principal server;
  krb5_auth_context auth_ctx = NULL;
  krb5_flags authopts = AP_OPTS_USE_SUBKEY;

  if (krb5_sname_to_principal (*ctx, sname, "host",
			       KRB5_NT_SRV_HST, &server))
    return (-1);

  /* If realm is null, look up from table */
  if (realm == NULL || realm[0] == '\0')
#  ifdef KRB5_GENERAL__ /* MIT */
    realm = (char *) krb5_princ_realm (*ctx, server);
#  else /* Heimdal */
    realm = krb5_principal_get_realm (*ctx, server);
#  endif

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
      return -1;
    }

  if (verbose)
    {
      printf ("Client: %s\n", *cname);
      printf ("Server: %s\n", sname);
    }

  /* Get a ticket for the server. */

  tmpserver = malloc (strlen (SERVICE) + strlen (sname) + 2);
  if (!tmpserver)
    {
      perror ("kerberos_auth()");
      return -1;
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
    }

  /* checksum = port: terminal name */

  cksum_data.length = snprintf (cksumdata, sizeof (cksumdata) - 1,
				"%u:%s%s", ntohs (port), cmd, *cname);

  if (strncmp (cmd, "-x ", 3) == 0)
    authopts |= AP_OPTS_MUTUAL_REQUIRED;

  cksum_data.data = cksumdata;

  rc = krb5_sendauth (*ctx, &auth_ctx, &sock, "KCMDV0.2",
		      NULL, server, authopts, &cksum_data,
		      NULL, NULL, NULL, NULL, NULL);

  if (rc == KRB5_SENDAUTH_REJECTED)
  {
    fprintf (stderr, "server rejected authentication");
    return rc;
  }

  krb5_free_principal (*ctx, server);
# if 0
  krb5_data_free (&cksum_data);
# endif

  rc = krb5_auth_con_getlocalsubkey (*ctx, auth_ctx, key);

  /* send size of AP-REQ to the server */

  msglen = outlen;
  msglen = htonl (msglen);
  write (sock, (char *) &msglen, sizeof (int));

  /* send AP-REQ to the server */

  write (sock, out, outlen);

  /* read response from server - what ? */

  read (sock, &rc, sizeof (rc));
  if (rc)
    return -1 /* SHISHI_APREP_VERIFY_FAILED */;

  /* For mutual authentication, wait for server reply. */

  /* We are now authenticated. */
  if (verbose)
    printf ("User authenticated.\n");

  return 0;

}

/* authentication, server side */
int
get_auth (int infd, krb5_context *ctx, krb5_auth_context *actx,
	  krb5_keyblock **key, const char **err_msg,
	  int *protoversion, int *cksumtype,
	  char **cksum, size_t *cksumlen, char *srvname)
{
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

# if 0
  /*
   * XXX: Taken straight from the version for libshishi.
   * XXX: No adaptions yet.
   */
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
# endif /* ENCRYPTION */

  return 0;
# else
  return -1;
# endif
}
#endif /* KRB5 */
