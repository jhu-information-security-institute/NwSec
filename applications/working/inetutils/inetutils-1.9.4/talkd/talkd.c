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

#include <config.h>
#include <argp.h>
#include <intalkd.h>
#include <signal.h>
#include <libinetutils.h>
#include <progname.h>
#include "unused-parameter.h"

#ifndef LOG_FACILITY
# define LOG_FACILITY LOG_DAEMON
#endif

void talkd_init (void);
void talkd_run (int fd);

/* Configurable parameters: */
int debug;
int logging;
int strict_policy;
unsigned int timeout = 30;
time_t max_idle_time = 120;
time_t max_request_ttl = MAX_LIFE;

char *acl_file;
char *hostname;

const char args_doc[] = "";
const char doc[] = "Talk daemon, using service `ntalk'.";
const char *program_authors[] = {
	"Sergey Poznyakoff",
	NULL
};
static struct argp_option argp_options[] = {
#define GRP 0
  {"acl", 'a', "FILE", 0, "read site-wide ACLs from FILE", GRP+1},
  {"debug", 'd', NULL, 0, "enable debugging", GRP+1},
  {"idle-timeout", 'i', "SECONDS", 0, "set idle timeout value to SECONDS",
   GRP+1},
  {"logging", 'l', NULL, 0, "enable more syslog reporting", GRP+1},
  {"request-ttl", 'r', "SECONDS", 0, "set request time-to-live value to "
   "SECONDS", GRP+1},
  {"strict-policy", 'S', NULL, 0, "apply strict ACL policy", GRP+1},
  {"timeout", 't', "SECONDS", 0, "set timeout value to SECONDS", GRP+1},
#undef GRP
  {NULL, 0, NULL, 0, NULL, 0}
};

static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state _GL_UNUSED_PARAMETER)
{
  switch (key)
    {
    case 'a':
      acl_file = arg;
      break;

    case 'd':
      debug++;
      break;

    case 'i':
      max_idle_time = strtoul (arg, NULL, 0);
      break;

    case 'l':
      logging++;
      break;

    case 'r':
      max_request_ttl = strtoul (arg, NULL, 0);
      break;

    case 'S':
      strict_policy++;
      break;

    case 't':
      timeout = strtoul (arg, NULL, 0);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp =
  {argp_options, parse_opt, args_doc, doc, NULL, NULL, NULL};

int
main (int argc, char *argv[])
{
  set_program_name (argv[0]);
  /* Parse command line */
  iu_argp_init ("talkd", program_authors);
  argp_parse (&argp, argc, argv, 0, NULL, NULL);

  openlog ("talkd", LOG_PID, LOG_FACILITY);
  read_acl (acl_file, 1);	/* System wide ACL.  Can abort.  */
  talkd_init ();
  talkd_run (STDIN_FILENO);
  return 0;
}

void
talkd_init (void)
{
  hostname = localhost ();
  if (!hostname)
    {
      syslog (LOG_ERR, "Cannot determine my hostname: %m");
      exit (EXIT_FAILURE);
    }
}

time_t last_msg_time;

static void
alarm_handler (int err _GL_UNUSED_PARAMETER)
{
  int oerrno = errno;

  if ((time (NULL) - last_msg_time) >= max_idle_time)
    exit (EXIT_SUCCESS);
  alarm (timeout);
  errno = oerrno;
}

void
talkd_run (int fd)
{
  struct sockaddr ctl_addr;

  signal (SIGALRM, alarm_handler);
  alarm (timeout);
  while (1)
    {
      int rc;
      struct sockaddr_in sa_in;
      CTL_MSG msg;
      CTL_RESPONSE resp;
      socklen_t len;

      len = sizeof sa_in;
      rc =
	recvfrom (fd, &msg, sizeof msg, 0, (struct sockaddr *) &sa_in, &len);
      if (rc != sizeof msg)
	{
	  if (rc < 0 && errno != EINTR && (logging || debug))
	    syslog (LOG_NOTICE, "recvfrom: %m");
	  continue;
	}
      last_msg_time = time (NULL);
      if (process_request (&msg, &sa_in, &resp) == 0)
	{
	  ctl_addr.sa_family = msg.ctl_addr.sa_family;
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	  ctl_addr.sa_len = sizeof (struct sockaddr_in);
#endif
	  memcpy (&ctl_addr.sa_data, &msg.ctl_addr.sa_data,
		  sizeof (ctl_addr.sa_data));
	  rc = sendto (fd, &resp, sizeof resp, 0,
		       &ctl_addr, sizeof (ctl_addr));
	  if (rc != sizeof resp && (logging || debug))
	    syslog (LOG_NOTICE, "sendto: %m");
	}
    }
}
