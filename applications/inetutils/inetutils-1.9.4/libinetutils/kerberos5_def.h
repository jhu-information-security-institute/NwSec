/*
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

#ifdef KRB5
# include <sys/socket.h>
# include <netinet/in.h>

# ifdef HAVE_KRB5_h
#  include <krb5.h>
# endif

# define SERVICE "host"

extern int kerberos_auth (krb5_context *ctx, int verbose, char **cname,
			  const char *sname, int sock, char *cmd,
			  unsigned short port, krb5_keyblock **key,
			  const char *realm);

extern int get_auth (int infd, krb5_context *ctx, krb5_auth_context *actx,
		     krb5_keyblock **key, const char **err_msg,
		     int *protoversion, int *cksumtype, char **cksum,
		     size_t *cksumlen, char *srvname);

extern int kcmd (krb5_context *ctx, int *sock, char **ahost,
		 unsigned short rport, char *locuser, char **remuser,
		 char *cmd, int *fd2p, char *service, const char *realm,
		 krb5_keyblock **key, struct sockaddr_in *laddr,
		 struct sockaddr_in *raddr, long opts);

extern int krcmd (krb5_context *ctx, char **ahost, unsigned short rport,
		  char **remuser, char *cmd, int *fd2p,
		  const char *realm);

extern int krcmd_mutual (krb5_context *ctx, char **ahost,
			 unsigned short rport, char **remuser,
			 char *cmd, int *fd2p, const char *realm,
			 krb5_keyblock **key);

#endif	/* KRB5 */
