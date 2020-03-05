/* printif.c -- print an interface configuration
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

/* Written by Marcus Brinkmann.  */

#include <config.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include <string.h>

#if STDC_HEADERS
# include <stdlib.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
#endif

#include <alloca.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ifconfig.h"
#include "xalloc.h"
#include <unused-parameter.h>

FILE *ostream;			/* Either stdout or stderror.  */
int column_stdout;		/* The column position of the cursor on stdout.  */
int column_stderr;		/* The column position of the cursor on stderr.  */
int *column = &column_stdout;	/* The column marker of ostream.  */
int had_output;			/* True if we had any output.  */

struct format_handle format_handles[] = {
#ifdef SYSTEM_FORMAT_HANDLER
  SYSTEM_FORMAT_HANDLER
#endif
  {"", fh_nothing},
  {"format?", fh_format_query},
  {"docstr", fh_docstr},
  {"defn", fh_defn},
  {"foreachformat", fh_foreachformat},
  {"verbose?", fh_verbose_query},
  {"newline", fh_newline},
  {"\\n", fh_newline},
  {"\\t", fh_tabulator},
  {"rep", fh_rep},
  {"first?", fh_first},
  {"ifdisplay?", fh_ifdisplay_query},
  {"tab", fh_tab},
  {"join", fh_join},
  {"exists?", fh_exists_query},
  {"format", fh_format},
  {"error", fh_error},
  {"progname", fh_progname},
  {"exit", fh_exit},

  {"name", fh_name},
  {"index?", fh_index_query},
  {"index", fh_index},
  {"addr?", fh_addr_query},
  {"addr", fh_addr},
  {"netmask?", fh_netmask_query},
  {"netmask", fh_netmask},
  {"brdaddr?", fh_brdaddr_query},
  {"brdaddr", fh_brdaddr},
  {"dstaddr?", fh_dstaddr_query},
  {"dstaddr", fh_dstaddr},
  {"flags?", fh_flags_query},
  {"flags", fh_flags},
  {"mtu?", fh_mtu_query},
  {"mtu", fh_mtu},
  {"metric?", fh_metric_query},
  {"metric", fh_metric},
  {"media?", fh_media_query},
  {"media", fh_media},
  {"status?", fh_status_query},
  {"status", fh_status},
#ifdef HAVE_STRUCT_IFREQ_IFR_MAP
  {"map?", fh_map_query},
  {"irq?", fh_irq_query},
  {"irq", fh_irq},
  {"baseaddr?", fh_baseaddr_query},
  {"baseaddr", fh_baseaddr},
  {"memstart?", fh_memstart_query},
  {"memstart", fh_memstart},
  {"memend?", fh_memend_query},
  {"memend", fh_memend},
  {"dma?", fh_dma_query},
  {"dma", fh_dma},
#endif /* HAVE_STRUCT_IFREQ_IFR_MAP */
  {NULL, NULL}
};

/* Various helper functions to get the job done.  */

void
put_char (format_data_t form _GL_UNUSED_PARAMETER, char c)
{
  switch (c)
    {
    case '\n':
      *column = 0;
      break;
    case '\t':
      *column = ((*column / TAB_STOP) + 1) * TAB_STOP;
      break;
    default:
      (*column)++;
    }
  putc (c, ostream);
  had_output = 1;
}

/* This is a simple print function, which tries to keep track of the
   column.  Of course, terminal behaviour can defeat this.  We should
   provide a handler to switch on/off column counting.  */
void
put_string (format_data_t form, const char *s)
{
  while (*s != '\0')
    put_char (form, *(s++));
}

