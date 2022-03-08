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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#ifdef HAVE_SYS_TTY_H
# include <sys/tty.h>
#endif
#ifdef HAVE_SYS_PTYVAR_H
# include <sys/ptyvar.h>
#endif
#if defined HAVE_STROPTS_H && !defined HAVE_IOCTL
# include <stropts.h>
#endif
#include <sys/ioctl.h>

#include <arpa/telnet.h>

#include <libtelnet/auth.h>
#include <libtelnet/encrypt.h>

#include <termios.h>

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>

#include "xalloc.h"

#ifndef HAVE_CC_T
typedef unsigned char cc_t;
#endif

typedef enum debug_mode
{
  debug_options,
  debug_report,
  debug_net_data,
  debug_pty_data,
  debug_auth,
  debug_encr,
  debug_max_mode
} debug_mode_t;

#define MAX_DEBUG_LEVEL 100

extern int debug_level[];

#define DEBUG(mode,level,c) if (debug_level[mode]>=level) c

struct telnetd_clocks
{
  int system;			/* what the current time is */
  int echotoggle;		/* last time user entered echo character */
  int modenegotiated;		/* last time operating mode negotiated */
  int didnetreceive;		/* last time we read data from network */
  int ttypesubopt;		/* ttype subopt is received */
  int tspeedsubopt;		/* tspeed subopt is received */
  int environsubopt;		/* environ subopt is received */
  int oenvironsubopt;		/* old environ subopt is received */
  int xdisplocsubopt;		/* xdisploc subopt is received */
  int baseline;			/* time started to do timed action */
  int gotDM;			/* when did we last see a data mark */
};
#define settimer(x)	(clocks.x = ++clocks.system)
#define sequenceIs(x,y)	(clocks.x < clocks.y)

/*
 * Structures of information for each special character function.
 */
typedef struct
{
  unsigned char flag;		/* the flags for this function */
  cc_t val;			/* the value of the special character */
} slcent, *Slcent;

typedef struct
{
  slcent defset;		/* the default settings */
  slcent current;		/* the current settings */
  cc_t *sptr;			/* a pointer to the char in */
  /* system data structures */
} slcfun, *Slcfun;

#ifdef HAVE_UNAME
   /* Prefix and suffix if the IM string can be generated from uname.  */
# define UNAME_IM_PREFIX "\r\n"
# define UNAME_IM_SUFFIX " (%l) (%t)\r\n\r\n"
#else /* ! HAVE_UNAME */
# define UNAME_IM_PREFIX "\r\n"
# define UNAME_IM_SUFFIX "\r\n"
#endif

/* ********************* */
/* State machine */
/*
 * We keep track of each side of the option negotiation.
 */

#define MY_STATE_WILL		0x01
#define MY_WANT_STATE_WILL	0x02
#define MY_STATE_DO		0x04
#define MY_WANT_STATE_DO	0x08

/*
 * Macros to check the current state of things
 */

#define my_state_is_do(opt)		(options[opt]&MY_STATE_DO)
#define my_state_is_will(opt)		(options[opt]&MY_STATE_WILL)
#define my_want_state_is_do(opt)	(options[opt]&MY_WANT_STATE_DO)
#define my_want_state_is_will(opt)	(options[opt]&MY_WANT_STATE_WILL)

#define my_state_is_dont(opt)		(!my_state_is_do(opt))
#define my_state_is_wont(opt)		(!my_state_is_will(opt))
#define my_want_state_is_dont(opt)	(!my_want_state_is_do(opt))
#define my_want_state_is_wont(opt)	(!my_want_state_is_will(opt))

#define set_my_state_do(opt)		(options[opt] |= MY_STATE_DO)
#define set_my_state_will(opt)		(options[opt] |= MY_STATE_WILL)
#define set_my_want_state_do(opt)	(options[opt] |= MY_WANT_STATE_DO)
#define set_my_want_state_will(opt)	(options[opt] |= MY_WANT_STATE_WILL)

