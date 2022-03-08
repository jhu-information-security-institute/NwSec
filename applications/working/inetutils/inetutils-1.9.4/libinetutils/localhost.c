/* localhost.c - A slightly more convenient wrapper for gethostname
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

/* Written by Miles Bader.  */

#include <config.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/* Return the name of the localhost.  This is just a wrapper for gethostname,
   taking care of allocating a big enough buffer, and caching the result
   after the first call (so the result should be copied before modification).
   If something goes wrong, 0 is returned, and errno is set.
 */
/* We no longer use static buffers, as it is too dangerous and
   it causes subtile bugs.  */
char *
localhost (void)
{
  char *buf = NULL;
  size_t buf_len = 0;
  int status = 0;

  do
    {
      char *tmp;
      errno = 0;

      buf_len += 256;		/* Initial guess */
      tmp = realloc (buf, buf_len);
      if (tmp == NULL)
	{
	  errno = ENOMEM;
	  free (buf);
	  return 0;
	}
      else
	buf = tmp;
    }
  while (((status = gethostname (buf, buf_len)) == 0
	  && !memchr (buf, '\0', buf_len))
#ifdef ENAMETOOLONG
	 || errno == ENAMETOOLONG
#endif
    );

  if (status != 0 && errno != 0)
    /* gethostname failed, abort.  */
    {
      free (buf);
      buf = 0;
    }
  else
    /* Determine FQDN.  */
    {
#if HAVE_DECL_GETADDRINFO
      int rc;
      struct addrinfo hints, *res;
      char fqdn[NI_MAXHOST];

      memset (&hints, 0, sizeof (hints));
      hints.ai_family = AF_INET;

      rc = getaddrinfo (buf, NULL, &hints, &res);
      if (!rc)
	{
	  /* Back resolving as official host name.  */
	  rc = getnameinfo (res->ai_addr, res->ai_addrlen,
			    fqdn, sizeof (fqdn), NULL, 0,
			    NI_NAMEREQD);
	  if (!rc)
	    {
	      free (buf);
	      buf = strdup (fqdn);
	    }
	  freeaddrinfo (res);
	}
#else /* !HAVE_DECL_GETADDRINFO */
      struct hostent *hp = gethostbyname (buf);

      if (hp)
	{
	  struct in_addr addr;
	  addr.s_addr = *(unsigned int *) hp->h_addr;
	  hp = gethostbyaddr ((char *) &addr, sizeof (addr), AF_INET);
	  if (hp)
	    {
	      free (buf);
	      buf = strdup (hp->h_name);
	    }
	}
#endif /* !HAVE_DECL_GETADDRINFO */
    }
  return buf;
}
