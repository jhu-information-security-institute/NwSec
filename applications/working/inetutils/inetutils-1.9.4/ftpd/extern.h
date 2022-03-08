/*
  Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
  2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013,
  2014, 2015 Free Software Foundation, Inc.

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
 * Copyright (c) 1992, 1993
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

#include <stdio.h>
#include <setjmp.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

extern void cwd (const char *);
extern int checkuser (const char *filename, const char *name);
extern void delete (const char *);
extern int display_file (const char *name, int code);
extern void dologout (int);
extern void fatal (const char *);
extern int ftpd_pclose (FILE *);
extern FILE *ftpd_popen (char *, const char *);
#if !HAVE_DECL_GETUSERSHELL
extern char *getusershell (void);
#endif
extern void lreply (int, const char *, ...);
extern void lreply_multiline (int n, const char *text);
extern void makedir (const char *);
extern void nack (const char *);
extern void pass (const char *);
extern void passive (int, int);
extern void perror_reply (int, const char *);
extern void pwd (void);
extern void removedir (const char *);
extern void renamecmd (const char *, const char *);
extern char *renamefrom (const char *);
extern void reply (int, const char *, ...);
extern void retrieve (const char *, const char *);
extern void send_file_list (const char *);
extern void setproctitle (const char *, ...);
extern void statcmd (void);
extern void statfilecmd (const char *);
extern void store (const char *, const char *, int);
extern void toolong (int);
extern char *telnet_fgets (char *, int, FILE *);
extern void upper (char *);
extern void user (const char *);
extern char *sgetsave (const char *);

/* Exported from ftpd.c.  */
jmp_buf errcatch;
extern struct sockaddr_storage data_dest;
extern socklen_t data_dest_len;
extern struct sockaddr_storage his_addr;
extern socklen_t his_addrlen;
extern struct passwd *pw;
extern int guest;
extern int logging;
extern int no_version;
extern int type;
extern int form;
extern int debug;
extern int rfc2577;
extern int timeout;
extern int maxtimeout;
extern int pdata;
extern char *hostname;
extern char *remotehost;
extern char proctitle[];
extern int usedefault;
extern char tmpline[];

/* Exported from ftpcmd.y.  */
extern off_t restart_point;

/* Distinguish passive address modes.  */
#define PASSIVE_PASV 0
#define PASSIVE_EPSV 1
#define PASSIVE_LPSV 2

/* Exported from server_mode.c.  */
extern int usefamily;
extern int server_mode (const char *pidfile, struct sockaddr *phis_addr,
			socklen_t *phis_addrlen, char *argv[]);

/* Credential for the request.  */
struct credentials
{
  char *name;
  char *homedir;
  char *rootdir;
  char *shell;
  char *remotehost;
  char *passwd;
  char *pass;
  char *message;		/* Sending back custom messages.  */
  uid_t uid;
  gid_t gid;
  int guest;
  int dochroot;
  int logged_in;
  int delayed_reject;
#define AUTH_EXPIRED_NOT    0
#define AUTH_EXPIRED_ACCT   1
#define AUTH_EXPIRED_PASS   2
  int expired;
#define AUTH_TYPE_PASSWD    0
#define AUTH_TYPE_PAM       1
#define AUTH_TYPE_KERBEROS  2
#define AUTH_TYPE_KERBEROS5 3
#define AUTH_TYPE_OPIE      4
  int auth_type;
};

/* Exported from ftpd.c */
extern struct credentials cred;

/* Exported from auth.c */
extern int sgetcred (const char *, struct credentials *);
extern int auth_user (const char *, struct credentials *);
extern int auth_pass (const char *, struct credentials *);

/* Exported from pam.c */
#ifdef WITH_PAM
extern int pam_user (const char *, struct credentials *);
extern int pam_pass (const char *, struct credentials *);
extern void pam_end_login (struct credentials *);
#endif /* WITH_PAM */
