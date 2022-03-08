/*
  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014,
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

#ifdef SHISHI
# include <sys/socket.h>
# include <netinet/in.h>

# define SERVICE "host"
# define SHISHI_ENCRYPT_BUFLEN 1040

struct shishi_iv
{
  char *iv;
  int ivlen;
  int keyusage;
  Shishi_crypto *ctx;
  int first;
};
typedef struct shishi_iv shishi_ivector;

struct auth_data
{
  struct sockaddr_storage from;
  socklen_t fromlen;
  char *hostaddr;
  char *hostname;
  char *lusername;
  char *rusername;
  char *rprincipal;
  char *term;
  char *env[2];
  int kerberos_version;
  int verbose;
  Shishi *h;
  Shishi_ap *ap;
  int protocol;
  Shishi_key *enckey;
  shishi_ivector iv1, iv2;
  shishi_ivector *ivtab[2];
};

extern int shishi_auth (Shishi ** handle, int verbose, char **cname,
			const char *sname, int sock, char *cmd,
			unsigned short port, Shishi_key ** enckey,
			const char *realm);

extern int get_auth (int infd, Shishi ** handle, Shishi_ap ** ap,
		     Shishi_key ** enckey, const char **err_msg,
		     int *protoversion, int *cksumtype, char **cksum,
		     size_t *cksumlen, char *srvname);

extern int readenc (Shishi * h, int sock, char *buf, int *len,
		    shishi_ivector * iv, Shishi_key * enckey, int proto);

extern int writeenc (Shishi * h, int sock, char *buf, int wlen, int *len,
		     shishi_ivector * iv, Shishi_key * enckey, int proto);

extern int krcmd (Shishi ** h, char **ahost, unsigned short rport,
		  char **remuser, char *cmd, int *fd2p,
		  const char *realm, int af);

extern int krcmd_mutual (Shishi ** h, char **ahost, unsigned short rport,
			 char **remuser, char *cmd, int *fd2p,
			 const char *realm, Shishi_key ** key, int af);

#endif
