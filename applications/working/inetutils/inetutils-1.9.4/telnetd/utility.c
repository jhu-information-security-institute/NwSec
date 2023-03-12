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

#define TELOPTS
#define TELCMDS
#define SLC_NAMES
#include "telnetd.h"
#include <stdarg.h>
#ifdef HAVE_TERMIO_H
# include <termio.h>
#endif
#include <time.h>

#if defined AUTHENTICATION || defined ENCRYPTION
# include <libtelnet/misc.h>
# define NET_ENCRYPT net_encrypt
#else
# define NET_ENCRYPT()
#endif

#ifdef HAVE_TERMCAP_TGETENT
# include <termcap.h>
#elif defined HAVE_CURSES_TGETENT
# include <curses.h>
# include <term.h>
#endif

#if defined HAVE_STREAMSPTY && defined HAVE_GETMSG	\
  && defined HAVE_STROPTS_H
# include <stropts.h>
#endif

static char netobuf[BUFSIZ + NETSLOP], *nfrontp, *nbackp;
static char *neturg;		/* one past last byte of urgent data */
#ifdef  ENCRYPTION
static char *nclearto;
#endif

static char ptyobuf[BUFSIZ + NETSLOP], *pfrontp, *pbackp;

static char netibuf[BUFSIZ], *netip;
static int ncc;

static char ptyibuf[BUFSIZ], *ptyip;
static int pcc;

int not42;

static int
readstream (int p, char *ibuf, int bufsize)
{
#ifndef HAVE_STREAMSPTY
  return read (p, ibuf, bufsize);
#else
  int flags = 0;
  int ret = 0;
  struct termios *tsp;
#ifdef HAVE_TERMIO_H
  struct termio *tp;
#endif
  struct iocblk *ip;
  char vstop, vstart;
  int ixon;
  int newflow;
  struct strbuf strbufc, strbufd;
  unsigned char ctlbuf[BUFSIZ];
  static int flowstate = -1;

  strbufc.maxlen = BUFSIZ;
  strbufc.buf = (char *) ctlbuf;
  strbufd.maxlen = bufsize - 1;
  strbufd.len = 0;
  strbufd.buf = ibuf + 1;
  ibuf[0] = 0;

  ret = getmsg (p, &strbufc, &strbufd, &flags);
  if (ret < 0)			/* error of some sort -- probably EAGAIN */
    return -1;

  if (strbufc.len <= 0 || ctlbuf[0] == M_DATA)
    {
      /* data message */
      if (strbufd.len > 0)	/* real data */
	return strbufd.len + 1;	/* count header char */
      else
	{
	  /* nothing there */
	  errno = EAGAIN;
	  return -1;
	}
    }

  /*
   * It's a control message.  Return 1, to look at the flag we set
   */

  switch (ctlbuf[0])
    {
    case M_FLUSH:
      if (ibuf[1] & FLUSHW)
	ibuf[0] = TIOCPKT_FLUSHWRITE;
      return 1;

    case M_IOCTL:
      ip = (struct iocblk *) (ibuf + 1);

      switch (ip->ioc_cmd)
	{
	case TCSETS:
	case TCSETSW:
	case TCSETSF:
	  tsp = (struct termios *) (ibuf + 1 + sizeof (struct iocblk));
	  vstop = tsp->c_cc[VSTOP];
	  vstart = tsp->c_cc[VSTART];
	  ixon = tsp->c_iflag & IXON;
	  break;

#ifdef HAVE_TERMIO_H
	case TCSETA:
	case TCSETAW:
	case TCSETAF:
	  tp = (struct termio *) (ibuf + 1 + sizeof (struct iocblk));
	  vstop = tp->c_cc[VSTOP];
	  vstart = tp->c_cc[VSTART];
	  ixon = tp->c_iflag & IXON;
	  break;
#endif /* HAVE_TERMIO_H */

	default:
	  errno = EAGAIN;
	  return -1;
	}

      newflow = (ixon && (vstart == 021) && (vstop == 023)) ? 1 : 0;
      if (newflow != flowstate)	/* it's a change */
	{
	  flowstate = newflow;
	  ibuf[0] = newflow ? TIOCPKT_DOSTOP : TIOCPKT_NOSTOP;
	  return 1;
	}
    }

  /* nothing worth doing anything about */
  errno = EAGAIN;
  return -1;
#endif /* HAVE_STREAMSPTY */
}

/* ************************************************************************* */
/* Net and PTY I/O functions */

void
io_setup (void)
{
  pfrontp = pbackp = ptyobuf;
  nfrontp = nbackp = netobuf;
#ifdef  ENCRYPTION
  nclearto = 0;
#endif
  netip = netibuf;
  ptyip = ptyibuf;
}

void
set_neturg (void)
{
  neturg = nfrontp - 1;
}

/* net-buffers */

void
net_output_byte (int c)
{
  *nfrontp++ = c;
}

int
net_output_data (const char *format, ...)
{
  va_list args;
  size_t remaining, ret;

  va_start (args, format);
  remaining = BUFSIZ - (nfrontp - netobuf);
  /* try a netflush() if the room is too low */
  if (strlen (format) > remaining || BUFSIZ / 4 > remaining)
    {
      netflush ();
      remaining = BUFSIZ - (nfrontp - netobuf);
    }
  ret = vsnprintf (nfrontp, remaining, format, args);
  nfrontp += ((ret < remaining - 1) ? ret : remaining - 1);
  va_end (args);
  return ret;
}

int
net_output_datalen (const void *buf, size_t l)
{
  size_t remaining;

  remaining = BUFSIZ - (nfrontp - netobuf);
  if (remaining < l)
    {
      netflush ();
      remaining = BUFSIZ - (nfrontp - netobuf);
    }
  if (remaining < l)
    return -1;
  memmove (nfrontp, buf, l);
  nfrontp += l;
  return (int) l;
}

int
net_input_level (void)
{
  return ncc;
}

int
net_output_level (void)
{
  return nfrontp - nbackp;
}

int
net_buffer_is_full (void)
{
  return (&netobuf[BUFSIZ] - nfrontp) < 2;
}

int
net_get_char (int peek)
{
  if (peek)
    return *netip;
  else if (ncc > 0)
    {
      ncc--;
      return *netip++ & 0377;
    }

  return 0;
}

