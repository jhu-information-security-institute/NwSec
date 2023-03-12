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

#ifdef	ENCRYPTION
# ifdef	AUTHENTICATION
#  if defined DES_ENCRYPTION || defined SHISHI
#   ifdef SHISHI
#    include <shishi.h>
extern Shishi *shishi_handle;
#   endif
#   include <arpa/telnet.h>
#   include <stdio.h>
#   include <stdlib.h>

#   include "encrypt.h"
#   include "key-proto.h"
#   include "misc-proto.h"

extern int encrypt_debug_mode;

#   define CFB	0
#   define OFB	1

#   define NO_SEND_IV	1
#   define NO_RECV_IV	2
#   define NO_KEYID	4
#   define IN_PROGRESS	(NO_SEND_IV|NO_RECV_IV|NO_KEYID)
#   define SUCCESS	0
#   define FAILED	-1


#   include <string.h>

struct fb
{
  Block krbdes_key;
  Schedule krbdes_sched;
  Block temp_feed;
  unsigned char fb_feed[64];
  int need_start;
  int state[2];
  int keyid[2];
  int once;
  struct stinfo
  {
    Block str_output;
    Block str_feed;
    Block str_iv;
    Block str_ikey;
    Schedule str_sched;
    int str_index;
    int str_flagshift;
  } streams[2];
};

static struct fb fb[2];

struct keyidlist
{
  char *keyid;
  int keyidlen;
  char *key;
  int keylen;
  int flags;
} keyidlist[] =
{
  {"\0", 1, 0, 0, 0},		/* default key of zero */
  {0, 0, 0, 0, 0}
};

#   define KEYFLAG_MASK	03

#   define KEYFLAG_NOINIT	00
#   define KEYFLAG_INIT	01
#   define KEYFLAG_OK	02
#   define KEYFLAG_BAD	03

#   define KEYFLAG_SHIFT	2

#   define SHIFT_VAL(a,b)	(KEYFLAG_SHIFT*((a)+((b)*2)))

#   ifndef FB64_IV
#    define FB64_IV	1
#   endif
#   ifndef FB64_IV_OK
#    define FB64_IV_OK	2
#   endif
#   ifndef FB64_IV_BAD
#    define FB64_IV_BAD	3
#   endif


/* Callback from consumer.  */
extern void printsub (char, unsigned char *, int);

static void fb64_stream_iv (Block, struct stinfo *);
static void fb64_init (struct fb *);
static int fb64_start (struct fb *, int, int);
static int fb64_is (unsigned char *, int, struct fb *);
static int fb64_reply (unsigned char *, int, struct fb *);
static void fb64_session (Session_Key *, int, struct fb *);
static void fb64_stream_key (Block, struct stinfo *);
static int fb64_keyid (int, unsigned char *, int *, struct fb *);
static int des_check_parity (Block b);
static int des_set_parity (Block b);

#   ifdef SHISHI
static void
shishi_des_ecb_encrypt (Shishi * h, const unsigned char key[sizeof (Block)],
			const unsigned char *in, unsigned char *out)
{
  char *tmp;

  shishi_des (h, 0, (const char *) key, NULL, NULL,
	      (const char *) in, sizeof (Block), &tmp);
  memcpy (out, tmp, sizeof (Block));
  free (tmp);
}
#   endif

void
cfb64_init (int server)
{
  fb64_init (&fb[CFB]);
  fb[CFB].fb_feed[4] = ENCTYPE_DES_CFB64;
  fb[CFB].streams[0].str_flagshift = SHIFT_VAL (0, CFB);
  fb[CFB].streams[1].str_flagshift = SHIFT_VAL (1, CFB);
}

#   ifdef ENCTYPE_DES_OFB64
void
ofb64_init (int server)
{
  fb64_init (&fb[OFB]);
  fb[OFB].fb_feed[4] = ENCTYPE_DES_OFB64;
  fb[CFB].streams[0].str_flagshift = SHIFT_VAL (0, OFB);
  fb[CFB].streams[1].str_flagshift = SHIFT_VAL (1, OFB);
}
#   endif /* ENCTYPE_DES_OFB64 */

