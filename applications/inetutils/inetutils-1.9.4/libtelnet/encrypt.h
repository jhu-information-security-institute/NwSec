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

#ifdef	ENCRYPTION
# ifndef __ENCRYPTION__
#  define __ENCRYPTION__

#  define DIR_DECRYPT		1
#  define DIR_ENCRYPT		2

/* Cope with variants of <arpa/telnet.h>.  */
#  if !defined ENCTYPE_ANY && defined TELOPT_ENCTYPE_NULL
#   define ENCTYPE_ANY TELOPT_ENCTYPE_NULL
#  endif
#  if !defined ENCTYPE_DES_CFB64 && defined TELOPT_ENCTYPE_DES_CFB64
#   define ENCTYPE_DES_CFB64 TELOPT_ENCTYPE_DES_CFB64
#  endif
#  if !defined ENCTYPE_DES_OFB64 && defined TELOPT_ENCTYPE_DES_OFB64
#   define ENCTYPE_DES_OFB64 TELOPT_ENCTYPE_DES_OFB64
#  endif
#  if !defined ENCTYPE_CNT && defined TELOPT_ENCTYPE_CNT
#   define ENCTYPE_CNT TELOPT_ENCTYPE_CNT
#  endif

/*
 * Our capabilities are restricted to the encryption types
 * DES_CFB64 and DES_OFB64.  The latter type is sometimes
 * missing in <arpa/telnet.h>.  On the other hand, the same
 * header file may indicate more encryption types than are
 * supported by the present code.
 */
#  ifndef ENCTYPE_DES_OFB64
#   define ENCTYPE_DES_OFB64	2	/* RFC 2953 */
#  endif
#  undef ENCTYPE_CNT
#  define ENCTYPE_CNT	3		/* Up to DES_OFB64.  */

#  undef ENCTYPE_NAME_OK
#  define ENCTYPE_NAME_OK(x)	((unsigned int)(x) < ENCTYPE_CNT)

typedef unsigned char Block[8];
typedef unsigned char *BlockT;

#  ifndef HAVE_ARPA_TELNET_H_SCHEDULE
typedef struct
{
  Block _;
} Schedule[16];
#  endif /* !HAVE_ARPA_TELNET_H_SCHEDULE */

#  ifndef VALIDKEY
#   define VALIDKEY(key)	( key[0] | key[1] | key[2] | key[3] | \
				  key[4] | key[5] | key[6] | key[7])
#  endif

#  define SAMEKEY(k1, k2)	(!memcmp ((void *) k1, (void *) k2, sizeof (Block)))

#  ifndef HAVE_ARPA_TELNET_H_SESSION_KEY
typedef struct
{
  short type;
  int length;
  const unsigned char *data;
} Session_Key;
#  endif /* !HAVE_ARPA_TELNET_H_SESSION_KEY */

typedef struct
{
  char *name;
  int type;
  void (*output) (unsigned char *, int);
  int (*input) (int);
  void (*init) (int);
  int (*start) (int, int);
  int (*is) (unsigned char *, int);
  int (*reply) (unsigned char *, int);
  void (*session) (Session_Key *, int);
  int (*keyid) (int, unsigned char *, int *);
  void (*printsub) (unsigned char *, int, char *, int);
} Encryptions;

#  define SK_DES		1	/* Matched Kerberos v5 KEYTYPE_DES */
#  define SK_OTHER	2	/* Non-DES key. */

#  include "enc-proto.h"

extern int encrypt_debug_mode;
extern int (*decrypt_input) (int);
extern void (*encrypt_output) (unsigned char *, int);
# endif	/* __ENCRYPTION__ */
#endif /* ENCRYPTION */