int
net_read (void)
{
  ncc = read (net, netibuf, sizeof (netibuf));
  if (ncc < 0 && errno == EWOULDBLOCK)
    ncc = 0;
  else if (ncc == 0)
    {
      syslog (LOG_INFO, "telnetd:  peer died");
      cleanup (0);
      /* NOT REACHED */
    }
  else if (ncc > 0)
    {
      netip = netibuf;
      DEBUG (debug_report, 1,
	     debug_output_data ("td: netread %d chars\r\n", ncc));
      DEBUG (debug_net_data, 1, printdata ("nd", netip, ncc));
    }
  return ncc;
}

/* PTY buffer functions */

int
pty_buffer_is_full (void)
{
  return (&ptyobuf[BUFSIZ] - pfrontp) < 2;
}

void
pty_output_byte (int c)
{
  *pfrontp++ = c;
}

void
pty_output_datalen (const void *data, size_t len)
{
  if ((size_t) (&ptyobuf[BUFSIZ] - pfrontp) > len)
    ptyflush ();
  memcpy (pfrontp, data, len);
  pfrontp += len;
}

int
pty_input_level (void)
{
  return pcc;
}

int
pty_output_level (void)
{
  return pfrontp - pbackp;
}

void
ptyflush (void)
{
  int n;

  if ((n = pfrontp - pbackp) > 0)
    {
      DEBUG (debug_report, 1,
	     debug_output_data ("td: ptyflush %d chars\r\n", n));
      DEBUG (debug_pty_data, 1, printdata ("pd", pbackp, n));
      n = write (pty, pbackp, n);
    }

  if (n < 0)
    {
      if (errno == EWOULDBLOCK || errno == EINTR)
	return;
      cleanup (0);
      /* NOT REACHED */
    }

  pbackp += n;
  if (pbackp == pfrontp)
    pbackp = pfrontp = ptyobuf;
}

int
pty_get_char (int peek)
{
  if (peek)
    return *ptyip;
  else if (pcc > 0)
    {
      pcc--;
      return *ptyip++ & 0377;
    }

  return 0;
}

int
pty_input_putback (const char *str, size_t len)
{
  if (len > (size_t) (&ptyibuf[BUFSIZ] - ptyip))
    len = &ptyibuf[BUFSIZ] - ptyip;
  strncpy (ptyip, str, len);
  pcc += len;

  return 0;
}

int
pty_read (void)
{
  pcc = readstream (pty, ptyibuf, BUFSIZ);
  if (pcc < 0 && (errno == EWOULDBLOCK
#ifdef	EAGAIN
		  || errno == EAGAIN
#endif
		  || errno == EIO))
    pcc = 0;
  ptyip = ptyibuf;

  DEBUG (debug_report, 1, debug_output_data ("td: ptyread %d chars\r\n", pcc));
  DEBUG (debug_pty_data, 1, printdata ("pd", ptyip, pcc));
  return pcc;
}

/* ************************************************************************* */


/* io_drain ()
 *
 *
 *	A small subroutine to flush the network output buffer, get some data
 * from the network, and pass it through the telnet state machine.  We
 * also flush the pty input buffer (by dropping its data) if it becomes
 * too full.
 */

void
io_drain (void)
{
  fd_set rfds;

  DEBUG (debug_report, 1, debug_output_data ("td: ttloop\r\n"));
  if (nfrontp - nbackp > 0)
    netflush ();

  FD_ZERO(&rfds);
  FD_SET(net, &rfds);
  if (1 != select(net + 1, &rfds, NULL, NULL, NULL))
    {
      syslog (LOG_INFO, "ttloop:  select: %m\n");
      exit (EXIT_FAILURE);
    }

  ncc = read (net, netibuf, sizeof netibuf);
  if (ncc < 0)
    {
      syslog (LOG_INFO, "ttloop:  read: %m\n");
      exit (EXIT_FAILURE);
    }
  else if (ncc == 0)
    {
      syslog (LOG_INFO, "ttloop:  peer died: %m\n");
      exit (EXIT_FAILURE);
    }
  DEBUG (debug_report, 1,
	 debug_output_data ("td: ttloop read %d chars\r\n", ncc));
  netip = netibuf;
  telrcv ();			/* state machine */
  if (ncc > 0)
    {
      pfrontp = pbackp = ptyobuf;
      telrcv ();
    }
}				/* end of ttloop */

/*
 * Check a descriptor to see if out of band data exists on it.
 */
/* int	s; socket number */
int
stilloob (int s)
{
  static struct timeval timeout = { 0, 0 };
  fd_set excepts;
  int value;

  do
    {
      FD_ZERO (&excepts);
      FD_SET (s, &excepts);
      value = select (s + 1, (fd_set *) 0, (fd_set *) 0, &excepts, &timeout);
    }
  while (value == -1 && errno == EINTR);

  if (value < 0)
    fatalperror (pty, "select");

  return FD_ISSET (s, &excepts);
}

/*
 * nextitem()
 *
 *	Return the address of the next "item" in the TELNET data
 * stream.  This will be the address of the next character if
 * the current address is a user data character, or it will
 * be the address of the character following the TELNET command
 * if the current address is a TELNET IAC ("I Am a Command")
 * character.
 */
char *
nextitem (char *current)
{
  if ((*current & 0xff) != IAC)
    return current + 1;

  switch (*(current + 1) & 0xff)
    {
    case DO:
    case DONT:
    case WILL:
    case WONT:
      return current + 3;

    case SB:			/* loop forever looking for the SE */
      {
	char *look = current + 2;

	for (;;)
	  if ((*look++ & 0xff) == IAC && (*look++ & 0xff) == SE)
	    return look;

      default:
	return current + 2;
      }
    }
}				/* end of nextitem */


/*
 * netclear()
 *
 *	We are about to do a TELNET SYNCH operation.  Clear
 * the path to the network.
 *
 *	Things are a bit tricky since we may have sent the first
 * byte or so of a previous TELNET command into the network.
 * So, we have to scan the network buffer from the beginning
 * until we are up to where we want to be.
 *
 *	A side effect of what we do, just to keep things
 * simple, is to clear the urgent data pointer.  The principal
 * caller should be setting the urgent data pointer AFTER calling
 * us in any case.
 */