static void
fb64_init (register struct fb *fbp)
{
  memset ((void *) fbp, 0, sizeof (*fbp));
  fbp->state[0] = fbp->state[1] = FAILED;
  fbp->fb_feed[0] = IAC;
  fbp->fb_feed[1] = SB;
  fbp->fb_feed[2] = TELOPT_ENCRYPT;
  fbp->fb_feed[3] = ENCRYPT_IS;
}

/*
 * Returns:
 *	-1: some error.  Negotiation is done, encryption not ready.
 *	 0: Successful, initial negotiation all done.
 *	 1: successful, negotiation not done yet.
 *	 2: Not yet.  Other things (like getting the key from
 *	    Kerberos) have to happen before we can continue.
 */
int
cfb64_start (int dir, int server)
{
  return (fb64_start (&fb[CFB], dir, server));
}

#   ifdef ENCTYPE_DES_OFB64
int
ofb64_start (int dir, int server)
{
  return (fb64_start (&fb[OFB], dir, server));
}
#   endif /* ENCTYPE_DES_OFB64 */

static int
fb64_start (struct fb *fbp, int dir, int server)
{
  size_t x;
  unsigned char *p;
  register int state;

  switch (dir)
    {
    case DIR_DECRYPT:
      /*
       * This is simply a request to have the other side
       * start output (our input).  He will negotiate an
       * IV so we need not look for it.
       */
      state = fbp->state[dir - 1];
      if (state == FAILED)
	state = IN_PROGRESS;
      break;

    case DIR_ENCRYPT:
      state = fbp->state[dir - 1];
      if (state == FAILED)
	state = IN_PROGRESS;
      else if ((state & NO_SEND_IV) == 0)
	break;

      if (!VALIDKEY (fbp->krbdes_key))
	{
	  fbp->need_start = 1;
	  break;
	}
      state &= ~NO_SEND_IV;
      state |= NO_RECV_IV;
      if (encrypt_debug_mode)
	printf ("Creating new feed\r\n");
      /*
       * Create a random feed and send it over.
       */
#   ifdef SHISHI
      if (shishi_randomize (shishi_handle, 0,
			    fbp->temp_feed, sizeof (Block)) != SHISHI_OK)
	return (FAILED);

#   else
      des_new_random_key (fbp->temp_feed);
      des_ecb_encrypt (fbp->temp_feed, fbp->temp_feed, fbp->krbdes_sched, 1);
#   endif
      p = fbp->fb_feed + 3;
      *p++ = ENCRYPT_IS;
      p++;
      *p++ = FB64_IV;
      for (x = 0; x < sizeof (Block); ++x)
	{
	  if ((*p++ = fbp->temp_feed[x]) == IAC)
	    *p++ = IAC;
	}
      *p++ = IAC;
      *p++ = SE;
      printsub ('>', &fbp->fb_feed[2], p - &fbp->fb_feed[2]);
      net_write (fbp->fb_feed, p - fbp->fb_feed);
      break;
    default:
      return (FAILED);
    }

  fbp->state[dir - 1] = state;

  return (state);
}

/*
 * Returns:
 *	-1: some error.  Negotiation is done, encryption not ready.
 *	 0: Successful, initial negotiation all done.
 *	 1: successful, negotiation not done yet.
 */
int
cfb64_is (unsigned char *data, int cnt)
{
  return (fb64_is (data, cnt, &fb[CFB]));
}

#   ifdef ENCTYPE_DES_OFB64
int
ofb64_is (unsigned char *data, int cnt)
{
  return (fb64_is (data, cnt, &fb[OFB]));
}
#   endif /* ENCTYPE_DES_OFB64 */

