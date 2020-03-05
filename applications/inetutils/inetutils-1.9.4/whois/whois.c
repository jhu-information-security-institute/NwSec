/*
  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
  2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

/* Written by Marco d'Itri.  */

#include <config.h>
/* System library */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <progname.h>
#include <argp.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>
#include <error.h>
#include <unused-parameter.h>
#include <libinetutils.h>

/* Application-specific */
#include <data.h>
#include <whois.h>
#include "xalloc.h"

/* Global variables */
int sockfd, verb = 0;
struct obstack query_stk;

#ifdef ALWAYS_HIDE_DISCL
int hide_discl = 0;
#else
int hide_discl = 2;
#endif
const char *server = NULL;
const char *port = NULL;
int nopar = 0;

static struct argp_option ripe_argp_options[] = {
#define GRP 10
  { NULL, 'a', NULL, 0,
    "search all databases", GRP },
  { NULL, 'F', NULL, 0,
    "fast raw output (implies -r)", GRP },
  { NULL, 'g', "SOURCE:FIRST-LAST", 0,
    "find updates from SOURCE from serial FIRST to LAST", GRP },
  { NULL, 'i', "ATTR[,ATTR]...", 0,
    "do an inverse lookup for specified ATTRibutes", GRP },
  { NULL, 'l', NULL, 0,
    "one level less specific lookup (RPSL only)", GRP },
  { NULL, 'L', NULL, 0,
    "find all Less specific matches", GRP },
  { NULL, 'M', NULL, 0,
    "find all More specific matches", GRP },
  { NULL, 'm', NULL, 0,
    "find first level more specific matches", GRP },
  { NULL, 'r', NULL, 0,
    "turn off recursive lookups", GRP },
  { NULL, 'R', NULL, 0,
    "force to show local copy of the domain object even "
    "if it contains referral", GRP },
  { NULL, 'S', NULL, 0,
    "tell server to leave out syntactic sugar", GRP },
  { NULL, 's', "SOURCE[,SOURCE]...", 0,
    "search the database from SOURCE", GRP },
  { NULL, 'T', "TYPE[,TYPE]...", 0,
    "only look for objects of TYPE", GRP },
  { NULL, 'q', "version|sources", 0,
    "query specified server info (RPSL only)", GRP },
  { NULL, 't', "TYPE", 0,
    "requests template for object of TYPE ('all' for a list)", GRP },
  { NULL, 'x', NULL, 0,
    "exact match only (RPSL only)", GRP },
#undef GRP
  { NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
ripe_argp_parser (int key, char *arg,
		  struct argp_state *state _GL_UNUSED_PARAMETER)
{
  if (key > 0 && (unsigned) key < 128)
    {
      if (key == 't' || key == 'v' || key == 'q')
	nopar = 1;

      obstack_1grow (&query_stk, '-');
      obstack_1grow (&query_stk, key);
      if (arg)
	{
	  obstack_1grow (&query_stk, ' ');
	  obstack_grow (&query_stk, arg, strlen (arg));
	}
      return 0;
    }
  return ARGP_ERR_UNKNOWN;
}

static struct argp ripe_argp =
  { ripe_argp_options, ripe_argp_parser, NULL, NULL, NULL, NULL, NULL };

static struct argp_option gwhois_argp_options[] = {
#define GRP 20
  { NULL, 0, NULL, 0, "General options", GRP },
  { "verbose", 'V', NULL, 0,
    "explain what is being done", GRP },
  { "server", 'h', "HOST", 0,
    "connect to server HOST", GRP},
  { "port", 'p', "PORT", 0,
    "connect to PORT", GRP },
  { NULL, 'H', NULL, 0,
    "hide legal disclaimers", GRP },
#undef GRP
  { NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
gwhois_argp_parser (int key, char *arg,
		    struct argp_state *state _GL_UNUSED_PARAMETER)
{
  char *p, *q;

  switch (key)
    {
    case 'h':
      server = q = xmalloc (strlen (arg) + 1);
      for (p = arg; *p != '\0' && *p != ':'; *q++ = tolower (*p++));
      if (*p == ':')
	port = p + 1;
      *q = '\0';
      break;

    case 'H':
      hide_discl = 0;	/* enable disclaimers hiding */
      break;

    case 'p':
      port = arg;
      break;

    case 'V':
      verb = 1;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

struct argp_child gwhois_argp_children[] = {
  { &ripe_argp,
    0,
    "RIPE-specific options",
    0
    },
  { NULL, 0, NULL, 0 }
};

static struct argp gwhois_argp = {
  gwhois_argp_options,
  gwhois_argp_parser,
  "OBJECT...",
  "client for the whois directory service",
  gwhois_argp_children,
  NULL,
  NULL
};

const char *program_authors[] =
  {
    "Marco d'Itri",
    NULL
  };

int
main (int argc, char *argv[])
{
  int index;
  char *fstring;
  char *qstring;
  char *p;

  set_program_name (argv[0]);

#ifdef ENABLE_NLS
  setlocale (LC_MESSAGES, "");
  bindtextdomain (NLS_CAT_NAME, LOCALEDIR);
  textdomain (NLS_CAT_NAME);
#endif

  obstack_init (&query_stk);
  iu_argp_init ("whois", program_authors);
  argp_parse (&gwhois_argp, argc, argv, ARGP_IN_ORDER, &index, NULL);
  obstack_1grow (&query_stk, 0);
  fstring = obstack_finish (&query_stk);
  argc -= index;
  argv += index;

  if (argc == 0 && !nopar)	/* there is no parameter */
    error (EXIT_FAILURE, 0, "not enough arguments");

  /* parse other parameters, if any */
  if (!nopar)
    {
      while (argc--)
	{
	  const char *arg = *argv++;
	  obstack_grow (&query_stk, arg, strlen (arg));
	  if (argc)
	    obstack_1grow (&query_stk, ' ');
	}
    }
  obstack_1grow (&query_stk, 0);
  qstring = obstack_finish (&query_stk);

  if (!server && domfind (qstring, gtlds))
    {
      if (verb)
	puts (_("Connecting to whois.internic.net."));
      sockfd = openconn ("whois.internic.net", NULL);
      server = query_crsnic (sockfd, qstring);
      closeconn (sockfd);
      if (!server)
	exit (EXIT_SUCCESS);
      printf (_("\nFound InterNIC referral to %s.\n\n"), server);
    }

  if (!server)
    {
      server = whichwhois (qstring);
      switch (server[0])
	{
	case 0:
	  if (!(server = getenv ("WHOIS_SERVER")))
	    server = DEFAULTSERVER;
	  if (verb)
	    printf (_("Using default server %s.\n"), server);
	  break;
	case 1:
	  puts (_("This TLD has no whois server, but you can access the "
		  "whois database at"));
	case 2:
	  puts (server + 1);
	  exit (EXIT_SUCCESS);
	case 3:
	  puts (_("This TLD has no whois server."));
	  exit (EXIT_SUCCESS);
	default:
	  if (verb)
	    printf (_("Using server %s.\n"), server);
	}
    }

  if (getenv ("WHOIS_HIDE"))
    hide_discl = 0;

  p = queryformat (server, fstring, qstring);
  if (verb)
    printf (_("Query string: \"%s\"\n\n"), p);
  strcat (p, "\r\n");

  signal (SIGTERM, sighandler);
  signal (SIGINT, sighandler);

  sockfd = openconn (server, port);
  do_query (sockfd, p);
  closeconn (sockfd);

  exit (EXIT_SUCCESS);
}

const char *
whichwhois (const char *s)
{
  unsigned long ip;
  unsigned int i;

  /* -v or -t has been used */
  if (*s == '\0')
    return "whois.ripe.net";

  /* IPv6 address */
  if (strchr (s, ':'))
    {
      if (strncasecmp (s, "2001:2", 6) == 0)	/* XXX ugly hack! */
	return "whois.apnic.net";
      if (strncasecmp (s, "2001:4", 6) == 0)
	return "whois.arin.net";
      if (strncasecmp (s, "2001:6", 6) == 0)
	return "whois.ripe.net";
      /* if (strncasecmp(s, "3ffe", 4) == 0) */
      return "whois.6bone.net";
    }

  /* email address */
  if (strchr (s, '@'))
    return "";

  /* no dot and no hyphen means it's a NSI NIC handle or ASN (?) */
  if (!strpbrk (s, ".-"))
    {
      const char *p;

      for (p = s; *p != '\0'; p++);	/* go to the end of s */
      if (strncasecmp (s, "as", 2) == 0 &&	/* it's an AS */
	  ((s[2] >= '0' && s[2] <= '9') || s[2] == ' '))
	return whereas (atoi (s + 2), as_assign);
      else if (strncasecmp (p - 2, "jp", 2) == 0)	/* JP NIC handle */
	return "whois.nic.ad.jp";
      if (*p == '!')		/* NSI NIC handle */
	return "whois.networksolutions.com";
      else			/* it's a NSI NIC handle or something we don't know about */
	return "";
    }

  /* smells like an IP? */
  if ((ip = myinet_aton (s)))
    {
      for (i = 0; ip_assign[i].serv; i++)
	if ((ip & ip_assign[i].mask) == ip_assign[i].net)
	  return ip_assign[i].serv;
      if (verb)
	puts (_("I don't know where this IP has been delegated.\n"
		"I'll try ARIN and hope for the best..."));
      return "whois.arin.net";
    }

  /* check TLD list */
  for (i = 0; tld_serv[i]; i += 2)
    if (domcmp (s, tld_serv[i]))
      return tld_serv[i + 1];

  /* no dot but hyphen */
  if (!strchr (s, '.'))
    {
      /* search for strings at the start of the word */
      for (i = 0; nic_handles[i]; i += 2)
	if (strncasecmp (s, nic_handles[i], strlen (nic_handles[i])) == 0)
	  return nic_handles[i + 1];
      if (verb)
	puts (_("I guess it's a netblock name but I don't know where to"
		" look it up."));
      return "whois.arin.net";
    }

  /* has dot and hypen and it's not in tld_serv[], WTF is it? */
  if (verb)
    puts (_("I guess it's a domain but I don't know where to look it"
	    " up."));
  return "";
}

const char *
whereas (int asn, struct as_del aslist[])
{
  int i;

  if (asn > 16383)
    puts (_("Unknown AS number. Please upgrade this program."));
  for (i = 0; aslist[i].serv; i++)
    if (asn >= aslist[i].first && asn <= aslist[i].last)
      return aslist[i].serv;
  return "whois.arin.net";
}

int
is_ripe_server (const char * const *srvtab, const char *name)
{
  struct in_addr addr;
  int isip = 0;

  isip = inet_aton (name, &addr);

  for (; *srvtab; ++srvtab)
    {
      const char *server = *srvtab;
      struct hostent *hp;

      if (strcmp (server, name) == 0)
	return 1;
      if (isip && (hp = gethostbyname (server)) != NULL)
	{
	  char **pa;
	  for (pa = hp->h_addr_list; *pa; pa++)
	    if (*(unsigned long*)*pa == addr.s_addr)
	      return 1;
	}
    }
  return 0;
}

char *
queryformat (const char *server, const char *flags, const char *query)
{
  char *buf;
  int isripe = 0;

  /* +10 for CORE; +2 for \r\n; +1 for NULL */
  buf = xmalloc (strlen (flags) + strlen (query) + 10 + 2 + 1);
  *buf = '\0';

  isripe = is_ripe_server (ripe_servers, server)
            || is_ripe_server (ripe_servers_old, server);
  if (isripe)
    strcat (buf, "-V" IDSTRING " ");

  if (*flags != '\0')
    {
      if (!isripe && strcmp (server, "whois.corenic.net") != 0)
	puts (_("Warning: RIPE flags ignored for a traditional server."));
      else
	strcat (buf, flags);
    }
  if (!isripe &&
      (strcmp (server, "whois.arin.net") == 0 ||
       strcmp (server, "whois.nic.mil") == 0) &&
      strncasecmp (query, "AS", 2) == 0 && query[2] >= '0' && query[2] <= '9')
    sprintf (buf, "AS %s", query + 2);	/* fix query for ARIN */
  else if (!isripe && strcmp (server, "whois.corenic.net") == 0)
    sprintf (buf, "--machine %s", query);	/* machine readable output */
  else if (!isripe && strcmp (server, "whois.ncst.ernet.in") == 0 &&
	   !strchr (query, ' '))
    sprintf (buf, "domain %s", query);	/* ask for a domain */
  else if (!isripe && strcmp (server, "whois.nic.ad.jp") == 0)
    {
      char *lang = getenv ("LANG");	/* not a perfect check, but... */
      if (!lang || (strncmp (lang, "ja", 2) != 0))
	sprintf (buf, "%s/e", query);	/* ask for english text */
      else
	strcat (buf, query);
    }
  else
    strcat (buf, query);
  return buf;
}

void
do_query (const int sock, const char *query)
{
  char buf[200], *p;
  FILE *fi;
  int i = 0, hide = hide_discl;

  fi = fdopen (sock, "r");
  if (write (sock, query, strlen (query)) < 0)
    err_sys ("write");

  while (fgets (buf, 200, fi))
    {				/* XXX errors? */
      if (hide == 1)
	{
	  if (strncmp (buf, hide_strings[i + 1], strlen (hide_strings[i + 1]))
	      == 0)
	    hide = 2;		/* stop hiding */
	  continue;		/* hide this line */
	}
      if (hide == 0)
	{
	  for (i = 0; hide_strings[i] != NULL; i += 2)
	    {
	      if (strncmp (buf, hide_strings[i], strlen (hide_strings[i])) ==
		  0)
		{
		  hide = 1;	/* start hiding */
		  break;
		}
	    }
	  if (hide == 1)
	    continue;		/* hide the first line */
	}
#ifdef EXT_6BONE
      /* % referto: whois -h whois.arin.net -p 43 as 1 */
      if (strncmp (buf, "% referto:", 10) == 0)
	{
	  char nh[256], np[16], nq[1024];

	  if (sscanf (buf, REFERTO_FORMAT, nh, np, nq) == 3)
	    {
	      int fd;

	      if (verb)
		printf (_("Detected referral to %s on %s.\n"), nq, nh);
	      strcat (nq, "\r\n");
	      fd = openconn (nh, np);
	      do_query (fd, nq);
	      closeconn (fd);
	      continue;
	    }
	}
#endif
      for (p = buf; *p && *p != '\r' && *p != '\n'; p++);
      *p = '\0';
      fprintf (stdout, "%s\n", buf);
    }
  if (ferror (fi))
    err_sys ("fgets");

  if (hide == 1)
    err_quit (_("Catastrophic error: disclaimer text has been changed.\n"
		"Please upgrade this program.\n"));
}

const char *
query_crsnic (const int sock, const char *query)
{
  char *temp, buf[100], *ret = NULL;
  FILE *fi;

  temp = xmalloc (strlen (query) + 1 + 2 + 1);
  *temp = '=';
  strcpy (temp + 1, query);
  strcat (temp, "\r\n");

  fi = fdopen (sock, "r");
  if (write (sock, temp, strlen (temp)) < 0)
    err_sys ("write");

  while (fgets (buf, 100, fi))
    {
      /* If there are multiple matches only the server of the first record
         is queried */
      if (strncmp (buf, "   Whois Server:", 16) == 0 && !ret)
	{
	  char *p, *q;

	  for (p = buf; *p != ':'; p++);	/* skip until colon */
	  for (p++; *p == ' '; p++);	/* skip colon and spaces */
	  ret = xmalloc (strlen (p) + 1);
	  for (q = ret; *p != '\n' && *p != '\r'; *q++ = *p++);	/*copy data */
	  *q = '\0';
	}
      fputs (buf, stdout);
    }
  if (ferror (fi))
    err_sys ("fgets");

  free (temp);
  return ret;
}

int
openconn (const char *server, const char *port)
{
  int fd;
  int i;
  struct addrinfo hints, *res, *ressave;

  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((i = getaddrinfo (server, port ? port : "whois", &hints, &res)) != 0)
    err_quit ("getaddrinfo: %s", gai_strerror (i));

  for (ressave = res; res; res = res->ai_next)
    {
      if ((fd =
	   socket (res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
	continue;		/* ignore */
      if (connect (fd, res->ai_addr, res->ai_addrlen) == 0)
	break;			/* success */
      close (fd);
    }

  if (!res)
    err_sys ("connect");
  freeaddrinfo (ressave);
  return (fd);
}

void
closeconn (const int fd)
{
  close (fd);
}

void
sighandler (int signum)
{
  closeconn (sockfd);
  err_quit (_("Interrupted by signal %d..."), signum);
}

/* check if dom ends with tld */
int
domcmp (const char *dom, const char *tld)
{
  const char *p, *q;

  for (p = dom; *p != '\0'; p++);
  p--;				/* move to the last char */
  for (q = tld; *q != '\0'; q++);
  q--;
  while (p >= dom && q >= tld && tolower (*p) == *q)
    {				/* compare backwards */
      if (q == tld)		/* start of the second word? */
	return 1;
      p--;
      q--;
    }
  return 0;
}

/* check if dom ends with an element of tldlist[] */
int
domfind (const char *dom, const char *tldlist[])
{
  int i;

  for (i = 0; tldlist[i]; i++)
    if (domcmp (dom, tldlist[i]))
      return 1;
  return 0;
}

unsigned long
myinet_aton (const char *s)
{
  int a, b, c, d;

  if (!s)
    return 0;
  if (sscanf (s, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
    return 0;
  return (a << 24) + (b << 16) + (c << 8) + d;
}


/* Error routines */
void
err_sys (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, ": %s\n", strerror (errno));
  va_end (ap);
  exit (EXIT_FAILURE);
}

void
err_quit (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fputs ("\n", stderr);
  va_end (ap);
  exit (EXIT_FAILURE);
}