#define wewant(p)					\
  ((nfrontp > p) && ((*p&0xff) == IAC) &&		\
   ((*(p+1)&0xff) != EC) && ((*(p+1)&0xff) != EL))


void
netclear (void)
{
  char *thisitem, *next;
  char *good;

#ifdef	ENCRYPTION
  thisitem = nclearto > netobuf ? nclearto : netobuf;
#else /* ENCRYPTION */
  thisitem = netobuf;
#endif /* ENCRYPTION */

  while ((next = nextitem (thisitem)) <= nbackp)
    thisitem = next;

  /* Now, thisitem is first before/at boundary. */

#ifdef	ENCRYPTION
  good = nclearto > netobuf ? nclearto : netobuf;
#else /* ENCRYPTION */
  good = netobuf;		/* where the good bytes go */
#endif /* ENCRYPTION */

  while (nfrontp > thisitem)
    {
      if (wewant (thisitem))
	{
	  int length;

	  for (next = thisitem; wewant (next) && nfrontp > next;
	       next = nextitem (next))
	    ;

	  length = next - thisitem;
	  memmove (good, thisitem, length);
	  good += length;
	  thisitem = next;
	}
      else
	{
	  thisitem = nextitem (thisitem);
	}
    }

  nbackp = netobuf;
  nfrontp = good;		/* next byte to be sent */
  neturg = 0;
}				/* end of netclear */

/*
 *  netflush
 *		Send as much data as possible to the network,
 *	handling requests for urgent data.
 */
void
netflush (void)
{
  int n;

  if ((n = nfrontp - nbackp) > 0)
    {
      NET_ENCRYPT ();

      /*
       * if no urgent data, or if the other side appears to be an
       * old 4.2 client (and thus unable to survive TCP urgent data),
       * write the entire buffer in non-OOB mode.
       */
      if (!neturg || !not42)
	n = write (net, nbackp, n);	/* normal write */
      else
	{
	  n = neturg - nbackp;
	  /*
	   * In 4.2 (and 4.3) systems, there is some question about
	   * what byte in a sendOOB operation is the "OOB" data.
	   * To make ourselves compatible, we only send ONE byte
	   * out of band, the one WE THINK should be OOB (though
	   * we really have more the TCP philosophy of urgent data
	   * rather than the Unix philosophy of OOB data).
	   */
	  if (n > 1)
	    n = send (net, nbackp, n - 1, 0);	/* send URGENT all by itself */
	  else
	    n = send (net, nbackp, n, MSG_OOB);	/* URGENT data */
	}
    }
  if (n < 0)
    {
      if (errno == EWOULDBLOCK || errno == EINTR)
	return;
      cleanup (0);
      /* NOT REACHED */
    }

  nbackp += n;
#ifdef	ENCRYPTION
  if (nbackp > nclearto)
    nclearto = 0;
#endif /* ENCRYPTION */
  if (nbackp >= neturg)
    neturg = 0;

  if (nbackp == nfrontp)
    {
      nbackp = nfrontp = netobuf;
#ifdef	ENCRYPTION
      nclearto = 0;
#endif /* ENCRYPTION */
    }
  DEBUG (debug_report, 1, debug_output_data ("td: netflush %d chars\r\n", n));

}				/* end of netflush */

/*
 * miscellaneous functions doing a variety of little jobs follow ...
 */

void
fatal (int f, char *msg)
{
  char buf[BUFSIZ];

  snprintf (buf, sizeof buf, "telnetd: %s.\r\n", msg);
#ifdef	ENCRYPTION
  if (encrypt_output)
    {
      /*
       * Better turn off encryption first....
       * Hope it flushes...
       */
      encrypt_send_end ();
      netflush ();
    }
#endif /* ENCRYPTION */
  write (f, buf, (int) strlen (buf));
  sleep (1);
  /*FIXME*/ exit (EXIT_FAILURE);
}

void
fatalperror (int f, char *msg)
{
  char buf[BUFSIZ];

  snprintf (buf, sizeof buf, "%s: %s", msg, strerror (errno));
  fatal (f, buf);
}

/* ************************************************************************* */
/* Terminal determination */

static unsigned char ttytype_sbbuf[] = {
  IAC, SB, TELOPT_TTYPE, TELQUAL_SEND, IAC, SE
};

static void
_gettermname (void)
{
  if (his_state_is_wont (TELOPT_TTYPE))
    return;
  settimer (baseline);
  net_output_datalen (ttytype_sbbuf, sizeof ttytype_sbbuf);
  ttloop (sequenceIs (ttypesubopt, baseline));
}

/*
 * Changes terminaltype.
 */