static int
fb64_is (unsigned char *data, int cnt, struct fb *fbp)
{
  unsigned char *p;
  register int state = fbp->state[DIR_DECRYPT - 1];

  if (cnt-- < 1)
    goto failure;

  switch (*data++)
    {
    case FB64_IV:
      if (cnt != sizeof (Block))
	{
	  if (encrypt_debug_mode)
	    printf ("FB64: initial vector failed on size\r\n");
	  state = FAILED;
	  goto failure;
	}

      if (encrypt_debug_mode)
	printf ("FB64: initial vector received\r\n");

      if (encrypt_debug_mode)
	printf ("Initializing Decrypt stream\r\n");

      fb64_stream_iv ((void *) data, &fbp->streams[DIR_DECRYPT - 1]);

      p = fbp->fb_feed + 3;
      *p++ = ENCRYPT_REPLY;
      p++;
      *p++ = FB64_IV_OK;
      *p++ = IAC;
      *p++ = SE;
      printsub ('>', &fbp->fb_feed[2], p - &fbp->fb_feed[2]);
      net_write (fbp->fb_feed, p - fbp->fb_feed);

      state = fbp->state[DIR_DECRYPT - 1] = IN_PROGRESS;
      break;

    default:
      if (encrypt_debug_mode)
	{
	  printf ("Unknown option type: %d\r\n", *(data - 1));
	  printd (data, cnt);
	  printf ("\r\n");
	}
      /* FALL THROUGH */
    failure:
      /*
       * We failed.  Send an FB64_IV_BAD option
       * to the other side so it will know that
       * things failed.
       */
      p = fbp->fb_feed + 3;
      *p++ = ENCRYPT_REPLY;
      p++;
      *p++ = FB64_IV_BAD;
      *p++ = IAC;
      *p++ = SE;
      printsub ('>', &fbp->fb_feed[2], p - &fbp->fb_feed[2]);
      net_write (fbp->fb_feed, p - fbp->fb_feed);

      break;
    }

  fbp->state[DIR_DECRYPT - 1] = state;

  return (state);
}

/*
 * Returns:
 *	-1: some error.  Negotiation is done, encryption not ready.
 *	 0: Successful, initial negotiation all done.
 *	 1: successful, negotiation not done yet.
 */
int
cfb64_reply (unsigned char *data, int cnt)
{
  return (fb64_reply (data, cnt, &fb[CFB]));
}

#   ifdef ENCTYPE_DES_OFB64
int
ofb64_reply (unsigned char *data, int cnt)
{
  return (fb64_reply (data, cnt, &fb[OFB]));
}
#   endif /* ENCTYPE_DES_OFB64 */


static int
fb64_reply (unsigned char *data, int cnt, struct fb *fbp)
{
  register int state = fbp->state[DIR_ENCRYPT - 1];

  if (cnt-- < 1)
    goto failure;

  switch (*data++)
    {
    case FB64_IV_OK:
      fb64_stream_iv (fbp->temp_feed, &fbp->streams[DIR_ENCRYPT - 1]);
      if (state == FAILED)
	state = IN_PROGRESS;
      state &= ~NO_RECV_IV;
      encrypt_send_keyid (DIR_ENCRYPT, (unsigned char *) "\0", 1, 1);
      break;

    case FB64_IV_BAD:
      memset (fbp->temp_feed, 0, sizeof (Block));
      fb64_stream_iv (fbp->temp_feed, &fbp->streams[DIR_ENCRYPT - 1]);
      state = FAILED;
      break;

    default:
      if (encrypt_debug_mode)
	{
	  printf ("Unknown option type: %d\r\n", data[-1]);
	  printd (data, cnt);
	  printf ("\r\n");
	}
      /* FALL THROUGH */
    failure:
      state = FAILED;
      break;
    }

  fbp->state[DIR_ENCRYPT - 1] = state;

  return (state);
}

void
cfb64_session (Session_Key *key, int server)
{
  fb64_session (key, server, &fb[CFB]);
}

#   ifdef ENCTYPE_DES_OFB64
void
ofb64_session (Session_Key *key, int server)
{
  fb64_session (key, server, &fb[OFB]);
}
#   endif /* ENCTYPE_DES_OFB64 */

