/*
  Copyright (C) 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

/* Written by Giuseppe Scrivano.  */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include <sys/select.h>

#include <stdarg.h>
#include <progname.h>
#include <argp.h>
#include <error.h>
#include "libinetutils.h"
#include "minmax.h"

#define MAX3(a, b, c) (MAX (MAX (a, b), c))

const char doc[] = "remote execution client";
static char args_doc[] = "COMMAND";

const char *program_authors[] =
  {
    "Giuseppe Scrivano",
    NULL
  };

static struct argp_option options[] = {
#define GRP 10
  {"user",  'u', "user", 0, "Specify the user", GRP},
  {"host",  'h', "host", 0, "Specify the host", GRP},
  {"password",  'p', "password", 0, "Specify the password", GRP},
  {"port",  'P', "port", 0, "Specify the port to connect to", GRP},
  {"noerr", 'n', NULL, 0, "Disable the stderr stream", GRP},
  {"error",  'e', "error", 0, "Specify a TCP port to use for stderr", GRP},
  {"ipv4",  '4', NULL, 0, "Use IPv4 address space.", GRP},
  {"ipv6",  '6', NULL, 0, "Use IPv6 address space.", GRP},
  {"ipany",  'a', NULL, 0, "Allow any address family. (default)", GRP},
#undef GRP
  { NULL, 0, NULL, 0, NULL, 0 }
};

struct arguments
{
  const char *host;
  const char *user;
  const char *password;
  const char *command;
  int af;
  int port;
  int use_err;
  int err_port;
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'u':
      arguments->user = arg;
      break;
    case 'p':
      arguments->password = arg;
      break;
    case 'P':
      arguments->port = atoi (arg);
      break;
    case 'e':
      arguments->err_port = atoi (arg);
      break;
    case 'h':
      arguments->host = arg;
      break;
    case 'n':
      arguments->use_err = 0;
      break;
    case '4':
      arguments->af = AF_INET;
      break;
    case '6':
      arguments->af = AF_INET6;
      break;
    case 'a':
      arguments->af = AF_UNSPEC;
      break;
    case ARGP_KEY_ARG:
      arguments->command = arg;
      state->next = state->argc;
    }

  return 0;
}

static struct argp argp =
  {options, parse_opt, args_doc, doc, NULL, NULL, NULL};

static void do_rexec (struct arguments *arguments);

static int remote_err = EXIT_SUCCESS;

int
main (int argc, char **argv)
{
  struct arguments arguments;
  char password[64];
  int n, failed = 0;
  set_program_name (argv[0]);

  iu_argp_init ("rexec", program_authors);

  arguments.user = NULL;
  arguments.password = NULL;
  arguments.host = NULL;
  arguments.command = NULL;
  arguments.af = AF_UNSPEC;
  arguments.err_port = 0;
  arguments.use_err = 1;
  arguments.port = 512;

  argp_parse (&argp, argc, argv, ARGP_IN_ORDER, 0, &arguments);

  if (arguments.user == NULL)
    {
      error (0, 0, "user not specified");
      failed++;
    }

  if (arguments.password == NULL)
    {
      error (0, 0, "password not specified");
      failed++;
    }

  if (arguments.host == NULL)
    {
      error (0, 0, "host not specified");
      failed++;
    }

  if (arguments.command == NULL)
    {
      error (0, 0, "command not specified");
      failed++;
    }

  if (failed > 0)
    exit (EXIT_FAILURE);

  if (strcmp ("-", arguments.password) == 0)
    {
      password[0] = '\0';

      alarm (15);

      if (isatty (STDIN_FILENO))
	{
	  int changed = 0;
	  struct termios tt, newtt;

	  if (tcgetattr (STDIN_FILENO, &tt) >= 0)
	    {
	      memcpy (&newtt, &tt, sizeof (newtt));
	      newtt.c_lflag &= ~ECHO;
	      newtt.c_lflag |= ECHONL;
	      if (tcsetattr (STDIN_FILENO, TCSANOW, &newtt))
		error (0, errno, "failed to turn off echo");
	      changed = 1;
	    }

	  printf ("Password: ");
	  if (fgets (password, sizeof (password), stdin) == NULL)
	    password[0] = '\0';

	  if (changed && (tcsetattr (STDIN_FILENO, TCSANOW, &tt) < 0))
	    error (0, errno, "failed to restore terminal");
	}
      else if (fgets (password, sizeof (password), stdin) == NULL)
	password[0] = '\0';
      alarm (0);

      n = strlen (password);
      if ((n > 0) && (password[n - 1] == '\n'))
	{
	  password[n - 1] = '\0';
	  --n;
	}

      if (n == 0)
	error (EXIT_FAILURE, 0, "empty password");
      else
	arguments.password = password;
    }

  do_rexec (&arguments);

  exit (remote_err);
}