int
getterminaltype (char *uname, size_t len)
{
  int retval = -1;

  settimer (baseline);
#if defined AUTHENTICATION
  /*
   * Handle the Authentication option before we do anything else.
   * Distinguish the available modes by level:
   *
   *   off:			Authentication is forbidden.
   *   none:			Volontary authentication.
   *   user, valid, other:	Mandatory authentication only.
   */
  if (auth_level < 0)
    send_wont (TELOPT_AUTHENTICATION, 1);
  else
    {
      if (auth_level > 0)
	send_do (TELOPT_AUTHENTICATION, 1);
      else
	send_will (TELOPT_AUTHENTICATION, 1);

      ttloop (his_will_wont_is_changing (TELOPT_AUTHENTICATION));

      if (his_state_is_will (TELOPT_AUTHENTICATION))
	retval = auth_wait (uname, len);
    }
#else /* !AUTHENTICATION */
  (void) uname;	/* Silence warning.  */
#endif

#ifdef	ENCRYPTION
  send_will (TELOPT_ENCRYPT, 1);
#endif /* ENCRYPTION */
  send_do (TELOPT_TTYPE, 1);
  send_do (TELOPT_TSPEED, 1);
  send_do (TELOPT_XDISPLOC, 1);
  send_do (TELOPT_NEW_ENVIRON, 1);
  send_do (TELOPT_OLD_ENVIRON, 1);

#ifdef ENCRYPTION
  ttloop (his_do_dont_is_changing (TELOPT_ENCRYPT)
	  || his_will_wont_is_changing (TELOPT_TTYPE)
	  || his_will_wont_is_changing (TELOPT_TSPEED)
	  || his_will_wont_is_changing (TELOPT_XDISPLOC)
	  || his_will_wont_is_changing (TELOPT_NEW_ENVIRON)
	  || his_will_wont_is_changing (TELOPT_OLD_ENVIRON));
#else
  ttloop (his_will_wont_is_changing (TELOPT_TTYPE)
	  || his_will_wont_is_changing (TELOPT_TSPEED)
	  || his_will_wont_is_changing (TELOPT_XDISPLOC)
	  || his_will_wont_is_changing (TELOPT_NEW_ENVIRON)
	  || his_will_wont_is_changing (TELOPT_OLD_ENVIRON));
#endif

#ifdef  ENCRYPTION
  if (his_state_is_will (TELOPT_ENCRYPT))
    encrypt_wait ();
#endif

  if (his_state_is_will (TELOPT_TSPEED))
    {
      static unsigned char sb[] =
	{ IAC, SB, TELOPT_TSPEED, TELQUAL_SEND, IAC, SE };

      net_output_datalen (sb, sizeof sb);
    }
  if (his_state_is_will (TELOPT_XDISPLOC))
    {
      static unsigned char sb[] =
	{ IAC, SB, TELOPT_XDISPLOC, TELQUAL_SEND, IAC, SE };

      net_output_datalen (sb, sizeof sb);
    }
  if (his_state_is_will (TELOPT_NEW_ENVIRON))
    {
      static unsigned char sb[] =
	{ IAC, SB, TELOPT_NEW_ENVIRON, TELQUAL_SEND, IAC, SE };

      net_output_datalen (sb, sizeof sb);
    }
  else if (his_state_is_will (TELOPT_OLD_ENVIRON))
    {
      static unsigned char sb[] =
	{ IAC, SB, TELOPT_OLD_ENVIRON, TELQUAL_SEND, IAC, SE };

      net_output_datalen (sb, sizeof sb);
    }
  if (his_state_is_will (TELOPT_TTYPE))
    net_output_datalen (ttytype_sbbuf, sizeof ttytype_sbbuf);

  if (his_state_is_will (TELOPT_TSPEED))
    ttloop (sequenceIs (tspeedsubopt, baseline));
  if (his_state_is_will (TELOPT_XDISPLOC))
    ttloop (sequenceIs (xdisplocsubopt, baseline));
  if (his_state_is_will (TELOPT_NEW_ENVIRON))
    ttloop (sequenceIs (environsubopt, baseline));
  if (his_state_is_will (TELOPT_OLD_ENVIRON))
    ttloop (sequenceIs (oenvironsubopt, baseline));

  if (his_state_is_will (TELOPT_TTYPE))
    {
      char *first = NULL, *last = NULL;

      ttloop (sequenceIs (ttypesubopt, baseline));
      /*
       * If the other side has already disabled the option, then
       * we have to just go with what we (might) have already gotten.
       */
      if (his_state_is_will (TELOPT_TTYPE) && !terminaltypeok (terminaltype))
	{
	  free (first);
	  first = xstrdup (terminaltype);
	  for (;;)
	    {
	      /* Save the unknown name, and request the next name. */
	      free (last);
	      last = xstrdup (terminaltype);
	      _gettermname ();
	      if (terminaltypeok (terminaltype))
		break;
	      if ((strcmp (last, terminaltype) == 0)
		  || his_state_is_wont (TELOPT_TTYPE))
		{
		  /*
		   * We've hit the end.  If this is the same as
		   * the first name, just go with it.
		   */
		  if (strcmp (first, terminaltype) == 0)
		    break;
		  /*
		   * Get the terminal name one more time, so that
		   * RFC1091 compliant telnets will cycle back to
		   * the start of the list.
		   */
		  _gettermname ();
		  if (strcmp (first, terminaltype) != 0)
		    {
		      free (terminaltype);
		      terminaltype = xstrdup (first);
		    }
		  break;
		}
	    }
	}
      free (first);
      free (last);
    }
  return retval;
}

/*
 * Exit status:
 *
 *  1   Accepted terminal type, or inconclusive,
 *  0   Explicitly unsupported type.
 */
int
terminaltypeok (char *s)
{
#ifdef HAVE_TGETENT
  char buf[2048];

  if (terminaltype == NULL)
    return 1;

  if (tgetent (buf, s) == 0)
    return 0;
#endif /* HAVE_TGETENT */

  return 1;
}


/* ************************************************************************* */
/* Debugging support */

static FILE *debug_fp = NULL;

static int
debug_open (void)
{
  int um = umask (077);
  if (!debug_fp)
    debug_fp = fopen ("/tmp/telnet.debug", "a");
  umask (um);
  return debug_fp == NULL;
}

static int
debug_close (void)
{
  if (debug_fp)
    fclose (debug_fp);
  debug_fp = NULL;

  return 0;
}

void
debug_output_datalen (const char *data, size_t len)
{
  if (debug_open ())
    return;

  fwrite (data, 1, len, debug_fp);
  debug_close ();
}

void
debug_output_data (const char *fmt, ...)
{
  va_list ap;
  if (debug_open ())
    return;
  va_start (ap, fmt);
  vfprintf (debug_fp, fmt, ap);
  va_end (ap);
  debug_close ();
}

/*
 * Print telnet options and commands in plain text, if possible.
 */
void
printoption (char *fmt, int option)
{
  if (TELOPT_OK (option))
    debug_output_data ("%s %s\r\n", fmt, TELOPT (option));
  else if (TELCMD_OK (option))
    debug_output_data ("%s %s\r\n", fmt, TELCMD (option));
  else
    debug_output_data ("%s %d\r\n", fmt, option);
}