static void
fb64_session (Session_Key *key, int server, struct fb *fbp)
{
  size_t offset;
  unsigned char *derived_key;

  if (!key || key->type != SK_DES)
    {
      /* FIXME: Support RFC 2952 approach instead of giving up here. */
      if (encrypt_debug_mode)
	printf ("Received non-DES session key (%d != %d)\r\n",
		key ? key->type : -1, SK_DES);
      if (!key)
	return;	/* XXX: Causes a segfault, since *key is NULL.  */

      /* Follow RFC 2952 in using the authentication key
       * to derived one or more DES-keys, after adjusting
       * the parity in each eight byte block.
       */
    }

  /* Make a copy of the authentication key,
   * since the parity might need mending.  */
  derived_key = malloc (key->length);
  if (!derived_key)
    return;	/* Still destructive, but no alternate method in sight.  */

  memmove ((void *) derived_key, (void *) key->data, key->length);

  /* Check parity of each DES block, correct it whenever needed.  */
  for (offset = 0; offset < (size_t) key->length; offset += sizeof (Block))
    (void) des_set_parity (derived_key + offset);

  /* XXX: A single key block is in use for now,
   *      but all block are of correct parity.
   *      krbdes_key should be an array of block,
   *      which each encryption method may use at
   *      it own discretion.  This is the content
   *      if RFC 2946 and 2952, etcetera.
   */
  memmove ((void *) fbp->krbdes_key, (void *) derived_key, sizeof (Block));

  /* XXX: These should at least be split according
   *      to direction and role, i.e., client or server.
   */
  fb64_stream_key (fbp->krbdes_key, &fbp->streams[DIR_ENCRYPT - 1]);
  fb64_stream_key (fbp->krbdes_key, &fbp->streams[DIR_DECRYPT - 1]);

  /* Erase sensitive key material.  */
  memset (derived_key, 0, key->length);

  if (fbp->once == 0)
    {
#   ifndef SHISHI
      des_set_random_generator_seed (fbp->krbdes_key);
#   endif
      fbp->once = 1;
    }
#   ifndef SHISHI
  des_key_sched (fbp->krbdes_key, fbp->krbdes_sched);
#   endif
  /*
   * Now look to see if krbdes_start() was waiting for
   * the key to show up.  If so, go ahead and call it now
   * that we have the key.
   */
  if (fbp->need_start)
    {
      fbp->need_start = 0;
      fb64_start (fbp, DIR_ENCRYPT, server);
    }
}

/*
 * We only accept a keyid of 0.  If we get a keyid of
 * 0, then mark the state as SUCCESS.
 */
int
cfb64_keyid (int dir, unsigned char *kp, int *lenp)
{
  return (fb64_keyid (dir, kp, lenp, &fb[CFB]));
}

#   ifdef ENCTYPE_DES_OFB64
int
ofb64_keyid (int dir, unsigned char *kp, int *lenp)
{
  return (fb64_keyid (dir, kp, lenp, &fb[OFB]));
}
#   endif /* ENCTYPE_DES_OFB64 */

static int
fb64_keyid (int dir, unsigned char *kp, int *lenp, struct fb *fbp)
{
  register int state = fbp->state[dir - 1];

  if (*lenp != 1 || (*kp != '\0'))
    {
      *lenp = 0;
      return (state);
    }

  if (state == FAILED)
    state = IN_PROGRESS;

  state &= ~NO_KEYID;

  fbp->state[dir - 1] = state;

  return (state);
}