#define set_my_state_dont(opt)		(options[opt] &= ~MY_STATE_DO)
#define set_my_state_wont(opt)		(options[opt] &= ~MY_STATE_WILL)
#define set_my_want_state_dont(opt)	(options[opt] &= ~MY_WANT_STATE_DO)
#define set_my_want_state_wont(opt)	(options[opt] &= ~MY_WANT_STATE_WILL)

/*
 * Tricky code here.  What we want to know is if the MY_STATE_WILL
 * and MY_WANT_STATE_WILL bits have the same value.  Since the two
 * bits are adjacent, a little arithmatic will show that by adding
 * in the lower bit, the upper bit will be set if the two bits were
 * different, and clear if they were the same.
 */
#define my_will_wont_is_changing(opt) \
			((options[opt]+MY_STATE_WILL) & MY_WANT_STATE_WILL)

#define my_do_dont_is_changing(opt) \
			((options[opt]+MY_STATE_DO) & MY_WANT_STATE_DO)

/*
 * Make everything symetrical
 */

#define HIS_STATE_WILL			MY_STATE_DO
#define HIS_WANT_STATE_WILL		MY_WANT_STATE_DO
#define HIS_STATE_DO			MY_STATE_WILL
#define HIS_WANT_STATE_DO		MY_WANT_STATE_WILL

#define his_state_is_do			my_state_is_will
#define his_state_is_will		my_state_is_do
#define his_want_state_is_do		my_want_state_is_will
#define his_want_state_is_will		my_want_state_is_do

#define his_state_is_dont		my_state_is_wont
#define his_state_is_wont		my_state_is_dont
#define his_want_state_is_dont		my_want_state_is_wont
#define his_want_state_is_wont		my_want_state_is_dont

#define set_his_state_do		set_my_state_will
#define set_his_state_will		set_my_state_do
#define set_his_want_state_do		set_my_want_state_will
#define set_his_want_state_will		set_my_want_state_do

#define set_his_state_dont		set_my_state_wont
#define set_his_state_wont		set_my_state_dont
#define set_his_want_state_dont		set_my_want_state_wont
#define set_his_want_state_wont		set_my_want_state_dont

#define his_will_wont_is_changing	my_do_dont_is_changing
#define his_do_dont_is_changing		my_will_wont_is_changing

/* ******* */

void fatal (int f, char *msg);

/*
 * Linemode support states, in decreasing order of importance
 */
#define REAL_LINEMODE	0x04
#define KLUDGE_OK	0x03
#define NO_AUTOKLUDGE	0x02
#define KLUDGE_LINEMODE	0x01
#define NO_LINEMODE	0x00

#include <arpa/telnet.h>

#define NETSLOP 64

#define ttloop(c) while (c) io_drain ()

/* External variables */
extern char options[256];
extern char do_dont_resp[256];
extern char will_wont_resp[256];
extern int linemode;		/* linemode on/off */
extern int uselinemode;		/* what linemode to use (on/off) */
extern int editmode;		/* edit modes in use */
extern int useeditmode;		/* edit modes to use */
extern int alwayslinemode;	/* command line option */
extern int lmodetype;		/* Client support for linemode */
extern int flowmode;		/* current flow control state */
extern int restartany;		/* restart output on any character state */
extern int diagnostic;		/* telnet diagnostic capabilities */
#if defined AUTHENTICATION
extern int auth_level;
extern int autologin;
#endif

extern slcfun slctab[NSLC + 1];	/* slc mapping table */

extern char *terminaltype;
extern char *remote_hostname;
extern char *local_hostname;
extern char *login_invocation;
extern char *user_name;

extern int pty, net;
extern int SYNCHing;		/* we are in TELNET SYNCH mode */
extern struct telnetd_clocks clocks;
extern char line[];

