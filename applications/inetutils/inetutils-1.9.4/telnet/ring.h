/*
  Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
  2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014,
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

/*
 * Copyright (c) 1988, 1993
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
 * This defines a structure for a ring buffer.
 *
 * The circular buffer has two parts:
 *(((
 *	full:	[consume, supply)
 *	empty:	[supply, consume)
 *]]]
 *
 */
typedef struct
{
  unsigned char *consume,	/* where data comes out of */
   *supply,			/* where data comes in to */
   *bottom,			/* lowest address in buffer */
   *top,			/* highest address+1 in buffer */
   *mark;			/* marker (user defined) */
#ifdef	ENCRYPTION
  unsigned char *clearto;	/* Data to this point is clear text */
  unsigned char *encryyptedto;	/* Data is encrypted to here */
#endif				/* ENCRYPTION */
  int size;			/* size in bytes of buffer */
  unsigned long consumetime,		/* help us keep straight full, empty, etc. */
    supplytime;
} Ring;

/* Here are some functions and macros to deal with the ring buffer */

/* Initialization routine */
extern int ring_init (Ring * ring, unsigned char *buffer, int count);

/* Data movement routines */
extern void ring_supply_data (Ring * ring, unsigned char *buffer, int count);

/* Buffer state transition routines */
extern void
ring_supplied (Ring * ring, int count),
ring_consumed (Ring * ring, int count);

/* Buffer state query routines */
extern int
ring_empty_count (Ring * ring),
ring_empty_consecutive (Ring * ring),
ring_full_count (Ring * ring), ring_full_consecutive (Ring * ring);

#ifdef	ENCRYPTION
extern void
ring_encrypt (Ring * ring, void (*func) ()), ring_clearto (Ring * ring);
#endif /* ENCRYPTION */

extern void ring_clear_mark (Ring *);
extern void ring_mark (Ring *);
extern int ring_at_mark (Ring *);