/* char	direction; '<' or '>' */
/* unsigned char *pointer;  where suboption data sits */
/* int	length;	 length of suboption data */
void
printsub (int direction, unsigned char *pointer, int length)
{
  int i = 0;

#if defined AUTHENTICATION || defined ENCRYPTION
  char buf[512];
#endif

  /* Silence unwanted debugging to '/tmp/telnet.debug'.
   *
   * XXX: Better location?
   */
  if ((pointer[0] == TELOPT_AUTHENTICATION && debug_level[debug_auth] < 1)
      || (pointer[0] == TELOPT_ENCRYPT && debug_level[debug_encr] < 1))
    return;

  if (direction)
    {
      debug_output_data ("td: %s suboption ",
			 direction == '<' ? "recv" : "send");
      if (length >= 3)
	{
	  int j;

	  i = pointer[length - 2];
	  j = pointer[length - 1];

	  if (i != IAC || j != SE)
	    {
	      debug_output_data ("(terminated by ");
	      if (TELOPT_OK (i))
		debug_output_data ("%s ", TELOPT (i));
	      else if (TELCMD_OK (i))
		debug_output_data ("%s ", TELCMD (i));
	      else
		debug_output_data ("%d ", i);
	      if (TELOPT_OK (j))
		debug_output_data ("%s", TELOPT (j));
	      else if (TELCMD_OK (j))
		debug_output_data ("%s", TELCMD (j));
	      else
		debug_output_data ("%d", j);
	      debug_output_data (", not IAC SE!) ");
	    }
	}
      length -= 2;
    }
  if (length < 1)
    {
      debug_output_data ("(Empty suboption??\?)");
      return;
    }

  switch (pointer[0])
    {
    case TELOPT_TTYPE:
      debug_output_data ("TERMINAL-TYPE ");
      switch (pointer[1])
	{
	case TELQUAL_IS:
	  debug_output_data ("IS \"%.*s\"", length - 2, (char *) pointer + 2);
	  break;

	case TELQUAL_SEND:
	  debug_output_data ("SEND");
	  break;

	default:
	  debug_output_data ("- unknown qualifier %d (0x%x).",
			     pointer[1], pointer[1]);
	}
      break;

    case TELOPT_TSPEED:
      debug_output_data ("TERMINAL-SPEED");
      if (length < 2)
	{
	  debug_output_data (" (empty suboption??\?)");
	  break;
	}

      switch (pointer[1])
	{
	case TELQUAL_IS:
	  debug_output_data (" IS %.*s", length - 2, (char *) pointer + 2);
	  break;

	default:
	  if (pointer[1] == 1)
	    debug_output_data (" SEND");
	  else
	    debug_output_data (" %d (unknown)", pointer[1]);
	  for (i = 2; i < length; i++)
	    {
	      debug_output_data (" ?%d?", pointer[i]);
	    }
	  break;
	}
      break;

    case TELOPT_LFLOW:
      debug_output_data ("TOGGLE-FLOW-CONTROL");
      if (length < 2)
	{
	  debug_output_data (" (empty suboption??\?)");
	  break;
	}

      switch (pointer[1])
	{
	case LFLOW_OFF:
	  debug_output_data (" OFF");
	  break;

	case LFLOW_ON:
	  debug_output_data (" ON");
	  break;

	case LFLOW_RESTART_ANY:
	  debug_output_data (" RESTART-ANY");
	  break;

	case LFLOW_RESTART_XON:
	  debug_output_data (" RESTART-XON");
	  break;

	default:
	  debug_output_data (" %d (unknown)", pointer[1]);
	}

      for (i = 2; i < length; i++)
	debug_output_data (" ?%d?", pointer[i]);
      break;

    case TELOPT_NAWS:
      debug_output_data ("NAWS");
      if (length < 2)
	{
	  debug_output_data (" (empty suboption??\?)");
	  break;
	}
      if (length == 2)
	{
	  debug_output_data (" ?%d?", pointer[1]);
	  break;
	}

      debug_output_data (" %d %d (%d)",
			 pointer[1], pointer[2],
			 (int) ((((unsigned int) pointer[1]) << 8) |
				((unsigned int) pointer[2])));
      if (length == 4)
	{
	  debug_output_data (" ?%d?", pointer[3]);
	  break;
	}

      debug_output_data (" %d %d (%d)",
			 pointer[3], pointer[4],
			 (int) ((((unsigned int) pointer[3]) << 8) |
				((unsigned int) pointer[4])));
      for (i = 5; i < length; i++)
	debug_output_data (" ?%d?", pointer[i]);
      break;

    case TELOPT_LINEMODE:
      debug_output_data ("LINEMODE ");
      if (length < 2)
	{
	  debug_output_data (" (empty suboption??\?)");
	  break;
	}

      switch (pointer[1])
	{
	case WILL:
	  debug_output_data ("WILL ");
	  goto common;

	case WONT:
	  debug_output_data ("WONT ");
	  goto common;

	case DO:
	  debug_output_data ("DO ");
	  goto common;

	case DONT:
	  debug_output_data ("DONT ");

	common:
	  if (length < 3)
	    {
	      debug_output_data ("(no option??\?)");
	      break;
	    }
	  switch (pointer[2])
	    {
	    case LM_FORWARDMASK:
	      debug_output_data ("Forward Mask");
	      for (i = 3; i < length; i++)
		debug_output_data (" %x", pointer[i]);
	      break;

	    default:
	      debug_output_data ("%d (unknown)", pointer[2]);
	      for (i = 3; i < length; i++)
		debug_output_data (" %d", pointer[i]);
	      break;
	    }
	  break;

	case LM_SLC:
	  debug_output_data ("SLC");
	  for (i = 2; i < length - 2; i += 3)
	    {
	      if (SLC_NAME_OK (pointer[i + SLC_FUNC]))
		debug_output_data (" %s", SLC_NAME (pointer[i + SLC_FUNC]));
	      else
		debug_output_data (" %d", pointer[i + SLC_FUNC]);
	      switch (pointer[i + SLC_FLAGS] & SLC_LEVELBITS)
		{
		case SLC_NOSUPPORT:
		  debug_output_data (" NOSUPPORT");
		  break;

		case SLC_CANTCHANGE:
		  debug_output_data (" CANTCHANGE");
		  break;

		case SLC_VARIABLE:
		  debug_output_data (" VARIABLE");
		  break;

		case SLC_DEFAULT:
		  debug_output_data (" DEFAULT");
		  break;
		}

	      debug_output_data ("%s%s%s",
				 pointer[i +
					 SLC_FLAGS] & SLC_ACK ? "|ACK" : "",
				 pointer[i +
					 SLC_FLAGS] & SLC_FLUSHIN ? "|FLUSHIN"
				 : "",
				 pointer[i +
					 SLC_FLAGS] & SLC_FLUSHOUT ?
				 "|FLUSHOUT" : "");
	      if (pointer[i + SLC_FLAGS] &
		  ~(SLC_ACK | SLC_FLUSHIN | SLC_FLUSHOUT | SLC_LEVELBITS))
		debug_output_data ("(0x%x)", pointer[i + SLC_FLAGS]);
	      debug_output_data (" %d;", pointer[i + SLC_VALUE]);

	      if ((pointer[i + SLC_VALUE] == IAC) &&
		  (pointer[i + SLC_VALUE + 1] == IAC))
		i++;
	    }

	  for (; i < length; i++)
	    debug_output_data (" ?%d?", pointer[i]);
	  break;

	case LM_MODE:
	  debug_output_data ("MODE ");
	  if (length < 3)
	    {
	      debug_output_data ("(no mode??\?)");
	      break;
	    }
	  {
	    char tbuf[32];
	    snprintf (tbuf, sizeof (tbuf), "%s%s%s%s%s",
		      pointer[2] & MODE_EDIT ? "|EDIT" : "",
		      pointer[2] & MODE_TRAPSIG ? "|TRAPSIG" : "",
		      pointer[2] & MODE_SOFT_TAB ? "|SOFT_TAB" : "",
		      pointer[2] & MODE_LIT_ECHO ? "|LIT_ECHO" : "",
		      pointer[2] & MODE_ACK ? "|ACK" : "");
	    debug_output_data ("%s", tbuf[1] ? &tbuf[1] : "0");
	  }

	  if (pointer[2] & ~(MODE_EDIT | MODE_TRAPSIG | MODE_ACK))
	    debug_output_data (" (0x%x)", pointer[2]);

	  for (i = 3; i < length; i++)
	    debug_output_data (" ?0x%x?", pointer[i]);
	  break;

	default:
	  debug_output_data ("%d (unknown)", pointer[1]);
	  for (i = 2; i < length; i++)
	    debug_output_data (" %d", pointer[i]);
	}
      break;

    case TELOPT_STATUS:
      {
	char *cp;
	int j, k;

	debug_output_data ("STATUS");

	switch (pointer[1])
	  {
	  default:
	    if (pointer[1] == TELQUAL_SEND)
	      debug_output_data (" SEND");
	    else
	      debug_output_data (" %d (unknown)", pointer[1]);
	    for (i = 2; i < length; i++)
	      debug_output_data (" ?%d?", pointer[i]);
	    break;

	  case TELQUAL_IS:
	    debug_output_data (" IS\r\n");

	    for (i = 2; i < length; i++)
	      {
		switch (pointer[i])
		  {
		  case DO:
		    cp = "DO";
		    goto common2;

		  case DONT:
		    cp = "DONT";
		    goto common2;

		  case WILL:
		    cp = "WILL";
		    goto common2;

		  case WONT:
		    cp = "WONT";
		    goto common2;

		  common2:
		    i++;
		    if (TELOPT_OK (pointer[i]))
		      debug_output_data (" %s %s\r\n", cp,
					 TELOPT (pointer[i]));
		    else
		      debug_output_data (" %s %d\r\n", cp, pointer[i]);
		    break;

		  case SB:
		    debug_output_data (" SB ");
		    i++;
		    j = k = i;
		    while (j < length)
		      {
			if (pointer[j] == SE)
			  {
			    if (j + 1 == length)
			      break;
			    if (pointer[j + 1] == SE)
			      j++;
			    else
			      break;
			  }
			pointer[k++] = pointer[j++];
		      }
		    printsub (0, &pointer[i], k - i);
		    if (i < length)
		      {
			debug_output_data (" SE");
			i = j;
		      }
		    else
		      i = j - 1;

		    debug_output_data ("\r\n");
		    break;

		  default:
		    debug_output_data (" %d", pointer[i]);
		    break;
		  }
	      }
	    break;
	  }
	break;
      }

    case TELOPT_XDISPLOC:
      debug_output_data ("X-DISPLAY-LOCATION ");
      switch (pointer[1])
	{
	case TELQUAL_IS:
	  debug_output_data ("IS \"%.*s\"", length - 2, (char *) pointer + 2);
	  break;
	case TELQUAL_SEND:
	  debug_output_data ("SEND");
	  break;
	default:
	  debug_output_data ("- unknown qualifier %d (0x%x).",
			     pointer[1], pointer[1]);
	}
      break;

    case TELOPT_NEW_ENVIRON:
      debug_output_data ("NEW-ENVIRON ");
      goto env_common1;

    case TELOPT_OLD_ENVIRON:
      debug_output_data ("OLD-ENVIRON");
    env_common1:
      switch (pointer[1])
	{
	case TELQUAL_IS:
	  debug_output_data ("IS ");
	  goto env_common;

	case TELQUAL_SEND:
	  debug_output_data ("SEND ");
	  goto env_common;

	case TELQUAL_INFO:
	  debug_output_data ("INFO ");

	env_common:
	  {
	    char *quote = "";
	    for (i = 2; i < length; i++)
	      {
		switch (pointer[i])
		  {
		  case NEW_ENV_VAR:
		    debug_output_data ("%sVAR ", quote);
		    quote = "";
		    break;

		  case NEW_ENV_VALUE:
		    debug_output_data ("%sVALUE ", quote);
		    quote = "";
		    break;

		  case ENV_ESC:
		    debug_output_data ("%sESC ", quote);
		    quote = "";
		    break;

		  case ENV_USERVAR:
		    debug_output_data ("%sUSERVAR ", quote);
		    quote = "";
		    break;

		  default:
		    if (isprint (pointer[i]) && pointer[i] != '"')
		      {
                        if (strcmp (quote, "") == 0)
			  {
			    debug_output_data ("\"");
			    quote = "\" ";
			  }
			debug_output_datalen ((char*) &pointer[i], 1);
		      }
		    else
		      {
			debug_output_data ("%s%03o ", quote, pointer[i]);
			quote = "";
		      }
		    break;
		  }
	      }
	    if (strcmp (quote, "\" ") == 0)
	      debug_output_data ("\"");
	    break;
	  }
	}
      break;

#if defined AUTHENTICATION
    case TELOPT_AUTHENTICATION:
      debug_output_data ("AUTHENTICATION");

      if (length < 2)
	{
	  debug_output_data (" (empty suboption??\?)");
	  break;
	}
      switch (pointer[1])
	{
	case TELQUAL_REPLY:
	case TELQUAL_IS:
	  debug_output_data (" %s ", (pointer[1] == TELQUAL_IS) ?
			     "IS" : "REPLY");
	  if (AUTHTYPE_NAME_OK (pointer[2]) && AUTHTYPE_NAME (pointer[2]))
	    debug_output_data ("%s ", AUTHTYPE_NAME (pointer[2]));
	  else
	    debug_output_data ("%d ", pointer[2]);
	  if (length < 3)
	    {
	      debug_output_data ("(partial suboption??\?)");
	      break;
	    }
	  debug_output_data ("%s|%s",
			     ((pointer[3] & AUTH_WHO_MASK) ==
			      AUTH_WHO_CLIENT) ? "CLIENT" : "SERVER",
			     ((pointer[3] & AUTH_HOW_MASK) ==
			      AUTH_HOW_MUTUAL) ? "MUTUAL" : "ONE-WAY");

	  auth_printsub (&pointer[1], length - 1, buf, sizeof (buf));
	  debug_output_data ("%s", buf);
	  break;

	case TELQUAL_SEND:
	  i = 2;
	  debug_output_data (" SEND ");
	  while (i < length)
	    {
	      if (AUTHTYPE_NAME_OK (pointer[i])
		  && AUTHTYPE_NAME (pointer[i]))
		debug_output_data ("%s ", AUTHTYPE_NAME (pointer[i]));
	      else
		debug_output_data ("%d ", pointer[i]);
	      if (++i >= length)
		{
		  debug_output_data ("(partial suboption??\?)");
		  break;
		}
	      debug_output_data ("%s|%s ",
				 ((pointer[i] & AUTH_WHO_MASK) ==
				  AUTH_WHO_CLIENT) ? "CLIENT" : "SERVER",
				 ((pointer[i] & AUTH_HOW_MASK) ==
				  AUTH_HOW_MUTUAL) ? "MUTUAL" : "ONE-WAY");
	      ++i;
	    }
	  break;

	case TELQUAL_NAME:
	  i = 2;
	  debug_output_data (" NAME \"");
	  debug_output_datalen ((char *) &pointer[i], length);
	  i += length;
	  debug_output_data ("\"");
	  break;

	default:
	  for (i = 2; i < length; i++)
	    debug_output_data (" ?%d?", pointer[i]);
	  break;
	}
      break;
#endif

#ifdef	ENCRYPTION
    case TELOPT_ENCRYPT:
      debug_output_data ("ENCRYPT");
      if (length < 2)
	{
	  debug_output_data (" (empty suboption??\?)");
	  break;
	}
      switch (pointer[1])
	{
	case ENCRYPT_START:
	  debug_output_data (" START");
	  break;

	case ENCRYPT_END:
	  debug_output_data (" END");
	  break;

	case ENCRYPT_REQSTART:
	  debug_output_data (" REQUEST-START");
	  break;

	case ENCRYPT_REQEND:
	  debug_output_data (" REQUEST-END");
	  break;

	case ENCRYPT_IS:
	case ENCRYPT_REPLY:
	  debug_output_data (" %s ", (pointer[1] == ENCRYPT_IS) ?
			     "IS" : "REPLY");
	  if (length < 3)
	    {
	      debug_output_data (" (partial suboption??\?)");
	      break;
	    }
	  if (ENCTYPE_NAME_OK (pointer[2]) && ENCTYPE_NAME (pointer[2]))
	    debug_output_data ("%s ", ENCTYPE_NAME (pointer[2]));
	  else
	    debug_output_data (" %d (unknown)", pointer[2]);

	  encrypt_printsub (&pointer[1], length - 1, buf, sizeof (buf));
	  debug_output_data ("%s", buf);
	  break;

	case ENCRYPT_SUPPORT:
	  i = 2;
	  debug_output_data (" SUPPORT ");
	  while (i < length)
	    {
	      if (ENCTYPE_NAME_OK (pointer[i]) && ENCTYPE_NAME (pointer[i]))
		debug_output_data ("%s ", ENCTYPE_NAME (pointer[i]));
	      else
		debug_output_data ("%d ", pointer[i]);
	      i++;
	    }
	  break;

	case ENCRYPT_ENC_KEYID:
	  debug_output_data (" ENC_KEYID", pointer[1]);
	  goto encommon;

	case ENCRYPT_DEC_KEYID:
	  debug_output_data (" DEC_KEYID", pointer[1]);
	  goto encommon;

	default:
	  debug_output_data (" %d (unknown)", pointer[1]);
	encommon:
	  for (i = 2; i < length; i++)
	    debug_output_data (" %d", pointer[i]);
	  break;
	}
      break;
#endif /* ENCRYPTION */

    default:
      if (TELOPT_OK (pointer[0]))
	debug_output_data ("%s (unknown)", TELOPT (pointer[0]));
      else
	debug_output_data ("%d (unknown)", pointer[0]);
      for (i = 1; i < length; i++)
	debug_output_data (" %d", pointer[i]);
      break;
    }
  debug_output_data ("\r\n");
}

