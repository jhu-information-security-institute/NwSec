/*
  Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
  2010, 2011, 2012, 2013, 2014, 2015 Free Software Foundation, Inc.

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

#include <intalkd.h>
#include <pwd.h>
#include <regex.h>
#include <ctype.h>
#include "argcv.h"

typedef struct netdef netdef_t;

struct netdef
{
  netdef_t *next;
  in_addr_t ipaddr;
  in_addr_t netmask;
};

typedef struct acl acl_t;

struct acl
{
  acl_t *next;
  regex_t re;
  netdef_t *netlist;
  int system;
  int action;
};


acl_t *acl_head, *acl_tail;

#define DOTTED_QUAD_LEN 16

static int
read_address (char **line_ptr, char *ptr)
{
  char *startp = *line_ptr;
  char *endp;
  int dotcount = 0;

  for (endp = startp; *endp; endp++, ptr++)
    if (!(isdigit (*endp) || *endp == '.'))
      break;
    else if (endp < startp + DOTTED_QUAD_LEN)
      {
	if (*endp == '.')
	  dotcount++;
	*ptr = *endp;
      }
    else
      break;
  *line_ptr = endp;
  *ptr = 0;
  return dotcount;
}

static netdef_t *
netdef_parse (char *str)
{
  in_addr_t ipaddr, netmask;
  netdef_t *netdef;
  char ipbuf[DOTTED_QUAD_LEN + 1];

  if (strcmp (str, "any") == 0)
    {
      ipaddr = 0;
      netmask = 0;
    }
  else
    {
      read_address (&str, ipbuf);
      ipaddr = inet_addr (ipbuf);
      if (ipaddr == INADDR_NONE)
	return NULL;
      if (*str == 0)
	netmask = 0xfffffffful;
      else if (*str != '/')
	return NULL;
      else
	{
	  str++;
	  if (read_address (&str, ipbuf) == 0)
	    {
	      /* netmask length */
	      unsigned int len = strtoul (ipbuf, NULL, 0);
	      if (len > 32)
		return NULL;
	      netmask = 0xfffffffful >> (32 - len);
	      netmask <<= (32 - len);
	      /*FIXME: hostorder? */
	    }
	  else
	    netmask = inet_network (ipbuf);
	  netmask = htonl (netmask);
	}
    }

  netdef = malloc (sizeof *netdef);
  if (!netdef)
    {
      syslog (LOG_ERR, "Out of memory");
      exit (EXIT_FAILURE);
    }

  netdef->next = NULL;
  netdef->ipaddr = ipaddr;
  netdef->netmask = netmask;

  return netdef;
}

void
read_acl (char *config_file, int system)
{
  FILE *fp;
  int line;
  char buf[128];
  char *ptr;

  if (!config_file || !config_file[0])
    return;

  fp = fopen (config_file, "r");
  if (!fp)
    {
      if (system > 0)
	{
	  /* A missing, yet specified, site-wide ACL is a serious error.
	   * Abort execution whenever this happens.
	   */
	  syslog (LOG_ERR, "Cannot open config file %s: %m", config_file);
	  exit (EXIT_FAILURE);
	}
      return;	/* User setting may fail to exist.  Just ignore.  */
    }

  if (system < 0)
    {
      /* Alarmed state, violating file access policy.
       * Insert a single, general denial equivalent to
       * a rule reading `deny .* any'.
       */
      const char any_user[] = "^.*$";
      char any_host[] = "any";
      acl_t *acl;
      regex_t re;
      netdef_t *cur;

      fclose (fp);
      acl = malloc (sizeof *acl);

      if (!acl)
	{
	  syslog (LOG_ERR, "Out of memory");
	  exit (EXIT_FAILURE);
	}
      if (regcomp (&re, any_user, 0) != 0)
	{
	  syslog (LOG_ERR, "Bad regexp '%s'.", any_user);
	  exit (EXIT_FAILURE);
	}
      cur = netdef_parse (any_host);
      if (!cur)
	{
	  syslog (LOG_ERR, "Cannot parse netdef '%s'.", any_host);
	  exit (EXIT_FAILURE);
	}

      acl->next = NULL;
      acl->action = ACL_DENY;
      acl->system = 0;
      acl->re = re;
      acl->netlist = cur;

      if (!acl_tail)
	acl_head = acl;
      else
	acl_tail->next = acl;
      acl_tail = acl;

      return;
    }

  line = 0;
  while ((ptr = fgets (buf, sizeof buf, fp)))
    {
      int len, i;
      int argc;
      char **argv;
      int action;
      regex_t re;
      netdef_t *head, *tail;
      acl_t *acl;

      line++;
      len = strlen (ptr);
      if (len > 0 && ptr[len - 1] == '\n')
	ptr[--len] = 0;

      while (*ptr && isspace (*ptr))
	ptr++;
      if (!*ptr || *ptr == '#')
	continue;

      argcv_get (ptr, "", &argc, &argv);
      if (argc < 2)
	{
	  syslog (LOG_ERR, "%s:%d: too few fields", config_file, line);
	  argcv_free (argc, argv);
	  continue;
	}

      if (strcmp (argv[0], "allow") == 0)
	action = ACL_ALLOW;
      else if (strcmp (argv[0], "deny") == 0)
	action = ACL_DENY;
      else
	{
	  syslog (LOG_WARNING, "%s:%d: unknown keyword", config_file, line);
	  argcv_free (argc, argv);
	  continue;
	}

      if (regcomp (&re, argv[1], 0) != 0)
	{
	  syslog (LOG_WARNING, "%s:%d: bad regexp", config_file, line);
	  argcv_free (argc, argv);
	  continue;
	}

      head = tail = NULL;
      for (i = 2; i < argc; i++)
	{
	  netdef_t *cur = netdef_parse (argv[i]);
	  if (!cur)
	    {
	      syslog (LOG_ERR, "%s:%d: can't parse netdef: %s",
		      config_file, line, argv[i]);
	      continue;
	    }
	  if (!tail)
	    head = cur;
	  else
	    tail->next = cur;
	  tail = cur;
	}

      argcv_free (argc, argv);

      acl = malloc (sizeof *acl);
      if (!acl)
	{
	  syslog (LOG_ERR, "Out of memory");
	  exit (EXIT_FAILURE);
	}
      acl->next = NULL;
      acl->action = action;
      acl->system = system;
      acl->netlist = head;
      acl->re = re;

      if (!acl_tail)
	acl_head = acl;
      else
	acl_tail->next = acl;
      acl_tail = acl;
    }

  fclose (fp);
}

