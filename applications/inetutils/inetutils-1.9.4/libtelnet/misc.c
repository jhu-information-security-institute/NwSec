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

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/telnet.h>

#include "auth.h"
#include "encrypt.h"
#include "misc.h"

char *RemoteHostName;
char *LocalHostName;
char *UserNameRequested = NULL;
char *ServerPrincipal;

void
auth_encrypt_init (char *local, char *remote, char *principal,
		   char *name, int server)
{
  RemoteHostName = remote;
  LocalHostName = local;
  ServerPrincipal = principal;
  (void) name;
  (void) server;		/* silence gcc */
#if defined AUTHENTICATION
  auth_init (name, server);
#endif
#ifdef	ENCRYPTION
  encrypt_init (name, server);
#endif /* ENCRYPTION */
  free (UserNameRequested);
  UserNameRequested = NULL;
}

void
auth_encrypt_user (char *name)
{
  extern char *strdup (const char *);

  free (UserNameRequested);
  UserNameRequested = name ? strdup (name) : NULL;
}

void
auth_encrypt_connect (int cnt)
{
  (void) cnt;			/* silence gcc */
}

void
printd (unsigned char *data, int cnt)
{
  if (cnt > 16)
    cnt = 16;
  while (cnt-- > 0)
    {
      printf (" %02x", *data);
      ++data;
    }
}
