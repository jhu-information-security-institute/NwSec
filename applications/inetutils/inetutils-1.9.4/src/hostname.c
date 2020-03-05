/*
  Copyright (C) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

#include <config.h>

#include <argp.h>
#include <error.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <progname.h>
#include "libinetutils.h"
#include "xalloc.h"
#include "xgethostname.h"
#include "xgetdomainname.h"


typedef struct
{
  const char *hostname_file;
  char *hostname_new;
  short int hostname_alias;
  short int hostname_fqdn;
  short int hostname_ip_address;
  short int hostname_dns_domain;
  short int hostname_short;
} hostname_arguments;

static char *(*get_name_action) (void) = NULL;
static int (*set_name_action) (const char *name, size_t size) = NULL;

const char args_doc[] = "[NAME]";
const char doc[] = "Show or set the system's host name.";
const char *program_authors[] = {
	"Debarshi Ray",
	NULL
};
static struct argp_option argp_options[] = {
#define GRP 0
  {"aliases", 'a', NULL, 0, "alias names", GRP+1},
  {"domain", 'd', NULL, 0, "DNS domain name", GRP+1},
  {"file", 'F', "FILE", 0, "set host name or NIS domain name from FILE",
   GRP+1},
  {"fqdn", 'f', NULL, 0, "DNS host name or FQDN", GRP+1},
  {"long", 'f', NULL, OPTION_ALIAS, "DNS host name or FQDN", GRP+1},
  {"ip-addresses", 'i', NULL, 0, "addresses for the host name", GRP+1},
  {"short", 's', NULL, 0, "short host name", GRP+1},
  {"yp", 'y', NULL, 0, "NIS/YP domain name", GRP+1},
  {"nis", 0, NULL, OPTION_ALIAS, NULL, GRP+1},
#undef GRP
  {NULL, 0, NULL, 0, NULL, 0}
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  hostname_arguments *const args = (hostname_arguments *const) state->input;

  switch (key)
    {
    case 'a':
      get_name_action = xgethostname;
      args->hostname_alias = 1;
      break;

    case 'd':
      get_name_action = xgethostname;
      args->hostname_fqdn = 1;
      args->hostname_dns_domain = 1;
      break;

    case 'F':
      set_name_action = sethostname;
      args->hostname_file = arg;
      break;

    case 'f':
      get_name_action = xgethostname;
      args->hostname_fqdn = 1;
      break;

    case 'i':
      get_name_action = xgethostname;
      args->hostname_ip_address = 1;
      break;

    case 's':
      get_name_action = xgethostname;
      args->hostname_fqdn = 1;
      args->hostname_short = 1;
      break;

    case 'y':
      get_name_action = xgetdomainname;
      break;

    case ARGP_KEY_ARG:
      set_name_action = sethostname;
      args->hostname_new = strdup (arg);
      if (args->hostname_new == NULL)
        error (EXIT_FAILURE, errno, "strdup");
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp =
  {argp_options, parse_opt, args_doc, doc, NULL, NULL, NULL};

static void get_name (const hostname_arguments *const args);
static void set_name (const hostname_arguments *const args);
static char * get_aliases (const char *const host_name);
static char * get_fqdn (const char *const host_name);
static char * get_ip_addresses (const char *const host_name);
static char * get_dns_domain_name (const char *const host_name);
static char * get_short_hostname (const char *const host_name);
static char * parse_file (const char *const file_name);

int
main (int argc, char *argv[])
{
  hostname_arguments args;

  set_program_name (argv[0]);

  memset ((void *) &args, 0, sizeof (args));

  /* Parse command line */
  iu_argp_init ("hostname", program_authors);
  argp_parse (&argp, argc, argv, 0, NULL, (void *) &args);

  /* Set default action */
  if (get_name_action == NULL && set_name_action ==  NULL)
    get_name_action = xgethostname;

  if (get_name_action == xgetdomainname || get_name_action == xgethostname)
    get_name (&args);
  else if (set_name_action == sethostname)
    set_name (&args);

  return 0;
}

static void
get_name (const hostname_arguments *const args)
{
  char *sname, *name;

  sname = (*get_name_action) ();

  if (!sname)
    error (EXIT_FAILURE, errno, "cannot determine name");

  if (args->hostname_alias == 1)
    name = get_aliases (sname);
  else if (args->hostname_fqdn == 1)
    {
      name = get_fqdn (sname);

      if (args->hostname_dns_domain == 1 || args->hostname_short == 1)
	{
	  /* Eliminate empty replies, as well as `(none)'.  */
	  if (name && *name && *name != '(')
	    {
	      free (sname);
	      sname = name;
	      name = NULL;
	    }
	  else if (name && *name == '(')
	    {
	      free (name);
	      name = NULL;
	    }
	}

      if (args->hostname_dns_domain == 1)
        name = get_dns_domain_name (sname);
      else if (args->hostname_short == 1)
        name = get_short_hostname (sname);
    }
  else if (args->hostname_ip_address == 1)
      name = get_ip_addresses (sname);
  else
    {
      name = strdup (sname);
      if (name == NULL)
        error (EXIT_FAILURE, errno, "strdup");
    }

  if (name && *name)
    puts (name);

  free (name);
  free (sname);
  return;
}