static void
fb64_printsub (unsigned char *data, int cnt,
	       char *buf, int buflen,
	       const char *type)
{
  char lbuf[32];
  register int i;
  char *cp;

  buf[buflen - 1] = '\0';	/* make sure it's NULL terminated */
  buflen -= 1;

  switch (data[2])
    {
    case FB64_IV:
      sprintf (lbuf, "%s_IV", type);
      cp = lbuf;
      goto common;

    case FB64_IV_OK:
      sprintf (lbuf, "%s_IV_OK", type);
      cp = lbuf;
      goto common;

    case FB64_IV_BAD:
      sprintf (lbuf, "%s_IV_BAD", type);
      cp = lbuf;
      goto common;

    default:
      sprintf (lbuf, " %d (unknown)", data[2]);
      cp = lbuf;
    common:
      for (; (buflen > 0) && (*buf = *cp++); buf++)
	buflen--;
      for (i = 3; i < cnt; i++)
	{
	  sprintf (lbuf, " %d", data[i]);
	  for (cp = lbuf; (buflen > 0) && (*buf = *cp++); buf++)
	    buflen--;
	}
      break;
    }
}

void
cfb64_printsub (unsigned char *data, int cnt,
		char *buf, int buflen)
{
  fb64_printsub (data, cnt, buf, buflen, "CFB64");
}

#   ifdef ENCTYPE_DES_OFB64
void
ofb64_printsub (unsigned char *data, int cnt,
		char *buf, int buflen)
{
  fb64_printsub (data, cnt, buf, buflen, "OFB64");
}
#   endif /* ENCTYPE_DES_OFB64 */

static void
fb64_stream_iv (Block seed, register struct stinfo *stp)
{

  memmove ((void *) stp->str_iv, (void *) seed, sizeof (Block));
  memmove ((void *) stp->str_output, (void *) seed, sizeof (Block));

#   ifndef SHISHI
  des_key_sched (stp->str_ikey, stp->str_sched);
#   endif

  stp->str_index = sizeof (Block);
}

static void
fb64_stream_key (Block key, register struct stinfo *stp)
{
  memmove ((void *) stp->str_ikey, (void *) key, sizeof (Block));
#   ifndef SHISHI
  des_key_sched (key, stp->str_sched);
#   endif
  memmove ((void *) stp->str_output, (void *) stp->str_iv, sizeof (Block));

  stp->str_index = sizeof (Block);
}

/*
 * DES 64 bit Cipher Feedback
 *
 *     key --->+-----+
 *          +->| DES |--+
 *          |  +-----+  |
 *	    |           v
 *  INPUT --(--------->(+)+---> DATA
 *          |             |
 *	    +-------------+
 *
 *
 * Given:
 *	iV: Initial vector, 64 bits (8 bytes) long.
 *	Dn: the nth chunk of 64 bits (8 bytes) of data to encrypt (decrypt).
 *	On: the nth chunk of 64 bits (8 bytes) of encrypted (decrypted) output.
 *
 *	V0 = DES(iV, key)
 *	On = Dn ^ Vn
 *	V(n+1) = DES(On, key)
 */

void
cfb64_encrypt (register unsigned char *s, int c)
{
  register struct stinfo *stp = &fb[CFB].streams[DIR_ENCRYPT - 1];
  register int index;

  index = stp->str_index;
  while (c-- > 0)
    {
      if (index == sizeof (Block))
	{
	  Block b;
#   ifdef SHISHI
	  shishi_des_ecb_encrypt (shishi_handle, fb[CFB].krbdes_key,
				  stp->str_output, b);
#   else
	  des_ecb_encrypt (stp->str_output, b, stp->str_sched, 1);
#   endif
	  memmove ((void *) stp->str_feed, (void *) b, sizeof (Block));
	  index = 0;
	}

      /* On encryption, we store (feed ^ data) which is cypher */
      *s = stp->str_output[index] = (stp->str_feed[index] ^ *s);
      s++;
      index++;
    }
  stp->str_index = index;
}