static void
safe_write (int socket, const char *str, size_t len)
{
  if (write (socket, str, len) < 0)
    error (EXIT_FAILURE, errno, "error sending data");
}

void
do_rexec (struct arguments *arguments)
{
  int err;
  char buffer[1024];
  int sock, ret;
  char port_str[12];
  socklen_t addrlen;
  struct sockaddr_storage addr;
  struct addrinfo hints, *ai, *res;
  int stdin_fd = STDIN_FILENO;
  int err_sock = -1;

  snprintf (port_str, sizeof (port_str), "%d", arguments->port);

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = arguments->af;
  hints.ai_socktype = SOCK_STREAM;

  ret = getaddrinfo (arguments->host, port_str, &hints, &res);
  if (ret)
    error (EXIT_FAILURE, errno, "getaddrinfo: %s", gai_strerror (ret));

  for (ai = res; ai != NULL; ai = ai->ai_next)
    {
      sock = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (sock < 0)
	continue;

      if (connect (sock, ai->ai_addr, ai->ai_addrlen) < 0)
	{
	  close (sock);
	  sock = -1;
	  continue;
	}

      break;	/* Acceptable.  */
    }

  if (ai == NULL)
    {
      freeaddrinfo (res);
      error (EXIT_FAILURE, errno, "cannot find host %s", arguments->host);
    }

  addrlen = ai->ai_addrlen;
  memcpy (&addr, ai->ai_addr, ai->ai_addrlen);
  freeaddrinfo (res);

  if (!arguments->use_err)
    {
      port_str[0] = '0';
      port_str[1] = '\0';
      safe_write (sock, port_str, 2);
      arguments->err_port = 0;
    }
  else
    {
      struct sockaddr_storage serv_addr;
      socklen_t len;
      int on = 1;
      int serv_sock = socket (addr.ss_family, SOCK_STREAM, 0);

      if (serv_sock < 0)
        error (EXIT_FAILURE, errno, "cannot open socket");

      setsockopt (serv_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));

      memset (&serv_addr, 0, sizeof (serv_addr));

      /* Need to bind to explicit port, when err_port is non-zero,
       * or to be assigned a free port, should err_port be naught.
       */
      switch (addr.ss_family)
	{
	case AF_INET:
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
	  ((struct sockaddr_in *) &serv_addr)->sin_len = addrlen;
#endif
	  ((struct sockaddr_in *) &serv_addr)->sin_family = addr.ss_family;
	  ((struct sockaddr_in *) &serv_addr)->sin_port = arguments->err_port;
	  break;
	case AF_INET6:
#ifdef HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN
	  ((struct sockaddr_in6 *) &serv_addr)->sin6_len = addrlen;
#endif
	  ((struct sockaddr_in6 *) &serv_addr)->sin6_family = addr.ss_family;
	  ((struct sockaddr_in6 *) &serv_addr)->sin6_port = arguments->err_port;
	  break;
	default:
	  error (EXIT_FAILURE, EAFNOSUPPORT, "unknown address family");
	}

      if (bind (serv_sock, (struct sockaddr *) &serv_addr, addrlen) < 0)
        error (EXIT_FAILURE, errno, "cannot bind socket");

      len = sizeof (serv_addr);
      if (getsockname (serv_sock, (struct sockaddr *) &serv_addr, &len))
        error (EXIT_FAILURE, errno, "error reading socket port");

      if (listen (serv_sock, 1))
        error (EXIT_FAILURE, errno, "error listening on socket");

      switch (serv_addr.ss_family)
	{
	case AF_INET:
	  arguments->err_port = ntohs (((struct sockaddr_in *) &serv_addr)->sin_port);
	  break;
	case AF_INET6:
	  arguments->err_port = ntohs (((struct sockaddr_in6 *) &serv_addr)->sin6_port);
	  break;
	default:
	  error (EXIT_FAILURE, EAFNOSUPPORT, "unknown address family");
	}
      snprintf (port_str, sizeof (port_str), "%u", arguments->err_port);
      safe_write (sock, port_str, strlen (port_str) + 1);

      /* Limit waiting time in case the server fails to call back:
       * if it aborts prematurely, or if a firewall is blocking
       * the intended STDERR connection to reach us.
       */
      alarm (5);

      err_sock = accept (serv_sock, (struct sockaddr *) &serv_addr, &len);
      if (err_sock < 0)
        error (EXIT_FAILURE, errno, "error accepting connection");

      alarm (0);

      shutdown (err_sock, SHUT_WR);

      close (serv_sock);
    }

  safe_write (sock, arguments->user, strlen (arguments->user) + 1);
  safe_write (sock, arguments->password, strlen (arguments->password) + 1);
  safe_write (sock, arguments->command, strlen (arguments->command) + 1);

  while (1)
    {
      int consumed = 0;		/* Signals remote return status.  */
      int ret, offset;
      fd_set rsocks;

      /* No other data to read.  */
      if (sock < 0 && err_sock < 0)
        break;

      FD_ZERO (&rsocks);
      if (0 <= sock)
        FD_SET (sock, &rsocks);
      if (0 <= stdin_fd)
	FD_SET (stdin_fd, &rsocks);
      if (0 <= err_sock)
        FD_SET (err_sock, &rsocks);

      ret = select (MAX3 (sock, stdin_fd, err_sock) + 1, &rsocks, NULL, NULL, NULL);
      if (ret == -1)
        error (EXIT_FAILURE, errno, "error select");

      if (0 <= stdin_fd && FD_ISSET (stdin_fd, &rsocks))
        {
	  err = read (stdin_fd, buffer, 1024);

          if (err < 0)
            error (EXIT_FAILURE, errno, "error reading stdin");

          if (!err)
            {
              shutdown (sock, SHUT_WR);
	      close (stdin_fd);
	      stdin_fd = -1;
              continue;
            }

          if (write (sock, buffer, err) < 0)
            error (EXIT_FAILURE, errno, "error writing");
        }

      if (0 <= sock && FD_ISSET (sock, &rsocks))
        {
          err = read (sock, buffer, 1024);

          if (err < 0)
            error (EXIT_FAILURE, errno, "error reading out stream");

          if (!err)
            {
              close (sock);
              sock = -1;
              continue;
            }

	  offset = 0;

	  if ((err > 0) && (consumed < 2))	/* Status can be two chars.  */
	    while ((err > offset) && (offset < 2)
		   && (buffer[offset] == 0 || buffer[offset] == 1))
	      remote_err = buffer[offset++];

          if (write (STDOUT_FILENO, buffer + offset, err - offset) < 0)
            error (EXIT_FAILURE, errno, "error writing");

	  consumed += err;
        }

     if (0 <= err_sock && FD_ISSET (err_sock, &rsocks))
        {
          err = read (err_sock, buffer, 1024);

          if (err < 0)
            error (EXIT_FAILURE, errno, "error reading err stream");

          if (!err)
            {
              close (err_sock);
              err_sock = -1;
              continue;
            }

	  offset = 0;

	  if ((err > 0) && (consumed < 2))	/* Status can be two chars.  */
	    while ((err > offset) && (offset < 2)
		   && (buffer[offset] == 0 || buffer[offset] == 1))
	      remote_err = buffer[offset++];

          if (write (STDERR_FILENO, buffer + offset, err - offset) < 0)
            error (EXIT_FAILURE, errno, "error writing to stderr");

	  consumed += err;
        }
    }
}