extern char *xstrdup (const char *);
extern int argcv_get (const char *command, const char *delim,
		      int *argc, char ***argv);

void io_setup (void);
int net_has_data (void);
int net_get_char (int peek);
void set_neturg (void);
int net_output_data (const char *format, ...);
int net_output_datalen (const void *buf, size_t l);
int net_buffer_level (void);
void io_drain (void);

int stilloob (int s);
void ptyflush (void);
char *nextitem (char *current);
void netclear (void);
void netflush (void);

int pty_buffer_is_full (void);
void pty_output_byte (int c);
void pty_output_datalen (const void *data, size_t len);
int pty_buffer_level ();

/* Debugging functions */
extern void printoption (char *, int);
extern void printdata (char *, char *, int);
extern void printsub (int, unsigned char *, int);
extern void debug_output_datalen (const char *data, size_t len);
extern void debug_output_data (const char *fmt, ...);

/* TTY functions */
extern void init_termbuf (void);
extern void set_termbuf (void);
extern int spcset (int func, cc_t * valp, cc_t ** valpp);
extern void term_send_eof (void);
extern int term_change_eof (void);

extern void tty_binaryin (int);
extern void tty_binaryout (int);
extern int tty_flowmode (void);
extern int tty_restartany (void);
extern int tty_isbinaryin (void);
extern int tty_isbinaryout (void);
extern int tty_iscrnl (void);
extern int tty_isecho (void);
extern int tty_isediting (void);
extern int tty_islitecho (void);
extern int tty_isnewmap (void);
extern int tty_israw (void);
extern int tty_issofttab (void);
extern int tty_istrapsig (void);
extern int tty_linemode (void);
extern void tty_rspeed (int);
extern void tty_setecho (int);
extern void tty_setedit (int);
extern void tty_setlinemode (int);
extern void tty_setlitecho (int);
extern void tty_setsig (int);
extern void tty_setsofttab (int);
extern void tty_tspeed (int);

extern char *expand_line (const char *fmt);

/*  FIXME */
extern void _termstat (void);
extern void add_slc (char func, char flag, cc_t val);
extern void check_slc (void);
extern void change_slc (char func, char flag, cc_t val);

extern void cleanup (int);
extern void clientstat (int, int, int);
extern void copy_termbuf ();
extern void deferslc (void);
extern void defer_terminit (void);
extern void do_opt_slc (unsigned char *, int);
extern void dooption (int);
extern void dontoption (int);
extern void edithost (char *, char *);
extern void fatal (int, char *);
extern void fatalperror (int, char *);
extern void get_slc_defaults (void);
extern void localstat (void);
extern void flowstat (void);
extern void netclear (void);

extern void send_do (int, int);
extern void send_dont (int, int);
extern void send_slc (void);
extern void send_status (void);
extern void send_will (int, int);
extern void send_wont (int, int);
extern void set_termbuf (void);
extern void start_login (char *, int, char *);
extern void start_slc (int);
extern void start_slave (char *, int, char *);

extern void suboption (void);
extern void telrcv (void);

extern int end_slc (unsigned char **);
extern int spcset (int, cc_t *, cc_t **);
extern int stilloob (int);
extern int terminit (void);
extern int termstat (void);
extern void willoption (int);
extern void wontoption (int);

#ifdef	ENCRYPTION
extern void (*encrypt_output) (unsigned char *, int);
extern int (*decrypt_input) (int);
#endif /* ENCRYPTION */

extern int startslave (char *host, int autologin, char *autoname);
extern int getterminaltype (char *user_name, size_t len);
extern int net_input_level (void);
extern int net_output_level (void);
extern int net_read (void);
extern int net_buffer_is_full (void);
extern void net_output_byte (int c);
extern int pty_input_level (void);
extern int pty_read (void);
extern int pty_output_level (void);
extern int pty_get_char (int peek);
extern int pty_input_putback (const char *str, size_t len);

extern int terminaltypeok (char *s);
