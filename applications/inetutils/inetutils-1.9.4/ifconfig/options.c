/* options.c -- process the command line options
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

#include <stdio.h>
#include <errno.h>

#if STDC_HEADERS
# include <stdlib.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
#endif

#include <string.h>

#include <sys/socket.h>
#include <net/if.h>

#include <unused-parameter.h>

#include "ifconfig.h"

/* List available interfaces.  */
int list_mode;

/* Be verbose about actions.  */
int verbose;

/* Flags asked for, possibly still pending application.  */
int pending_setflags;
int pending_clrflags;

/* Array of all interfaces on the command line.  */
struct ifconfig *ifs;

/* Size of IFS.  */
int nifs;

static struct ifconfig ifconfig_initializer = {
  NULL,				/* name */
  0,				/* valid */
  NULL, NULL, 0, NULL, NULL, NULL, NULL, 0, 0, 0, 0
};

struct format formats[] = {
  /* This is the default output format if no system_default_format is
     specified.  */
  {"default",
   "Default format.  Equivalent to \"gnu\".",
   "${format}{gnu}"},
  /* This is the standard GNU output.  */
  {"gnu",
   "Standard GNU output format.",
   "${ifdisplay?}{${first?}{}{${\\n}}${format}{gnu-one-entry}}"
  },
  {"gnu-one-entry",
   "Same as GNU, but without additional newlines between the entries.",
   "${format}{check-existence}"
   "${ifdisplay?}{"
   "${name} (${index}):${\\n}"
   "${addr?}{  inet address ${tab}{16}${addr}${\\n}}"
   "${netmask?}{  netmask ${tab}{16}${netmask}${\\n}}"
   "${brdaddr?}{  broadcast ${tab}{16}${brdaddr}${\\n}}"
   "${dstaddr?}{  peer address ${tab}{16}${dstaddr}${\\n}}"
   "${flags?}{  flags ${tab}{16}${flags}${\\n}}"
   "${mtu?}{  mtu ${tab}{16}${mtu}${\\n}}"
   "${metric?}{  metric ${tab}{16}${metric}${\\n}}"
   "${exists?}{hwtype?}{${hwtype?}{  link encap ${tab}{16}${hwtype}${\\n}}}"
   "${exists?}{hwaddr?}{${hwaddr?}{  hardware addr ${tab}{16}${hwaddr}${\\n}}}"
   "${media?}{  link medium ${tab}{16}${media}${\\n}}"
   "${status?}{  link status ${tab}{16}${status}${\\n}}"
   "${exists?}{txqlen?}{${txqlen?}{  tx queue len ${tab}{16}${txqlen}${\\n}}}"
   "}"
  },
  /* Resembles the output of ifconfig 1.39 (1999-03-19) in net-tools 1.52.  */
  {"net-tools",
   "Similar to the output of net-tools.  Default for GNU/Linux.",
   "${format}{check-existence}"
   "${ifdisplay?}{"
   "${name}${exists?}{hwtype?}{${hwtype?}{${tab}{10}Link encap:${hwtype}}"
   "${hwaddr?}{  HWaddr ${hwaddr}}}${\\n}"
   "${addr?}{${tab}{10}inet addr:${addr}"
   "${brdaddr?}{  Bcast:${brdaddr}}"
   "${netmask?}{  Mask:${netmask}}"
   "${newline}}"
   "${tab}{10}${flags}"
   "${mtu?}{  MTU:${mtu}}"
   "${metric?}{  Metric:${metric}}"
   "${media?}{"
   "${newline}${tab}{10}Media: ${media}"
   "${status?}{, ${status}}}"
   "${exists?}{ifstat?}{"
   "${ifstat?}{"
   "${newline}          RX packets:${rxpackets}"
   " errors:${rxerrors} dropped:${rxdropped} overruns:${rxfifoerr}"
   " frame:${rxframeerr}"
   "${newline}          TX packets:${txpackets}"
   " errors:${txerrors} dropped:${txdropped} overruns:${txfifoerr}"
   " carrier:${txcarrier}"
   "${newline}"
   "          collisions:${collisions}"
   "${exists?}{txqlen?}{${txqlen?}{ txqueuelen:${txqlen}}}"
   "${newline}"
   "          RX bytes:${rxbytes}  TX bytes:${txbytes}"
   "}}{"
   "${exists?}{txqlen?}{${txqlen?}{ ${tab}{10}txqueuelen:${txqlen}}${\\n}}}"
   "${newline}"
   "${exists?}{map?}{${map?}{${irq?}{"
   "          Interrupt:${irq}"
   "${baseaddr?}{ Base address:0x${baseaddr}{%x}}"
   "${memstart?}{ Memory:${memstart}{%lx}-${memend}{%lx}}"
   "${dma?}{ DMA chan:${dma}{%x}}"
   "${newline}"
   "}}}"
   "${newline}"
   "}"
  },
  {"netstat",
   "Terse output, similar to that of \"netstat -i\".",
   "${first?}{Iface    MTU Met    RX-OK RX-ERR RX-DRP RX-OVR    TX-OK TX-ERR TX-DRP TX-OVR Flg${newline}}"
   "${format}{check-existence}"
   "${name}${tab}{6}${mtu}{%6d} ${metric}{%3d}"
   /* Insert blanks without ifstat.  */
   "${exists?}{ifstat?}{"
   "${ifstat?}{"
   " ${rxpackets}{%8lu} ${rxerrors}{%6lu} ${rxdropped}{%6lu} ${rxfifoerr}{%6lu}"
   " ${txpackets}{%8lu} ${txerrors}{%6lu} ${txdropped}{%6lu} ${txfifoerr}{%6lu}"
   "}{   - no statistics available -}}"
   "${tab}{76} ${flags?}{${flags}{short}}{[NO FLAGS]}"
   "${newline}"
  },
  /* Resembles the output of ifconfig shipped with unix systems like
     Solaris 2.7 or HPUX 10.20.  */
  {"unix",
   "Traditional UNIX interface listing.  Default for Solaris, BSD and HPUX.",
   "${format}{check-existence}"
   "${ifdisplay?}{"
   "${name}: flags=${flags}{number}{%x}<${flags}{string}{,}>"
   "${metric?}{ metric ${metric}}"
   "${mtu?}{ mtu ${mtu}}${\\n}"
   /* Print only if hwtype emits something.  */
   "${exists?}{hwtype?}{"
     "${hwtype?}{${\\t}${hwtype}${exists?}{hwaddr?}{"
       "${hwaddr?}{ ${hwaddr}}}${\\n}}}"
   "${addr?}{${\\t}inet ${addr}"
   "${dstaddr?}{ --> ${dstaddr}}"
   " netmask ${netmask}{0}{%#02x}${netmask}{1}{%02x}"
   "${netmask}{2}{%02x}${netmask}{3}{%02x}"
   "${brdaddr?}{ broadcast ${brdaddr}}${\\n}}"
   "${media?}{${\\t}media: ${media}${\\n}}"
   "${status?}{${\\t}status: ${status}${\\n}}"
   "}"
  },
  /* Resembles the output of ifconfig shipped with OSF 4.0g.  */
  {"osf",
   "OSF-style output.",
   "${format}{check-existence}"
   "${ifdisplay?}{"
   "${name}: flags=${flags}{number}{%x}<${flags}{string}{,}>${\\n}"
   "${addr?}{${\\t}inet ${addr}"
   " netmask ${netmask}{0}{%02x}${netmask}{1}{%02x}"
   "${netmask}{2}{%02x}${netmask}{3}{%02x}"
   "${brdaddr?}{ broadcast ${brdaddr}}" "${mtu?}{ ipmtu ${mtu}}${\\n}}"
   "}"
  },
  {"check",
   "Shorthand for `check-existence'.",
   "${format}{check-existence}"
  },
  /* If interface does not exist, print error message and exit. */
  {"check-existence",
   "If interface does not exist, print error message and exit.",
   "${index?}{}"
   "{${error}{${progname}: error: interface `${name}' does not exist${\\n}}"
   "${exit}{1}}"},
  {"?",
   "Synonym for `help'.",
   "${format}{help}"
  },
  {"help",
   "Display this help output.",
   "${foreachformat}{"
   "${name}:${tab}{17}${docstr}"
   "${verbose?}{"
   "${newline}"
   "${rep}{79}{-}"
   "${newline}"
   "${defn}"
   "${newline}"
   "}"
   "${newline}"
   "}"
   "${exit}{0}"},
  {NULL, NULL, NULL}
};

