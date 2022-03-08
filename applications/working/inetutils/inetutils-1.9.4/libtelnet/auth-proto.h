/*
  Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
  2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015
  Free Software Foundation, Inc.

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
 * Copyright (C) 1990, 2000 by the Massachusetts Institute of Technology
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

#if defined AUTHENTICATION
TN_Authenticator *findauthenticator (int, int);

void auth_init (char *, int);
int auth_cmd (int, char **);
void auth_request (void);
void auth_send (unsigned char *, int);
void auth_send_retry (void);
void auth_is (unsigned char *, int);
void auth_reply (unsigned char *, int);
void auth_finished (TN_Authenticator *, int);
int auth_wait (char *, size_t);
void auth_name (unsigned char *, int);
void auth_disable_name (char *);
void auth_printsub (unsigned char *, int, char *, int);
int auth_sendname (char *, int);

# ifdef	KRB4
int kerberos4_init (TN_Authenticator *, int);
int kerberos4_send (TN_Authenticator *);
void kerberos4_is (TN_Authenticator *, unsigned char *, int);
void kerberos4_reply (TN_Authenticator *, unsigned char *, int);
int kerberos4_status (TN_Authenticator *, char *, size_t, int);
void kerberos4_printsub (unsigned char *, int, char *, int);
# endif

# ifdef	KRB5
int kerberos5_init (TN_Authenticator *, int);
int kerberos5_send (TN_Authenticator *);
void kerberos5_is (TN_Authenticator *, unsigned char *, int);
void kerberos5_reply (TN_Authenticator *, unsigned char *, int);
int kerberos5_status (TN_Authenticator *, char *, size_t, int);
void kerberos5_printsub (unsigned char *, int, char *, int);
# endif

# ifdef	SHISHI
int krb5shishi_init (TN_Authenticator *, int);
int krb5shishi_send (TN_Authenticator *);
void krb5shishi_is (TN_Authenticator *, unsigned char *, int);
void krb5shishi_reply (TN_Authenticator *, unsigned char *, int);
int krb5shishi_status (TN_Authenticator *, char *, size_t, int);
void krb5shishi_printsub (unsigned char *, int, char *, int);
void krb5shishi_cleanup (TN_Authenticator *);
# endif
#endif