static acl_t *
open_users_acl (char *name)
{
  int level = 0;	/* Private file, not mandatory.  */
  int rc;
  char *filename;
  struct passwd *pw;
  struct stat st;
  acl_t *mark;

  pw = getpwnam (name);
  if (!pw)
    return NULL;

  filename =
    malloc (strlen (pw->pw_dir) + sizeof (USER_ACL_NAME) +
	    2 /* Null and separator.  */ );
  if (!filename)
    {
      syslog (LOG_ERR, "Out of memory");
      return NULL;
    }

  sprintf (filename, "%s/%s", pw->pw_dir, USER_ACL_NAME);

  /* The location must be a file, and must be owned by the
   * indicated user and his corresponding group.  No write
   * access by group or world.  Record a syslog warning,
   * should either of these not be true.
   */
  rc = stat (filename, &st);
  if (rc < 0)
    return NULL;
  if (!S_ISREG(st.st_mode)
      || st.st_uid != pw->pw_uid
      || st.st_gid != pw->pw_gid
      || st.st_mode & S_IWGRP
      || st.st_mode & S_IWOTH)
    {
      if (logging || debug)
	syslog (LOG_WARNING, "Discarding '%s': insecure access.", filename);
      level = -1;	/* Enforce a deny rule.  */
    }

  mark = acl_tail;
  read_acl (filename, level);
  free (filename);

  return mark;
}

static void
netdef_free (netdef_t * netdef)
{
  netdef_t *next;

  while (netdef)
    {
      next = netdef->next;
      free (netdef);
      netdef = next;
    }
}

static void
acl_free (acl_t * acl)
{
  acl_t *next;

  while (acl)
    {
      next = acl->next;
      regfree (&acl->re);
      netdef_free (acl->netlist);
      free (acl);
      acl = next;
    }
}

static void
discard_acl (acl_t * mark)
{
  if (mark)
    {
      acl_free (mark->next);
      acl_tail = mark;
      acl_tail->next = NULL;
    }
  else
    acl_head = acl_tail = NULL;
}

int
acl_match (CTL_MSG * msg, struct sockaddr_in *sa_in)
{
  acl_t *acl, *mark;
  in_addr_t ip;
  int system_action = ACL_ALLOW, user_action = ACL_ALLOW;
  int found_user_acl = 0;

  if (strict_policy)
    system_action = ACL_DENY;

  mark = open_users_acl (msg->r_name);
  ip = sa_in->sin_addr.s_addr;
  if (mark && (mark != acl_tail))
    found_user_acl = 1;

  for (acl = acl_head; acl; acl = acl->next)
    {
      netdef_t *net;

      for (net = acl->netlist; net; net = net->next)
	{
	  /* Help the administrator and his users
	   * to simplify net list syntax:
	   *
	   *   mask the address `net->ipaddr' with
	   *   `net->netmask' for less computations
	   *   within the ACL specification.
	   */
	  if ((net->ipaddr & net->netmask) == (ip & net->netmask))
	    {
	      /*
	       * Site-wide ACLs concern user's name on this machine,
	       * whereas user's rules concern the incoming client name.
	       */
	      if (acl->system &&
		  regexec (&acl->re, msg->r_name, 0, NULL, 0) == 0)
		system_action = acl->action;
	      else if (regexec (&acl->re, msg->l_name, 0, NULL, 0) == 0)
		user_action = acl->action;
	    }
	}
    }
  discard_acl (mark);

  if (system_action == ACL_ALLOW)
    return user_action;

  if (strict_policy)
    return ACL_DENY;	/* Equal to `system_action'.  */

  /* At this point it is known that last activated site-wide
   * ACL rule has set SYSTEM_ACTION to ACL_DENY.  Do we
   * always want it to be overridable?
   */

  /* Override ACL_DENY only if there was a user specific file
   * ~/.talkrc containing some active rules at all.  In other
   * words, a site-policy claiming `deny' will need an act of
   * will by the user in order that it be overridden.
   */

  return found_user_acl ? user_action : ACL_DENY;
}
