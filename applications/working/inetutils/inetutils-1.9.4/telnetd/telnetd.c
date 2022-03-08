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

#include <config.h>

#include "telnetd.h"

#include <sys/utsname.h>
#include <argp.h>
#include <progname.h>
#include <error.h>
#include <unused-parameter.h>
#include <libinetutils.h>

#if defined AUTHENTICATION || defined ENCRYPTION
# include <libtelnet/misc.h>
#endif

#ifdef  AUTHENTICATION
static void parse_authmode (char *str);
#endif

static void parse_linemode (char *str);
static void parse_debug_level (char *str);
static void telnetd_setup (int fd);
static int telnetd_run (void);
static void print_hostinfo (void);
static void chld_is_done (int sig);

/* Template command line for invoking login program.  */

char *login_invocation =
#ifdef SOLARIS10
  /* TODO: `-s telnet' or `-s ktelnet'.
   *       `-u' takes the Kerberos principal name
   *       of the authenticating, remote user.
   */
  PATH_LOGIN " -p -h %h %?T{-t %T} -d %L %?u{-u %u}{%U}"

#elif defined SOLARIS
  /* At least for SunOS 5.8.  */
  PATH_LOGIN " -h %h %?T{%T} %?u{-- %u}{%U}"

#else /* !SOLARIS */
  PATH_LOGIN " -p -h %h %?u{-f %u}{%U}"
#endif
  ;

int keepalive = 1;		/* Should the TCP keepalive bit be set */
int reverse_lookup = 0;		/* Reject connects from hosts which IP numbers
				   cannot be reverse mapped to their hostnames */
int alwayslinemode;		/* Always set the linemode (1) */
int lmodetype;			/* Type of linemode (2) */
int hostinfo = 1;		/* Print the host-specific information before
				   login */

int debug_level[debug_max_mode];	/* Debugging levels */
int debug_tcp = 0;		/* Should the SO_DEBUG be set? */

int pending_sigchld = 0;	/* Needed to drain pty input.  */

int net;			/* Network connection socket */
int pty;			/* PTY master descriptor */
#if defined AUTHENTICATION || defined ENCRYPTION
char *principal = NULL;
#endif
char *remote_hostname;
char *local_hostname;
char *user_name;
char line[256];

char options[256];
char do_dont_resp[256];
char will_wont_resp[256];
int linemode;			/* linemode on/off */
int uselinemode;		/* what linemode to use (on/off) */
int editmode;			/* edit modes in use */
int useeditmode;		/* edit modes to use */
int alwayslinemode;		/* command line option */
int lmodetype;			/* Client support for linemode */
int flowmode;			/* current flow control state */
int restartany;			/* restart output on any character state */
int diagnostic;			/* telnet diagnostic capabilities */
#if defined AUTHENTICATION
int auth_level = 0;		/* Authentication level */
int autologin;
#endif

slcfun slctab[NSLC + 1];	/* slc mapping table */

char *terminaltype;

int SYNCHing;			/* we are in TELNET SYNCH mode */
struct telnetd_clocks clocks;