/* Default format.  */
const char *default_format;
/* Display all interfaces, even if down */
int all_option;
/* True if interfaces were given in the command line */
int ifs_cmdline;

enum {
  METRIC_OPTION = 256,
  FORMAT_OPTION,
  UP_OPTION,
  DOWN_OPTION,
};

static struct argp_option argp_options[] = {
#define GRP 10
  { "verbose", 'v', NULL, 0,
    "output information when configuring interface", GRP },
  { "all", 'a', NULL, 0,
    "display all available interfaces", GRP },
  { "interface", 'i', "NAME", 0,
    "configure network interface NAME", GRP },
  { "address", 'A', "ADDR", 0,
    "set interface address to ADDR", GRP },
  { "netmask", 'm', "MASK", 0,
    "set netmask to MASK", GRP },
  { "dstaddr", 'd', "ADDR", 0,
    "set destination (peer) address to ADDR", GRP },
  { "peer", 'p', "ADDR", OPTION_ALIAS,
    "synonym for dstaddr", GRP },
  { "broadcast", 'B', "ADDR", 0,
    "set broadcast address to ADDR", GRP },
  { "brdaddr", 'b', NULL, OPTION_ALIAS,
    "synonym for broadcast", GRP }, /* FIXME: Do we really need it? */
  { "mtu", 'M', "N", 0,
    "set mtu of interface to N", GRP },
  { "metric", METRIC_OPTION, "N", 0,
    "set metric of interface to N", GRP },
  { "format", FORMAT_OPTION, "FORMAT", 0,
    "select output format; set to `help' for info", GRP },
  { "up", UP_OPTION, NULL, 0,
    "activate the interface (default if address is given)", GRP },
  { "down", DOWN_OPTION, NULL, 0,
    "shut the interface down", GRP },
  { "flags", 'F', "FLAG[,FLAG...]", 0,
    "set interface flags", GRP },
  { "list", 'l', NULL, 0,
    "list available or selected interfaces", GRP },
  { "short", 's', NULL, 0,
    "short output format", GRP },
#undef GRP
  { NULL, 0, NULL, 0, NULL, 0 }
};

