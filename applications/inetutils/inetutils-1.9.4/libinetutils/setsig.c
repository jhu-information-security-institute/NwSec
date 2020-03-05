/* setsig.c - Set a signal handler, trying to turning on the SA_RESTART bit
  Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
  2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
  Foundation, Inc.

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

/* Written by Miles Bader.  */

#include <config.h>

#include <stdlib.h>

#include <signal.h>

/* This is exactly like the traditional signal function, but turns on the
   SA_RESTART bit where possible.  */
sighandler_t
setsig (int sig, sighandler_t handler)
{
#ifdef HAVE_SIGACTION
  struct sigaction sa, osa;
  sigemptyset (&sa.sa_mask);
  sigemptyset (&osa.sa_mask);
# ifdef SA_RESTART
  sa.sa_flags |= SA_RESTART;
# endif
  sa.sa_handler = handler;
  if (sigaction (sig, &sa, &osa) < 0)
    return SIG_ERR;
  return osa.sa_handler;
#else /* !HAVE_SIGACTION */
# ifdef HAVE_SIGVEC
  struct sigvec sv, osv;
  sigemptyset (&sv.sv_mask);
  sigemptyset (&osv.sv_mask);
  sv.sv_handler = handler;
  if (sigvec (sig, &sv, &osv) < 0)
    return SIG_ERR;
  return osv.sv_handler;
# else /* !HAVE_SIGVEC */
  return signal (sig, handler);
# endif	/* HAVE_SIGVEC */
#endif /* HAVE_SIGACTION */
}