static struct argp_option argp_options[] = {
#define GRID 10
  { "debug", 'D', "LEVEL", OPTION_ARG_OPTIONAL,
    "set debugging level", GRID },
  { "exec-login", 'E', "STRING", 0,
    "set program to be executed instead of " PATH_LOGIN, GRID },
  { "no-hostinfo", 'h', NULL, 0,
    "do not print host information before login has been completed", GRID },
  { "linemode", 'l', "MODE", OPTION_ARG_OPTIONAL,
    "set line mode", GRID },
  { "no-keepalive", 'n', NULL, 0,
    "disable TCP keep-alives", GRID },
  { "reverse-lookup", 'U', NULL, 0,
    "refuse connections from addresses that "
    "cannot be mapped back into a symbolic name", GRID },
#undef GRID

#ifdef AUTHENTICATION
# define GRID 20
  { NULL, 0, NULL, 0, "Authentication control:", GRID },
  { "authmode", 'a', "MODE", 0,
    "specify what mode to use for authentication", GRID },
  { "server-principal", 'S', "NAME", 0,
    "set Kerberos principal name for this server instance, "
    "with or without explicit realm", GRID },
  { "disable-auth-type", 'X', "TYPE", 0,
    "disable the use of given authentication option", GRID },
# undef GRID
#endif /* AUTHENTICATION */
  { NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state _GL_UNUSED_PARAMETER)
{
  switch (key)
    {
#ifdef  AUTHENTICATION
    case 'a':
      parse_authmode (arg);
      break;
#endif

    case 'D':
      parse_debug_level (arg);
      break;

    case 'E':
      login_invocation = arg;
      break;

    case 'h':
      hostinfo = 0;
      break;

    case 'l':
      parse_linemode (arg);
      break;

    case 'n':
      keepalive = 0;
      break;

#if defined AUTHENTICATION || defined ENCRYPTION
    case 'S':
      principal = arg;
      break;
#endif

    case 'U':
      reverse_lookup = 1;
      break;

#ifdef	AUTHENTICATION
    case 'X':
      auth_disable_name (arg);
      break;
#endif

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp =
  {
    argp_options,
    parse_opt,
    NULL,
    "DARPA telnet protocol server",
    NULL, NULL, NULL
  };



int
main (int argc, char **argv)
{
  int index;

  set_program_name (argv[0]);
  iu_argp_init ("telnetd", default_program_authors);

  openlog ("telnetd", LOG_PID | LOG_ODELAY, LOG_DAEMON);

  argp_parse (&argp, argc, argv, 0, &index, NULL);

  if (argc != index)
    error (EXIT_FAILURE, 0, "junk arguments in the command line");

  telnetd_setup (0);
  return telnetd_run ();	/* Never returning.  */
}

void
parse_linemode (char *str)
{
  if (!str)
    alwayslinemode = 1;
  else if (strcmp (str, "nokludge") == 0)
    lmodetype = NO_AUTOKLUDGE;
  else
    syslog (LOG_NOTICE, "invalid argument to --linemode: %s", str);
}

#ifdef  AUTHENTICATION
void
parse_authmode (char *str)
{
  if (strcasecmp (str, "none") == 0)
    auth_level = 0;
  else if (strcasecmp (str, "other") == 0)
    auth_level = AUTH_OTHER;
  else if (strcasecmp (str, "user") == 0)
    auth_level = AUTH_USER;
  else if (strcasecmp (str, "valid") == 0)
    auth_level = AUTH_VALID;
  else if (strcasecmp (str, "off") == 0)
    auth_level = -1;
  else
    syslog (LOG_NOTICE, "unknown authorization level for -a: %s", str);
}
#endif /* AUTHENTICATION */

static struct
{
  char *name;
  int modnum;
} debug_mode[debug_max_mode] =
{
  {"options", debug_options},
  {"report", debug_report},
  {"netdata", debug_net_data},
  {"ptydata", debug_pty_data},
  {"auth", debug_auth},
  {"encr", debug_encr},
};

void
parse_debug_level (char *str)
{
  int i;
  char *tok;

  if (!str)
    {
      for (i = 0; i < debug_max_mode; i++)
	debug_level[debug_mode[i].modnum] = MAX_DEBUG_LEVEL;
      return;
    }

  for (tok = strtok (str, ","); tok; tok = strtok (NULL, ","))
    {
      int length, level;
      char *p;

      if (strcmp (tok, "tcp") == 0)
	{
	  debug_tcp = 1;
	  continue;
	}

      p = strchr (tok, '=');
      if (p)
	{
	  length = p - tok;
	  level = strtoul (p + 1, NULL, 0);
	}
      else
	{
	  length = strlen (tok);
	  level = MAX_DEBUG_LEVEL;
	}

      for (i = 0; i < debug_max_mode; i++)
	if (strncmp (debug_mode[i].name, tok, length) == 0)
	  {
	    debug_level[debug_mode[i].modnum] = level;
	    break;
	  }

      if (i == debug_max_mode)
	syslog (LOG_NOTICE, "unknown debug mode: %s", tok);
    }
}


typedef unsigned int ip_addr_t;
 /*FIXME*/ void
telnetd_setup (int fd)
{
#ifdef IPV6
  struct sockaddr_storage saddr;
  char buf[256], buf2[256];	/* FIXME: We should use dynamic allocation. */
  int err;
#else
  struct sockaddr_in saddr;
  struct hostent *hp;
#endif
  int true = 1;
  socklen_t len;
  char uname[256];
   /*FIXME*/ int level;

  len = sizeof (saddr);
  if (getpeername (fd, (struct sockaddr *) &saddr, &len) < 0)
    {
      syslog (LOG_ERR, "getpeername: %m");
      exit (EXIT_FAILURE);
    }

#ifdef IPV6
  err = getnameinfo ((struct sockaddr *) &saddr, len, buf,
		     sizeof (buf), NULL, 0, NI_NUMERICHOST);
  if (err)
    {
      const char *errmsg;

      if (err == EAI_SYSTEM)
	errmsg = strerror (errno);
      else
	errmsg = gai_strerror (err);

      syslog (LOG_AUTH | LOG_NOTICE, "Cannot get address: %s", errmsg);
      fatal (fd, "Cannot get address.");
    }

  /* We use a second buffer so we don't have to call getnameinfo again
     if we need the numeric host below.  */
  err = getnameinfo ((struct sockaddr *) &saddr, len, buf2,
		     sizeof (buf2), NULL, 0, NI_NAMEREQD);

  if (reverse_lookup)
    {
      struct addrinfo *result, *aip;

      if (err)
	{
	  const char *errmsg;

	  if (err == EAI_SYSTEM)
	    errmsg = strerror (errno);
	  else
	    errmsg = gai_strerror (err);

	  syslog (LOG_AUTH | LOG_NOTICE, "Can't resolve %s: %s", buf, errmsg);
	  fatal (fd, "Cannot resolve address.");
	}

      remote_hostname = xstrdup (buf2);

      err = getaddrinfo (remote_hostname, NULL, NULL, &result);
      if (err)
	{
	  const char *errmsg;

	  if (err == EAI_SYSTEM)
	    errmsg = strerror (errno);
	  else
	    errmsg = gai_strerror (err);

	  syslog (LOG_AUTH | LOG_NOTICE, "Forward resolve of %s failed: %s",
		  remote_hostname, errmsg);
	  fatal (fd, "Cannot resolve address.");
	}

      for (aip = result; aip; aip = aip->ai_next)
	{
	  if (aip->ai_family != saddr.ss_family)
	    continue;

	  /* Must compare the address part only.
	   * The ports are almost surely different!
	   */
	  if (aip->ai_family == AF_INET
	      && !memcmp (&((struct sockaddr_in *) aip->ai_addr)->sin_addr,
			  &((struct sockaddr_in *) &saddr)->sin_addr,
			  sizeof (struct in_addr)))
	    break;
	  if (aip->ai_family == AF_INET6
	      && !memcmp (&((struct sockaddr_in6 *) aip->ai_addr)->sin6_addr,
			  &((struct sockaddr_in6 *) &saddr)->sin6_addr,
			  sizeof (struct in6_addr)))
	    break;
	}

      if (aip == NULL)
	{
	  syslog (LOG_AUTH | LOG_NOTICE,
		  "No address of %s matched %s", remote_hostname, buf);
	  fatal (fd, "Cannot resolve address.");
	}

      freeaddrinfo (result);
    }
  else
    {
      if (!err)
	remote_hostname = xstrdup (buf2);
      else
	remote_hostname = xstrdup (buf);
    }
#else
  hp = gethostbyaddr ((char *) &saddr.sin_addr.s_addr,
		      sizeof (saddr.sin_addr.s_addr), AF_INET);
  if (reverse_lookup)
    {
      char **ap;

      if (!hp)
	{
	  syslog (LOG_AUTH | LOG_NOTICE,
		  "Can't resolve %s: %s",
		  inet_ntoa (saddr.sin_addr), hstrerror (h_errno));
	  fatal (fd, "Cannot resolve address.");
	}

      remote_hostname = xstrdup (hp->h_name);

      hp = gethostbyname (remote_hostname);
      if (!hp)
	{
	  syslog (LOG_AUTH | LOG_NOTICE,
		  "Forward resolve of %s failed: %s",
		  remote_hostname, hstrerror (h_errno));
	  fatal (fd, "Cannot resolve address.");
	}

      for (ap = hp->h_addr_list; *ap; ap++)
	if (*(ip_addr_t *) ap == saddr.sin_addr.s_addr)
	  break;

      if (ap == NULL)
	{
	  syslog (LOG_AUTH | LOG_NOTICE,
		  "No address of %s matched %s",
		  remote_hostname, inet_ntoa (saddr.sin_addr));
	  fatal (fd, "Cannot resolve address.");
	}
    }
  else
    {
      if (hp)
	remote_hostname = xstrdup (hp->h_name);
      else
	remote_hostname = xstrdup (inet_ntoa (saddr.sin_addr));
    }
#endif

  /* Set socket options */

  if (keepalive
      && setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE,
		     (char *) &true, sizeof (true)) < 0)
    syslog (LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");

  if (debug_tcp
      && setsockopt (fd, SOL_SOCKET, SO_DEBUG,
		     (char *) &true, sizeof (true)) < 0)
    syslog (LOG_WARNING, "setsockopt (SO_DEBUG): %m");

  net = fd;

  local_hostname = localhost ();
#if defined AUTHENTICATION || defined ENCRYPTION
  auth_encrypt_init (local_hostname, remote_hostname, principal,
		     "TELNETD", 1);
#endif

  io_setup ();

  /* get terminal type. */
  uname[0] = 0;
  level = getterminaltype (uname, sizeof (uname));
  setenv ("TERM", terminaltype ? terminaltype : "network", 1);
  if (uname[0])
    user_name = xstrdup (uname);
  pty = startslave (remote_hostname, level, user_name);

#ifndef HAVE_STREAMSPTY
  /* Turn on packet mode */
  ioctl (pty, TIOCPKT, (char *) &true);
#endif
  ioctl (pty, FIONBIO, (char *) &true);
  ioctl (net, FIONBIO, (char *) &true);

#if defined SO_OOBINLINE
  setsockopt (net, SOL_SOCKET, SO_OOBINLINE, (char *) &true, sizeof true);
#endif

#ifdef SIGTSTP
  signal (SIGTSTP, SIG_IGN);
#endif
#ifdef	SIGTTOU
  signal (SIGTTOU, SIG_IGN);
#endif

  /* Activate SA_RESTART whenever available.  */
  setsig (SIGCHLD, chld_is_done);
}

int
telnetd_run (void)
{
  int nfd;

  get_slc_defaults ();

  if (my_state_is_wont (TELOPT_SGA))
    send_will (TELOPT_SGA, 1);

  /* Old BSD 4.2 clients are unable to deal with TCP out-of-band data.
     To find out, we send out a "DO ECHO". If the remote side is
     a BSD 4.2 it will answer "WILL ECHO". See the response processing
     below. */
  send_do (TELOPT_ECHO, 1);
  if (his_state_is_wont (TELOPT_LINEMODE))
    {
      /* Query the peer for linemode support by trying to negotiate
         the linemode option. */
      linemode = 0;
      editmode = 0;
      send_do (TELOPT_LINEMODE, 1);	/* send do linemode */
    }

  send_do (TELOPT_NAWS, 1);
  send_will (TELOPT_STATUS, 1);
  flowmode = 1;			/* default flow control state */
  restartany = -1;		/* uninitialized... */
  send_do (TELOPT_LFLOW, 1);

  /* Wait for a response from the DO ECHO. Reportedly, some broken
     clients might not respond to it. To work around this, we wait
     for a response to NAWS, which should have been processed after
     DO ECHO (most dumb telnets respond with WONT for a DO that
     they don't understand).
     On the other hand, the client might have sent WILL NAWS as
     part of its startup code, in this case it surely should have
     answered our DO ECHO, so the second loop is waiting for
     the ECHO to settle down.  */
  ttloop (his_will_wont_is_changing (TELOPT_NAWS));

  if (his_want_state_is_will (TELOPT_ECHO) && his_state_is_will (TELOPT_NAWS))
    ttloop (his_will_wont_is_changing (TELOPT_ECHO));

  /* If the remote client is badly broken and did not respond to our
     DO ECHO, we simulate the receipt of a will echo. This will also
     send a WONT ECHO to the client, since we assume that the client
     failed to respond because it believes that it is already in DO ECHO
     mode, which we do not want. */

  if (his_want_state_is_will (TELOPT_ECHO))
    {
      DEBUG (debug_options, 1, debug_output_data ("td: simulating recv\r\n"));
      willoption (TELOPT_ECHO);
    }

  /* Turn on our echo */
  if (my_state_is_wont (TELOPT_ECHO))
    send_will (TELOPT_ECHO, 1);

  /* Continuing line mode support.  If client does not support
     real linemode, attempt to negotiate kludge linemode by sending
     the do timing mark sequence. */
  if (lmodetype < REAL_LINEMODE)
    send_do (TELOPT_TM, 1);

  /* Pick up anything received during the negotiations */
  telrcv ();

  if (hostinfo)
    print_hostinfo ();

  init_termbuf ();
  localstat ();

  DEBUG (debug_report, 1,
	 debug_output_data ("td: Entering processing loop\r\n"));

  nfd = ((net > pty) ? net : pty) + 1;

  for (;;)
    {
      fd_set ibits, obits, xbits;
      register int c;

      if (net_input_level () < 0 && pty_input_level () < 0)
	break;

      FD_ZERO (&ibits);
      FD_ZERO (&obits);
      FD_ZERO (&xbits);

      /* Never look for input if there's still stuff in the corresponding
         output buffer */
      if (net_output_level () || pty_input_level () > 0)
	FD_SET (net, &obits);
      else
	FD_SET (pty, &ibits);

      if (pty_output_level () || net_input_level () > 0)
	FD_SET (pty, &obits);
      else
	FD_SET (net, &ibits);

      if (!SYNCHing)
	FD_SET (net, &xbits);

      if ((c = select (nfd, &ibits, &obits, &xbits, NULL)) <= 0)
	{
	  if (c == -1 && errno == EINTR)
	    continue;
	  sleep (5);
	  continue;
	}

      if (FD_ISSET (net, &xbits))
	SYNCHing = 1;

      if (FD_ISSET (net, &ibits))
	{
	  /* Something to read from the network... */
	  /*FIXME: handle  !defined(SO_OOBINLINE) */
	  net_read ();
	}

      if (FD_ISSET (pty, &ibits))
	{
	  /* Something to read from the pty... */
	  if (pty_read () <= 0)
	    break;

	  /* The first byte is now TIOCPKT data.  Peek at it.  */
	  c = pty_get_char (1);

#if defined TIOCPKT_IOCTL
	  if (c & TIOCPKT_IOCTL)
	    {
	      pty_get_char (0);
	      copy_termbuf ();	/* Pty buffer is now emptied.  */
	      localstat ();
	    }
#endif
	  if (c & TIOCPKT_FLUSHWRITE)
	    {
	      static char flushdata[] = { IAC, DM };
	      pty_get_char (0);
	      netclear ();	/* clear buffer back */
	      net_output_datalen (flushdata, sizeof (flushdata));
	      set_neturg ();
	      DEBUG (debug_options, 1, printoption ("td: send IAC", DM));
	    }

	  if (his_state_is_will (TELOPT_LFLOW)
	      && (c & (TIOCPKT_NOSTOP | TIOCPKT_DOSTOP)))
	    {
	      int newflow = (c & TIOCPKT_DOSTOP) ? 1 : 0;
	      if (newflow != flowmode)
		{
		  net_output_data ("%c%c%c%c%c%c",
				   IAC, SB, TELOPT_LFLOW,
				   flowmode ? LFLOW_ON : LFLOW_OFF, IAC, SE);
		}
	    }

	  pty_get_char (0);	/* Discard the TIOCPKT preamble.  */
	}

      while (pty_input_level () > 0)
	{
	  if (net_buffer_is_full ())
	    break;
	  c = pty_get_char (0);
	  if (c == IAC)
	    net_output_byte (c);
	  net_output_byte (c);
	  if (c == '\r' && my_state_is_wont (TELOPT_BINARY))
	    {
	      if (pty_input_level () > 0 && pty_get_char (1) == '\n')
		net_output_byte (pty_get_char (0));
	      else
		net_output_byte (0);
	    }
	}

      if (FD_ISSET (net, &obits) && net_output_level () > 0)
	netflush ();
      if (net_input_level () > 0)
	telrcv ();

      if (FD_ISSET (pty, &obits) && pty_output_level () > 0)
	ptyflush ();

      /* Attending to the child must come last in the loop,
       * so as to let pending data be flushed, mainly to the
       * benefit of the remote and expecting client.
       */
      if (pending_sigchld) {
	/* Check for pending output, independently of OBITS.  */
	if (net_output_level () > 0)
	  netflush ();

	cleanup (SIGCHLD);	/* Not returning from this.  */
      }
    }

  cleanup (0);
  /* NOT REACHED */

  return 0;
}

void
print_hostinfo (void)
{
  char *im = NULL;
  char *str;
#ifdef HAVE_UNAME
  struct utsname u;

  if (uname (&u) >= 0)
    {
      im = malloc (strlen (UNAME_IM_PREFIX)
		   + strlen (u.sysname)
		   + 1 + strlen (u.release) + strlen (UNAME_IM_SUFFIX) + 1);
      if (im)
	sprintf (im, "%s%s %s%s",
		 UNAME_IM_PREFIX, u.sysname, u.release, UNAME_IM_SUFFIX);
    }
#endif /* HAVE_UNAME */
  if (!im)
    im = xstrdup ("\r\n\r\nUNIX (%l) (%t)\r\n\r\n");

  str = expand_line (im);
  free (im);

  DEBUG (debug_pty_data, 1, debug_output_data ("sending %s", str));
  pty_input_putback (str, strlen (str));
  free (str);
}

static void
chld_is_done (int sig _GL_UNUSED_PARAMETER)
{
  pending_sigchld = 1;
}