void
put_int (format_data_t form _GL_UNUSED_PARAMETER,
	 int argc, char *argv[], int nr)
{
  char *fmt;
  if (argc > 0)
    {
      char *p = argv[0];

      if (*p != '%')
	fmt = "%i";
      else
	{
	  p++;

	  if (*p == '#')
	    p++;

	  while (isdigit (*p))
	    p++;

	  if ((*p == 'h' || *p == 'H') && p[1])
	    ++p; /* Half length modifier, go to type specifier.  */

	  switch (*p)
	    {
	    default:
	    case 'i':
	    case 'd':
	    case 'D':
	      *p = 'i';
	      break;
	    case 'x':
	    case 'h':
	      *p = 'x';
	      break;
	    case 'X':
	    case 'H':
	      *p = 'X';
	      break;
	    case 'o':
	    case 'O':
	      *p = 'o';
	      break;
	    }
	  p++;
	  *p = '\0';
	  fmt = argv[0];
	}
    }
  else
    fmt = "%i";

  *column += printf (fmt, nr);
  had_output = 1;
}

void
put_ulong (format_data_t form _GL_UNUSED_PARAMETER,
	   int argc, char *argv[], unsigned long value)
{
  char *fmt;
  if (argc > 0)
    {
      char *p = argv[0];

      if (*p != '%')
	fmt = "%lu";
      else
	{
	  p++;

	  while (isdigit (*p))
	    p++;

	  if (*p == '#')
	    p++;

	  if (*p == 'l')
	    p++;

	  switch (*p)
	    {
	    default:
	    case 'i':
	    case 'd':
	    case 'D':
	      *p = 'i';
	      break;
	    case 'x':
	    case 'h':
	      *p = 'x';
	      break;
	    case 'X':
	    case 'H':
	      *p = 'X';
	      break;
	    case 'o':
	    case 'O':
	      *p = 'o';
	      break;
	    }
	  p++;
	  *p = '\0';
	  fmt = argv[0];
	}
    }
  else
    fmt = "%lu";

  *column += printf (fmt, value);
  had_output = 1;
}

void
select_arg (format_data_t form, int argc, char *argv[], int nr)
{
  if (nr < argc)
    {
      form->format = argv[nr];
      print_interfaceX (form, 0);
    }
}

void
put_addr (format_data_t form, int argc, char *argv[], struct sockaddr *sa)
{
  struct sockaddr_in *sin = (struct sockaddr_in *) sa;
  char *addr = inet_ntoa (sin->sin_addr);
  long byte[4];
  char *p = strchr (addr, '.');

  *p = '\0';
  byte[0] = strtol (addr, NULL, 0);
  addr = p + 1;
  p = strchr (addr, '.');
  *p = '\0';
  byte[1] = strtol (addr, NULL, 0);
  addr = p + 1;
  p = strchr (addr, '.');
  *p = '\0';
  byte[2] = strtol (addr, NULL, 0);
  byte[3] = strtol (p + 1, NULL, 0);

  addr = inet_ntoa (sin->sin_addr);

  if (argc > 0)
    {
      long i = strtol (argv[0], NULL, 0);
      if (i >= 0 && i <= 3)
	put_int (form, argc - 1, &argv[1], byte[i]);
    }
  else
    put_string (form, addr);
}

void
put_flags (format_data_t form, int argc, char *argv[], int flags)
{
  unsigned int f = 1;
  const char *name;
  int first = 1;
  unsigned int uflags = (unsigned int) flags;

  while (uflags && f)
    {
      if (f & uflags)
	{
	  name = if_flagtoname (f, NULL);
	  if (name)
	    {
	      if (!first)
		{
		  if (argc > 0)
		    put_string (form, argv[0]);
		  else
		    put_char (form, ' ');
		}
	      put_string (form, name);
	      uflags &= ~f;
	      first = 0;
	    }
	}
      f = f << 1;
    }
  if (uflags)
    {
      if (!first)
	{
	  if (argc > 0)
	    put_string (form, argv[0]);
	  else
	    put_char (form, ' ');
	}
      put_int (form, argc - 1, &argv[1], uflags);
    }
}

void
put_flags_short (format_data_t form, int argc _GL_UNUSED_PARAMETER,
		 char *argv[] _GL_UNUSED_PARAMETER, int flags)
{
  char buf[IF_FORMAT_FLAGS_BUFSIZE];
  if_format_flags (flags, buf, sizeof buf);
  put_string (form, buf);
}