/*
 * Dump a data buffer in hex and ascii to the output data stream.
 */
void
printdata (char *tag, char *ptr, int cnt)
{
  int i;
  char xbuf[30];

  while (cnt)
    {
      /* add a line of output */
      debug_output_data ("%s: ", tag);
      for (i = 0; i < 20 && cnt; i++)
	{
	  debug_output_data ("%02x", (unsigned char) *ptr);
	  xbuf[i] = isprint ((int) *ptr) ? *ptr : '.';

	  if (i % 2)
	    debug_output_data (" ");
	  cnt--;
	  ptr++;
	}

      xbuf[i] = '\0';
      debug_output_data (" %s\r\n", xbuf);
    }
}

#if defined AUTHENTICATION || defined ENCRYPTION

int
net_write (unsigned char *str, int len)
{
  return net_output_datalen (str, len);
}

void
net_encrypt ()
{
# ifdef	ENCRYPTION
  char *s = (nclearto > nbackp) ? nclearto : nbackp;
  if (s < nfrontp && encrypt_output)
    (*encrypt_output) ((unsigned char *) s, nfrontp - s);
  nclearto = nfrontp;
# endif	/* ENCRYPTION */
}

int
telnet_spin ()
{
  io_drain ();
  return 0;
}


#endif

/* ************************************************************************* */
/* String expansion functions */