static void
set_name (const hostname_arguments *const args)
{
  char *hostname_new;
  int status;
  size_t size;

  if (args->hostname_file != NULL)
    hostname_new = parse_file (args->hostname_file);
  else
    hostname_new = args->hostname_new;

  size = strlen (hostname_new);
  if (!size)
    error (EXIT_FAILURE, 0, "Empty hostname");

  status = (*set_name_action) (hostname_new, size);
  if (status == -1)
    error (EXIT_FAILURE, errno, "sethostname");

  free (hostname_new);
  return;
}

static char *
get_aliases (const char *const host_name)
{
  char *aliases;
  unsigned int count = 0;
  unsigned int i;
  unsigned int size = 256;
  struct hostent *ht;

  aliases = xmalloc (sizeof (char) * size);
  aliases[0] = '\0';

  ht = gethostbyname (host_name);
  if (ht == NULL)
    strcpy (aliases, "");	/* Be honest about missing aliases.  */
  else
    {
      for (i = 0; ht->h_aliases[i] != NULL; i++)
        {
          /* Aliases should be blankspace separated. */
          if (ht->h_aliases[i+1] != NULL)
            count++;
          count += strlen (ht->h_aliases[i]);
          if (count >= size)
            {
              size *= 2;
              aliases = xrealloc (aliases, size);
            }

          strcat (aliases, ht->h_aliases[i]);
          strcat (aliases, " ");
        }
    }

  return aliases;
}

static char *
get_fqdn (const char *const host_name)
{
  char *fqdn;
  struct hostent *ht;

  ht = gethostbyname (host_name);
  if (ht == NULL)
    fqdn = strdup (host_name);	/* Fall back to system name.  */
  else
    fqdn = strdup (ht->h_name);

  if (fqdn == NULL)
    error (EXIT_FAILURE, errno, "strdup");

  return fqdn;
}

static char *
get_ip_addresses (const char *const host_name)
{
  char address[16];
  char *addresses;
  unsigned int count = 0;
  unsigned int i;
  unsigned int size = 256;
  struct hostent *ht;

  addresses = xmalloc (sizeof (char) * size);
  addresses[0] = '\0';

  ht = gethostbyname (host_name);
  if (ht == NULL)
#if HAVE_HSTRERROR
    error (EXIT_FAILURE, 0, "gethostbyname: %s", hstrerror (h_errno));
#else
    strcpy (addresses, "(none)");
#endif
  else
    {
      for (i = 0; ht->h_addr_list[i] != NULL; i++)
        {
          inet_ntop (ht->h_addrtype, (void *) ht->h_addr_list[i],
                     address, sizeof (address));

          /* IP addresses should be blankspace separated. */
          if (ht->h_addr_list[i+1] != NULL)
            count++;
          count += strlen (address);
          if (count >= size)
            {
              size *= 2;
              addresses = xrealloc (addresses, size);
            }

          strcat (addresses, address);
          strcat (addresses, " ");
        }
    }

  return addresses;
}

static char *
get_dns_domain_name (const char *const host_name)
{
  char *domain_name;
  const char * pos;

  pos = strchr (host_name, '.');
  if (pos == NULL)
    domain_name = strdup ("(none)");
  else
    domain_name = strdup (pos+1);

  if (domain_name == NULL)
    error (EXIT_FAILURE, errno, "strdup");

  return domain_name;
}

static char *
get_short_hostname (const char *const host_name)
{
  size_t size;
  char *short_hostname;
  const char * pos;

  pos = strchr (host_name, '.');
  if (pos == NULL)
    short_hostname = strdup (host_name);
  else
    {
      size = pos - host_name;
      short_hostname = strndup (host_name, size);
    }

  if (short_hostname == NULL)
    error (EXIT_FAILURE, errno, "strdup");

  return short_hostname;
}

static char *
parse_file (const char *const file_name)
{
  char *buffer = NULL;
  char *name = NULL;
  FILE *file;
  ssize_t nread;
  size_t size = 0;

  file = fopen (file_name, "r");
  if (file == NULL)
    error (EXIT_FAILURE, errno, "fopen");

  errno = 0;			/* Portability issue!  */

  do
    {
      nread = getline (&buffer, &size, file);
      if (nread == -1)
	error (EXIT_FAILURE, errno, "getline%s", errno ? "" : ": No text");

      if (buffer[0] != '#')
        {
	  name = (char *) xmalloc (sizeof (char) * nread);
	  if (sscanf (buffer, "%s", name)  == 1)
	    break;
        }
    }
  while (feof (file) == 0);

  free (buffer);
  fclose (file);
  return name;
}