/* Format handler can mangle form->format, so update it after calling
   here.  */
void
format_handler (const char *name, format_data_t form, int argc, char *argv[])
{
  struct format_handle *fh;

  for (fh = format_handles; fh->name; fh++)
    {
      if (!strcmp (fh->name, name))
	{
	  if (fh->handler)
	    (fh->handler) (form, argc, argv);
	  return;
	}
    }

  *column += printf ("(");
  put_string (form, name);
  *column += printf (" unknown)");
  had_output = 1;
}

void
fh_nothing (format_data_t form _GL_UNUSED_PARAMETER,
	    int argc _GL_UNUSED_PARAMETER,
	    char *argv[] _GL_UNUSED_PARAMETER)
{
}

void
fh_format_query (format_data_t form, int argc, char *argv[])
{
  if (argc < 1)
    return;
  select_arg (form, argc, argv, format_find (argv[0]) ? 1 : 2);
}

void
fh_docstr (format_data_t form, int argc, char *argv[])
{
  const char *name;
  struct format *frm;

  name = (argc == 0) ? form->name : argv[0];
  frm = format_find (name);
  if (!frm)
    error (EXIT_FAILURE, errno, "unknown format: `%s'", name);
  put_string (form, frm->docstr);
}

void
fh_defn (format_data_t form, int argc, char *argv[])
{
  const char *name;
  struct format *frm;

  name = (argc == 0) ? form->name : argv[0];

  frm = format_find (name);
  if (!frm)
    error (EXIT_FAILURE, errno, "unknown format: `%s'", name);
  put_string (form, frm->templ);
}

void
fh_foreachformat (format_data_t form, int argc, char *argv[])
{
  struct format *frm;
  const char *save_name;

  if (argc == 0)
    return;

  save_name = form->name;
  for (frm = formats; frm->name; frm++)
    {
      form->name = frm->name;
      form->format = argv[0];
      print_interfaceX (form, 0);
    }
  form->name = save_name;
}

void
fh_newline (format_data_t form, int argc _GL_UNUSED_PARAMETER,
	    char *argv[] _GL_UNUSED_PARAMETER)
{
  put_char (form, '\n');
}

void
fh_tabulator (format_data_t form, int argc _GL_UNUSED_PARAMETER,
	      char *argv[] _GL_UNUSED_PARAMETER)
{
  put_char (form, '\t');
}

void
fh_rep (format_data_t form, int argc, char *argv[])
{
  unsigned int count;
  char *p;

  if (argc < 2)
    return;
  count = strtoul (argv[0], &p, 10);
  if (*p)
    error (EXIT_FAILURE, 0, "invalid repeat count");
  while (count--)
    {
      form->format = argv[1];
      print_interfaceX (form, 0);
    }
}

void
fh_first (format_data_t form, int argc, char *argv[])
{
  select_arg (form, argc, argv, form->first ? 0 : 1);
}

void
fh_ifdisplay_query (format_data_t form, int argc, char *argv[])
{
  int n;

#ifdef SIOCGIFFLAGS
  /* Request for all, or for a specified interface?  */
  n = all_option || ifs_cmdline;
  if (!n)
    {
      /* Otherwise, only interfaces in state `UP' are displayed.  */
      int rev = 0;
      int f = if_nameztoflag ("UP", &rev);

      n = f && ioctl (form->sfd, SIOCGIFFLAGS, form->ifr) == 0;
      if (n) {
	unsigned int uflags = (unsigned short) form->ifr->ifr_flags;

# ifdef ifr_flagshigh
	uflags |= (unsigned short) form->ifr->ifr_flagshigh << 16;
# endif /* ifr_flagshigh */

	n = n && (f & uflags);
      };
    }
#else
  n = 1;	/* Display all of them.  */
#endif
  select_arg (form, argc, argv, !n);
}