#define EXP_STATE_CONTINUE 0
#define EXP_STATE_SUCCESS  1
#define EXP_STATE_ERROR    2

struct line_expander
{
  int state;			/* Current state */
  int level;			/* The nesting level */
  char *source;			/* The source string */
  char *cp;			/* Current position in the source */
  struct obstack stk;		/* Obstack for expanded version */
};

static char *_var_short_name (struct line_expander *exp);
static char *_var_long_name (struct line_expander *exp,
			     char *start, int length);
static char *_expand_var (struct line_expander *exp);
static void _expand_cond (struct line_expander *exp);
static void _skip_block (struct line_expander *exp);
static void _expand_block (struct line_expander *exp);

/* Expand a variable referenced by its short one-symbol name.
   Input: exp->cp points to the variable name.
   FIXME: not implemented */
char *
_var_short_name (struct line_expander *exp)
{
  char *q;
  char timebuf[64];
  time_t t;

  switch (*exp->cp++)
    {
    case 'a':
#ifdef AUTHENTICATION
      if (auth_level >= 0 && autologin == AUTH_VALID)
	return xstrdup ("ok");
#endif
      return NULL;

    case 'd':
      time (&t);
      strftime (timebuf, sizeof (timebuf),
		"%l:%M%p on %A, %d %B %Y", localtime (&t));
      return xstrdup (timebuf);

    case 'h':
      return xstrdup (remote_hostname);

    case 'l':
      return xstrdup (local_hostname);

    case 'L':
      return xstrdup (line);

    case 't':
      q = strchr (line + 1, '/');
      if (q)
	q++;
      else
	q = line;
      return xstrdup (q);

    case 'T':
      return terminaltype ? xstrdup (terminaltype) : NULL;

    case 'u':
      return user_name ? xstrdup (user_name) : NULL;

    case 'U':
      return getenv ("USER") ? xstrdup (getenv ("USER")) : xstrdup ("");

    default:
      exp->state = EXP_STATE_ERROR;
      return NULL;
    }
}