int
cfb64_decrypt (int data)
{
  register struct stinfo *stp = &fb[CFB].streams[DIR_DECRYPT - 1];
  int index;

  if (data == -1)
    {
      /*
       * Back up one byte.  It is assumed that we will
       * never back up more than one byte.  If we do, this
       * may or may not work.
       */
      if (stp->str_index)
	--stp->str_index;
      return (0);
    }

  index = stp->str_index++;
  if (index == sizeof (Block))
    {
      Block b;
#   ifdef SHISHI
      shishi_des_ecb_encrypt (shishi_handle, fb[CFB].krbdes_key,
			      stp->str_output, b);
#   else
      des_ecb_encrypt (stp->str_output, b, stp->str_sched, 1);
#   endif
      memmove ((void *) stp->str_feed, (void *) b, sizeof (Block));
      stp->str_index = 1;	/* Next time will be 1 */
      index = 0;		/* But now use 0 */
    }

  /* On decryption we store (data) which is cypher. */
  stp->str_output[index] = data;

  return (data ^ stp->str_feed[index]);
}

/*
 * DES 64 bit Output Feedback
 *
 * key --->+-----+
 *	+->| DES |--+
 *	|  +-----+  |
 *	+-----------+
 *	            v
 *  INPUT -------->(+) ----> DATA
 *
 * Given:
 *	iV: Initial vector, 64 bits (8 bytes) long.
 *	Dn: the nth chunk of 64 bits (8 bytes) of data to encrypt (decrypt).
 *	On: the nth chunk of 64 bits (8 bytes) of encrypted (decrypted) output.
 *
 *	V0 = DES(iV, key)
 *	V(n+1) = DES(Vn, key)
 *	On = Dn ^ Vn
 */
#   ifdef ENCTYPE_DES_OFB64
void
ofb64_encrypt (register unsigned char *s, int c)
{
  register struct stinfo *stp = &fb[OFB].streams[DIR_ENCRYPT - 1];
  register int index;

  index = stp->str_index;
  while (c-- > 0)
    {
      if (index == sizeof (Block))
	{
	  Block b;
#    ifdef SHISHI
	  shishi_des_ecb_encrypt (shishi_handle, fb[OFB].krbdes_key,
				  stp->str_feed, b);
#    else
	  des_ecb_encrypt (stp->str_feed, b, stp->str_sched, 1);
#    endif
	  memmove ((void *) stp->str_feed, (void *) b, sizeof (Block));
	  index = 0;
	}
      *s++ ^= stp->str_feed[index];
      index++;
    }
  stp->str_index = index;
}

int
ofb64_decrypt (int data)
{
  register struct stinfo *stp = &fb[OFB].streams[DIR_DECRYPT - 1];
  int index;

  if (data == -1)
    {
      /*
       * Back up one byte.  It is assumed that we will
       * never back up more than one byte.  If we do, this
       * may or may not work.
       */
      if (stp->str_index)
	--stp->str_index;
      return (0);
    }

  index = stp->str_index++;
  if (index == sizeof (Block))
    {
      Block b;
#    ifdef SHISHI
      shishi_des_ecb_encrypt (shishi_handle, fb[OFB].krbdes_key,
			      stp->str_feed, b);
#    else
      des_ecb_encrypt (stp->str_feed, b, stp->str_sched, 1);
#    endif
      memmove ((void *) stp->str_feed, (void *) b, sizeof (Block));
      stp->str_index = 1;	/* Next time will be 1 */
      index = 0;		/* But now use 0 */
    }

  return (data ^ stp->str_feed[index]);
}
#   endif /* ENCTYPE_DES_OFB64 */

static int
des_parity (Block b, int adjust)
{
  size_t index;
  int adj = 0;

  for (index = 0; index < sizeof (Block); index++)
    {
      unsigned char c = b[index];

      c ^= (c >> 4);
      c ^= (c >> 2);
      c ^= (c >> 1);

      if (!(c & 1))
	{
	  /* Even parity.  */
	  adj++;
	  if (adjust)
	    *(&b[index]) ^= 0x01;
	}
    }

  return adj;
}

/*
 * Returns:
 *	 0: Correct parity in full key block.
 *	 n: Count of corrected bytes.
 */

static int
des_check_parity (Block b)
{
  return des_parity (b, 0);
}

static int
des_set_parity (Block b)
{
  return des_parity (b, 1);
}

#  endif /* DES_ENCRYPTION || SHISHI */
# endif	/* AUTHENTICATION */
#endif /* ENCRYPTION */