void
fh_verbose_query (format_data_t form, int argc, char *argv[])
{
  select_arg (form, argc, argv, verbose ? 0 : 1);
}

/* A tab implementation, which fills with spaces up to requested column or next
   tabstop.  */
void
fh_tab (format_data_t form, int argc, char *argv[])
{
  long goal = 0;

  errno = 0;
  if (argc >= 1)
    goal = strtol (argv[0], NULL, 0);
  if (goal <= 0)
    goal = ((*column / TAB_STOP) + 1) * TAB_STOP;

  while (*column < goal)
    put_char (form, ' ');
}

void
fh_join (format_data_t form, int argc, char *argv[])
{
  int had_output_saved = had_output;
  int count = 0;

  if (argc < 2)
    return;

  /* Suppress delimiter before first argument.  */
  had_output = 0;

  while (++count < argc)
    {
      if (had_output)
	{
	  put_string (form, argv[0]);
	  had_output = 0;
	  had_output_saved = 1;
	}
      form->format = argv[count];
      print_interfaceX (form, 0);
    }
  had_output = had_output_saved;
}

void
fh_exists_query (format_data_t form, int argc, char *argv[])
{
  if (argc > 0)
    {
      struct format_handle *fh;
      int sel = 2; /* assume 2nd arg by default */

      for (fh = format_handles; fh->name; fh++)
	{
	  if (!strcmp (fh->name, argv[0]))
	    {
	      sel = 1; /* select 1st argument */
	      break;
	    }
	}
      select_arg (form, argc, argv, sel);
    }
}

void
fh_format (format_data_t form, int argc, char *argv[])
{
  int i;

  for (i = 0; i < argc; i++)
    {
      struct format *frm = format_find (argv[i]);

      if (frm)
	{
	  /* XXX: Avoid infinite recursion by appending name to a list
	     during the next call (but removing it afterwards, and
	     checking in this function if the name is in the list
	     already.  */
	  form->format = frm->templ;
	  print_interfaceX (form, 0);
	  break;
	}
    }
}

void
fh_error (format_data_t form, int argc, char *argv[])
{
  int i;
  FILE *s = ostream;
  int *c = column;

  ostream = stderr;
  column = &column_stderr;
  for (i = 0; i < argc; i++)
    select_arg (form, argc, argv, i);
  ostream = s;
  column = c;
}

void
fh_progname (format_data_t form, int argc _GL_UNUSED_PARAMETER,
	     char *argv[] _GL_UNUSED_PARAMETER)
{
  put_string (form, program_name);
}

void
fh_exit (format_data_t form _GL_UNUSED_PARAMETER,
	 int argc, char *argv[])
{
  int err = 0;

  if (argc > 0)
    err = strtoul (argv[0], NULL, 0);

  exit (err);
}

void
fh_name (format_data_t form, int argc _GL_UNUSED_PARAMETER,
	 char *argv[] _GL_UNUSED_PARAMETER)
{
  put_string (form, form->name);
}

void
fh_index_query (format_data_t form, int argc, char *argv[])
{
  select_arg (form, argc, argv, (if_nametoindex (form->name) == 0) ? 1 : 0);
}

void
fh_index (format_data_t form, int argc _GL_UNUSED_PARAMETER,
	  char *argv[] _GL_UNUSED_PARAMETER)
{
  int indx = if_nametoindex (form->name);

  if (indx == 0)
    error (EXIT_FAILURE, errno,
	   "No index number found for interface `%s'",
	   form->name);
  *column += printf ("%i", indx);
  had_output = 1;
}

void
fh_addr_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFADDR
  if (ioctl (form->sfd, SIOCGIFADDR, form->ifr) >= 0)
    select_arg (form, argc, argv, 0);
  else
#endif
    select_arg (form, argc, argv, 1);
}