/* Expand a variable referenced by its long name.
   Input: exp->cp points to initial '('
   FIXME: not implemented */
char *
_var_long_name (struct line_expander *exp, char *start, int length)
{
  (void) start;		/* Silence warnings until implemented.  */
  (void) length;
  exp->state = EXP_STATE_ERROR;
  return NULL;
}

/* Expand a variable to its value.
   Input: exp->cp points one character _past_ % (or ?) */
char *
_expand_var (struct line_expander *exp)
{
  char *p;
  switch (*exp->cp)
    {
    case '{':
      /* Collect variable name */
      for (p = ++exp->cp; *exp->cp && *exp->cp != '}'; exp->cp++)
	;
      if (*exp->cp == 0)
	{
	  exp->cp = p;
	  exp->state = EXP_STATE_ERROR;
	  break;
	}
      p = _var_long_name (exp, p, exp->cp - p);
      exp->cp++;
      break;

    default:
      p = _var_short_name (exp);
      break;
    }
  return p;
}

/* Expand a conditional block. A conditional block is:
       %?<var>{true-stmt}[{false-stmt}]
   <var> may be either a one-symbol variable name or (string). The latter
   is not handled yet.
   On input exp->cp points to % character */
void
_expand_cond (struct line_expander *exp)
{
  char *p;

  if (*++exp->cp == '?')
    {
      /* condition */
      exp->cp++;
      p = _expand_var (exp);
      if (p)
	{
	  _expand_block (exp);
	  _skip_block (exp);
	}
      else
	{
	  _skip_block (exp);
	  _expand_block (exp);
	}
      free (p);
    }
  else
    {
      p = _expand_var (exp);
      if (p)
	obstack_grow (&exp->stk, p, strlen (p));
      free (p);
    }
}

/* Skip the block. If the exp->cp does not point to the beginning of a
   block ({ character), the function does nothing */
void
_skip_block (struct line_expander *exp)
{
  int level = exp->level;
  if (*exp->cp != '{')
    return;
  for (; *exp->cp; exp->cp++)
    {
      switch (*exp->cp)
	{
	case '{':
	  exp->level++;
	  break;

	case '}':
	  exp->level--;
	  if (exp->level == level)
	    {
	      exp->cp++;
	      return;
	    }
	}
    }
}

/* Expand a block within the formatted line. Stops either when end of source
   line was reached or the nesting reaches the initial value */
void
_expand_block (struct line_expander *exp)
{
  int level = exp->level;
  if (*exp->cp == '{')
    {
      exp->level++;
      exp->cp++;		/*FIXME? */
    }
  while (exp->state == EXP_STATE_CONTINUE)
    {
      for (; *exp->cp && *exp->cp != '%'; exp->cp++)
	{
	  switch (*exp->cp)
	    {
	    case '{':
	      exp->level++;
	      break;

	    case '}':
	      exp->level--;
	      if (exp->level == level)
		{
		  exp->cp++;
		  return;
		}
	      break;

	    case '\\':
	      exp->cp++;
	      break;
	    }
	  obstack_1grow (&exp->stk, *exp->cp);
	}

      if (*exp->cp == 0)
	{
	  obstack_1grow (&exp->stk, 0);
	  exp->state = EXP_STATE_SUCCESS;
	  break;
	}
      else if (*exp->cp == '%' && exp->cp[1] == '%')
	{
	  obstack_1grow (&exp->stk, *exp->cp);
	  exp->cp += 2;
	  continue;
	}

      _expand_cond (exp);
    }
}

/* Expand a format line */
char *
expand_line (const char *line)
{
  char *p = NULL;
  struct line_expander exp;

  exp.state = EXP_STATE_CONTINUE;
  exp.level = 0;
  exp.source = (char *) line;
  exp.cp = (char *) line;
  obstack_init (&exp.stk);
  _expand_block (&exp);
  if (exp.state == EXP_STATE_SUCCESS)
    p = xstrdup (obstack_finish (&exp.stk));
  else
    {
      syslog (LOG_ERR, "can't expand line: %s", line);
      syslog (LOG_ERR, "stopped near %s", exp.cp ? exp.cp : "(END)");
    }
  obstack_free (&exp.stk, NULL);
  return p;
}