const char doc[] = "Configure network interfaces.";
const char *program_authors[] =
  {
    "Marcus Brinkmann",
    NULL
  };

struct format *
format_find (const char *name)
{
  struct format *frm;

  for (frm = formats; frm->name; frm++)
    {
      if (strcmp (frm->name, name) == 0)
	return frm;
    }
  return NULL;
}

struct ifconfig *
parse_opt_new_ifs (char *name)
{
  struct ifconfig *ifp;

  ifs_cmdline = 1;
  ifs = realloc (ifs, ++nifs * sizeof (struct ifconfig));
  if (!ifs)
    error (EXIT_FAILURE, errno,
	   "can't get memory for interface configuration");
  ifp = &ifs[nifs - 1];
  *ifp = ifconfig_initializer;
  ifp->name = name;
  return ifp;
}

#define PARSE_OPT_SET_ADDR(field, fname, fvalid) \
void								\
parse_opt_set_##field (struct ifconfig *ifp, char *addr)	\
{								\
  if (!ifp)							\
    error (EXIT_FAILURE, 0,                                     \
	   "no interface specified for %s `%s'", #fname, addr); \
  if (ifp->valid & IF_VALID_##fvalid)				\
    error (EXIT_FAILURE, 0,                                     \
	   "only one %s allowed for interface `%s'",		\
	   #fname, ifp->name);				        \
  ifp->field = addr;						\
  ifp->valid |= IF_VALID_##fvalid;				\
}

PARSE_OPT_SET_ADDR (address, address, ADDR)
PARSE_OPT_SET_ADDR (netmask, netmask, NETMASK)
PARSE_OPT_SET_ADDR (dstaddr, destination / peer address, DSTADDR)
PARSE_OPT_SET_ADDR (brdaddr, broadcast address, BRDADDR)

#define PARSE_OPT_SET_INT(field, fname, fvalid) \
void								\
parse_opt_set_##field (struct ifconfig *ifp, char *arg)		\
{								\
  char *end;							\
  if (!ifp)							\
    error (EXIT_FAILURE, 0,                                     \
	   "no interface specified for %s `%s'\n",              \
	   #fname, arg);                                        \
  if (ifp->valid & IF_VALID_##fvalid)				\
    error (EXIT_FAILURE, 0,                                     \
	   "only one %s allowed for interface `%s'",		\
	   #fname, ifp->name);				        \
  ifp->field =  strtol (arg, &end, 0);				\
  if (*arg == '\0' || *end != '\0')				\
    error (EXIT_FAILURE, 0,                                     \
	   "mtu value `%s' for interface `%s' is not a number",	\
	   arg, ifp->name);			                \
  ifp->valid |= IF_VALID_##fvalid;				\
}
PARSE_OPT_SET_INT (mtu, mtu value, MTU)
PARSE_OPT_SET_INT (metric, metric value, METRIC)
void parse_opt_set_af (struct ifconfig *ifp, char *af)
{
  if (!ifp)
    error (EXIT_FAILURE, 0,
	   "no interface specified for address family `%s'", af);

  if (!strcasecmp (af, "inet"))
    ifp->af = AF_INET;
  else
    error (EXIT_FAILURE, 0,
	   "unknown address family `%s' for interface `%s': is not a number",
	   af, ifp->name);
  ifp->valid |= IF_VALID_AF;
}

void
parse_opt_set_flag (struct ifconfig *ifp _GL_UNUSED_PARAMETER,
		    int flag, int rev)
{
  if (rev)
    {
      pending_clrflags |= flag;
      pending_setflags &= ~flag;
    }
  else
    {
      pending_setflags |= flag;
      pending_clrflags &= ~flag;
    }
}

void
parse_opt_flag_list (struct ifconfig *ifp, const char *name)
{
  while (*name)
    {
      int mask, rev;
      char *p = strchr (name, ',');
      size_t len;

      if (p)
	len = p - name;
      else
	len = strlen (name);

      if ((mask = if_nametoflag (name, len, &rev)) == 0)
	error (EXIT_FAILURE, 0, "unknown flag %*.*s",
	       (int) len, (int) len, name);
      parse_opt_set_flag (ifp, mask, rev);

      name += len;
      if (p)
	name++;
    }
}

void
parse_opt_set_point_to_point (struct ifconfig *ifp, char *addr)
{
  parse_opt_set_dstaddr (ifp, addr);
  parse_opt_set_flag (ifp, IFF_POINTOPOINT, 0);
}

void
parse_opt_set_default_format (const char *format)
{
  struct format *frm;

  if (!format)
    format = system_default_format ? system_default_format : "default";

  for (frm = formats; frm->name; frm++)
    if (!strcmp (format, frm->name)) break;
  if (frm == NULL || frm->templ == NULL)
    error (EXIT_FAILURE, 0, "%s: unknown output format", format);
  default_format = frm->templ;
}

static int
is_comment_line (const char *p, size_t len)
{
  while (len--)
    {
      int c = *p++;
      switch (c)
	{
	case ' ':
	case '\t':
	  continue;

	case '#':
	  return 1;

	default:
	  return 0;
	}
    }
  return 0;
}

void
parse_opt_set_default_format_from_file (const char *file)
{
  static struct obstack stk;
  FILE *fp;
  char *buf = NULL;
  size_t size = 0;

  fp = fopen (file, "r");
  if (!fp)
    error (EXIT_FAILURE, errno, "cannot open format file %s", file);

  obstack_init (&stk);
  while (getline (&buf, &size, fp) > 0)
    {
      size_t len = strlen (buf);

      if (len >= 1 && buf[len-1] == '\n')
	len--;

      if (len == 0 || is_comment_line (buf, len))
	continue;
      obstack_grow (&stk, buf, len);
    }
  free (buf);
  fclose (fp);
  obstack_1grow (&stk, 0);
  default_format = obstack_finish (&stk);
}

/* Must be reentrant!  */
void
parse_opt_finalize (struct ifconfig *ifp)
{
  if (ifp && !ifp->valid)
    {
      ifp->valid = IF_VALID_FORMAT;
      ifp->format = default_format;
      ifp->setflags |= pending_setflags;
      ifp->clrflags |= pending_clrflags;
    }
}

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct ifconfig *ifp = *(struct ifconfig **)state->input;

  switch (key)
    {
    case ARGP_KEY_INIT:
      state->child_inputs[0] = state->input;
      break;

    case 'i':		/* Interface name.  */
      parse_opt_finalize (ifp);
      ifp = parse_opt_new_ifs (arg);
      *(struct ifconfig **) state->input = ifp;
      break;

    case 'a':
      all_option = 1;
      break;

    case 'A':		/* Interface address.  */
      parse_opt_set_address (ifp, arg);
      break;

    case 'm':		/* Interface netmask.  */
      parse_opt_set_netmask (ifp, arg);
      break;

    case 'd':		/* Interface dstaddr.  */
    case 'p':
      parse_opt_set_point_to_point (ifp, arg);
      break;

    case 'b':		/* Interface broadcast address.  */
    case 'B':
      parse_opt_set_brdaddr (ifp, arg);
      break;

    case 'F':
      parse_opt_flag_list (ifp, arg);
      break;

    case 'M':		/* Interface MTU.  */
      parse_opt_set_mtu (ifp, arg);
      break;

    case 's':
      parse_opt_set_default_format ("netstat");
      break;

    case 'l':
      list_mode++;
      break;

    case 'v':
      verbose++;
      break;

    case METRIC_OPTION:		/* Interface metric.  */
      parse_opt_set_metric (ifp, arg);
      break;

    case FORMAT_OPTION:		/* Output format.  */
      if (arg && arg[0] == '@')
	parse_opt_set_default_format_from_file (arg + 1);
      else
	parse_opt_set_default_format (arg);
      break;

    case UP_OPTION:
      parse_opt_set_flag (ifp, IFF_UP | IFF_RUNNING, 0);
      break;

    case DOWN_OPTION:
      parse_opt_set_flag (ifp, IFF_UP, 1);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static char *
default_help_filter (int key, const char *text,
		     void *input _GL_UNUSED_PARAMETER)
{
  char *s;

  switch (key)
    {
    default:
      s = (char*) text;
      break;

    case ARGP_KEY_HELP_EXTRA:
      s = if_list_flags ("Known flags are: ");
    }
  return s;
}

static struct argp_child argp_children[2];
static struct argp argp =
  {
    argp_options,
    parse_opt,
    NULL,
    doc,
    NULL,
    default_help_filter,
    NULL
  };

static int
cmp_if_name (const void *a, const void *b)
{
  const struct ifconfig *ifa = a;
  const struct ifconfig *ifb = b;

  return strcmp (ifa->name, ifb->name);
}

void
parse_cmdline (int argc, char *argv[])
{
  int index;
  struct ifconfig *ifp = ifs;

  parse_opt_set_default_format (NULL);
  iu_argp_init ("ifconfig", program_authors);
  argp_children[0] = system_argp_child;
  argp.children = argp_children;
  argp.args_doc = system_help;
  argp_parse (&argp, argc, argv, ARGP_IN_ORDER, &index, &ifp);

  parse_opt_finalize (ifp);

  if (index < argc)
    {
      if (!system_parse_opt_rest (&ifp, argc - index, &argv[index]))
	error (EXIT_FAILURE, 0, "invalid arguments");
      parse_opt_finalize (ifp);
    }
  if (!ifs)
    {
      /* No interfaces specified.  Get a list of all interfaces.  */
      struct if_nameindex *ifnx, *ifnxp;

      ifnx = ifnxp = system_if_nameindex ();
      if (!ifnx)
	error (EXIT_FAILURE, 0, "could not get list of interfaces");
      while (ifnxp->if_index != 0 || ifnxp->if_name != NULL)
	{
	  struct ifconfig *ifp;

	  ifs = realloc (ifs, ++nifs * sizeof (struct ifconfig));
	  if (!ifs)
	    error (EXIT_FAILURE, errno,
		   "can't get memory for interface configuration");
	  ifp = &ifs[nifs - 1];
	  *ifp = ifconfig_initializer;
	  ifp->name = ifnxp->if_name;
	  ifp->valid = IF_VALID_FORMAT;
	  ifp->format = default_format;
	  ifnxp++;
	}
      /* XXX: We never do if_freenameindex (ifnx), because we are
	 keeping the names for later instead using strdup
	 (if->if_name) here.  */

      qsort (ifs, nifs, sizeof (ifs[0]), cmp_if_name);
    }
}