void
fh_addr (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFADDR
  if (ioctl (form->sfd, SIOCGIFADDR, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFADDR failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    put_addr (form, argc, argv, &form->ifr->ifr_addr);
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}

void
fh_netmask_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFNETMASK
  if (ioctl (form->sfd, SIOCGIFNETMASK, form->ifr) >= 0)
    select_arg (form, argc, argv, 0);
  else
#endif
    select_arg (form, argc, argv, 1);
}

void
fh_netmask (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFNETMASK
  if (ioctl (form->sfd, SIOCGIFNETMASK, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFNETMASK failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    put_addr (form, argc, argv, &form->ifr->ifr_netmask);
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}

void
fh_brdaddr_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFBRDADDR
# ifdef SIOCGIFFLAGS
  int f;
  int rev;
  unsigned int uflags = (unsigned short) form->ifr->ifr_flags;

# ifdef ifr_flagshigh
  uflags |= (unsigned short) form->ifr->ifr_flagshigh << 16;
# endif /* ifr_flagshigh */

  if (0 == (f = if_nameztoflag ("BROADCAST", &rev))
      || (ioctl (form->sfd, SIOCGIFFLAGS, form->ifr) < 0)
      || ((f & uflags) == 0))
    {
      select_arg (form, argc, argv, 1);
      return;
    }
# endif
  if (ioctl (form->sfd, SIOCGIFBRDADDR, form->ifr) >= 0)
    select_arg (form, argc, argv, 0);
  else
#endif
    select_arg (form, argc, argv, 1);
}

void
fh_brdaddr (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFBRDADDR
  if (ioctl (form->sfd, SIOCGIFBRDADDR, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFBRDADDR failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    put_addr (form, argc, argv, &form->ifr->ifr_broadaddr);
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}

void
fh_dstaddr_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFDSTADDR
# ifdef SIOCGIFFLAGS
  int f;
  int rev;
  unsigned int uflags = (unsigned short) form->ifr->ifr_flags;

#  ifdef ifr_flagshigh
  uflags |= (unsigned short) form->ifr->ifr_flagshigh << 16;
#  endif /* ifr_flagshigh */

  if (0 == (f = if_nameztoflag ("POINTOPOINT", &rev))
      || (ioctl (form->sfd, SIOCGIFFLAGS, form->ifr) < 0)
      || ((f & uflags) == 0))
    {
      select_arg (form, argc, argv, 1);
      return;
    }
# endif
  if (ioctl (form->sfd, SIOCGIFDSTADDR, form->ifr) >= 0)
    select_arg (form, argc, argv, 0);
  else
#endif
    select_arg (form, argc, argv, 1);
}

void
fh_dstaddr (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFDSTADDR
  if (ioctl (form->sfd, SIOCGIFDSTADDR, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFDSTADDR failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    put_addr (form, argc, argv, &form->ifr->ifr_dstaddr);
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}

void
fh_mtu_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFMTU
  if (ioctl (form->sfd, SIOCGIFMTU, form->ifr) >= 0)
    select_arg (form, argc, argv, 0);
  else
#endif
    select_arg (form, argc, argv, 1);
}

void
fh_mtu (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFMTU
  if (ioctl (form->sfd, SIOCGIFMTU, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFMTU failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    put_int (form, argc, argv, form->ifr->ifr_mtu);
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}

/* The portable behaviour is to display strictly positive
 * metrics, but to supress the default value naught.
 */
void
fh_metric_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFMETRIC
  if (ioctl (form->sfd, SIOCGIFMETRIC, form->ifr) >= 0
      && form->ifr->ifr_metric > 0)
    select_arg (form, argc, argv, 0);
  else
#endif
    select_arg (form, argc, argv, 1);
}

void
fh_metric (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFMETRIC
  if (ioctl (form->sfd, SIOCGIFMETRIC, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFMETRIC failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    put_int (form, argc, argv, form->ifr->ifr_metric);
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}

void
fh_flags_query (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFFLAGS
  if (ioctl (form->sfd, SIOCGIFFLAGS, form->ifr) >= 0)
    select_arg (form, argc, argv, 0);
  else
#endif
    select_arg (form, argc, argv, 1);
}

void
fh_flags (format_data_t form, int argc, char *argv[])
{
#ifdef SIOCGIFFLAGS
  if (ioctl (form->sfd, SIOCGIFFLAGS, form->ifr) < 0)
    error (EXIT_FAILURE, errno,
	   "SIOCGIFFLAGS failed for interface `%s'",
	   form->ifr->ifr_name);
  else
    {
      unsigned int uflags = (unsigned short) form->ifr->ifr_flags;

# ifdef ifr_flagshigh
      uflags |= (unsigned short) form->ifr->ifr_flagshigh << 16;
# endif /* ifr_flagshigh */

      if (argc >= 1)
	{
	  if (!strcmp (argv[0], "number"))
	    put_int (form, argc - 1, &argv[1], uflags);
	  else if (!strcmp (argv[0], "short"))
	    put_flags_short (form, argc - 1, &argv[1], uflags);
	  else if (!strcmp (argv[0], "string"))
	    put_flags (form, argc - 1, &argv[1], uflags);
	}
      else
	put_flags (form, argc, argv, uflags);
    }
#else
  *column += printf ("(not available)");
  had_output = 1;
#endif
}

void
fh_media_query (format_data_t form, int argc, char *argv[])
{
  /* Must be overridden by a system dependent implementation.  */

  /* Claim it to be absent.  */
  select_arg (form, argc, argv, 1);
}

void
fh_media (format_data_t form, int argc _GL_UNUSED_PARAMETER,
	  char *argv[] _GL_UNUSED_PARAMETER)
{
  /* Must be overridden by a system dependent implementation.  */
  put_string (form, "(not known)");
}

void
fh_status_query (format_data_t form, int argc, char *argv[])
{
  /* Must be overridden by a system dependent implementation.  */

  /* Claim it to be absent.  */
  select_arg (form, argc, argv, 1);
}

void
fh_status (format_data_t form, int argc _GL_UNUSED_PARAMETER,
	   char *argv[] _GL_UNUSED_PARAMETER)
{
  /* Must be overridden by a system dependent implementation.  */
  put_string (form, "(not known)");
}

#ifdef HAVE_STRUCT_IFREQ_IFR_MAP

void
fh_map_query (format_data_t form, int argc, char *argv[])
{
# ifdef SIOCGIFMAP
  if (ioctl (form->sfd, SIOCGIFMAP, form->ifr) >= 0)
    select_arg (form, argc, argv, 0);
  else
# endif
    select_arg (form, argc, argv, 1);
}

void
fh_irq_query (format_data_t form, int argc, char *argv[])
{
  if (form->ifr->ifr_map.irq)
    select_arg (form, argc, argv, 0);
  else
    select_arg (form, argc, argv, 1);
}

void
fh_irq (format_data_t form, int argc, char *argv[])
{
  put_int (form, argc, argv, form->ifr->ifr_map.irq);
}

void
fh_baseaddr_query (format_data_t form, int argc, char *argv[])
{
  if (form->ifr->ifr_map.base_addr >= 0x100)
    select_arg (form, argc, argv, 0);
  else
    select_arg (form, argc, argv, 1);
}

void
fh_baseaddr (format_data_t form, int argc, char *argv[])
{
  if (form->ifr->ifr_map.base_addr >= 0x100)
    put_int (form, argc, argv, form->ifr->ifr_map.base_addr);
  else
    put_string (form, "(not available)");
}

void
fh_memstart_query (format_data_t form, int argc, char *argv[])
{
  if (form->ifr->ifr_map.mem_start)
    select_arg (form, argc, argv, 0);
  else
    select_arg (form, argc, argv, 1);
}

void
fh_memstart (format_data_t form, int argc, char *argv[])
{
  if (form->ifr->ifr_map.mem_start)
    put_ulong (form, argc, argv, form->ifr->ifr_map.mem_start);
  else
    put_string (form, "(not available)");
}

void
fh_memend_query (format_data_t form, int argc, char *argv[])
{
  if (form->ifr->ifr_map.mem_end)
    select_arg (form, argc, argv, 0);
  else
    select_arg (form, argc, argv, 1);
}

void
fh_memend (format_data_t form, int argc, char *argv[])
{
  if (form->ifr->ifr_map.mem_end)
    put_ulong (form, argc, argv, form->ifr->ifr_map.mem_end);
  else
    put_string (form, "(not available)");
}

void
fh_dma_query (format_data_t form, int argc, char *argv[])
{
  if (form->ifr->ifr_map.dma)
    select_arg (form, argc, argv, 0);
  else
    select_arg (form, argc, argv, 1);
}

void
fh_dma (format_data_t form, int argc, char *argv[])
{
  if (form->ifr->ifr_map.dma)
    put_int (form, argc, argv, form->ifr->ifr_map.dma);
  else
    put_string (form, "(not available)");
}

#endif /* HAVE_STRUCT_IFREQ_IFR_MAP */

void
print_interfaceX (format_data_t form, int quiet)
{
  const char *p = form->format;
  const char *q;

  form->depth++;

  while (!(*p == '\0' || (form->depth > 1 && *p == '}')))
    {
      /* Echo until end of string or '$'.  */
      while (!(*p == '$' || *p == '\0' || (form->depth > 1 && *p == '}')))
	{
	  quiet || (put_char (form, *p), 0);
	  p++;
	}

      if (*p != '$')
	break;

      /* Look at next character.  If it is a '$' or '}', print that
	 and skip the '$'.  If it is something else than '{', print
	 both.  Otherwise enter substitution mode.  */
      switch (*(++p))
	{
	default:
	  quiet || (put_char (form, '$'), 0);
	  /* Fallthrough. */

	case '$':
	case '}':
	  quiet || (put_char (form, *p), 0);
	  p++;
	  continue;
	  /* Not reached.  */

	case '{':
	  p++;
	  break;
	}

      /* P points to character following '{' now.  */
      q = strchr (p, '}');
      if (!q)
	{
	  /* Without a following '}', no substitution at all can occure,
	     so just dump the string that is missing.  */
	  p -= 2;
	  put_string (form, p);
	  p = strchr (p, '\0');
	  continue;
	}
      else
	{
	  char *id;
	  id = alloca (q - p + 1);
	  memcpy (id, p, q - p);
	  id[q - p] = '\0';
	  p = q + 1;

	  /* We have now in ID the content of the first field, and
	     in P the following string.  Now take the arguments. */
	  if (quiet)
	    {
	      /* Just consume all arguments.  */
	      form->format = p;

	      while (*(form->format) == '{')
		{
		  form->format++;
		  print_interfaceX (form, 1);
		  if (*(form->format) == '}')
		    form->format++;
		}

	      p = form->format;
	    }
	  else
	    {
	      int argc = 0;
	      char **argv;
	      argv = alloca (strlen (q) / 2);

	      while (*p == '{')
		{
		  p++;
		  form->format = p;
		  print_interfaceX (form, 1);
		  q = form->format;
		  argv[argc] = xmalloc (q - p + 1);
		  memcpy (argv[argc], p, q - p);
		  argv[argc][q - p] = '\0';
		  if (*q == '}')
		    q++;
		  p = q;
		  argc++;
		}

	      format_handler (id, form, argc, argv);

	      /* Clean up.  */
	      form->format = p;
	      while (--argc >= 0)
		free (argv[argc]);
	    }
	}
    }

  form->format = p;
  form->depth--;
}

void
print_interface (int sfd, const char *name, struct ifreq *ifr,
		 const char *format)
{
  struct format_data form;
  static int first_passed_already;

  if (!ostream)
    ostream = stdout;

  if (!first_passed_already)
    first_passed_already = form.first = 1;
  else
    form.first = 0;

  form.name = name;
  form.ifr = ifr;
  form.format = format;
  form.sfd = sfd;
  form.depth = 0;

  print_interfaceX (&form, 0);
}
