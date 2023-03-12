/*
  Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
  2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013,
  2014, 2015 Free Software Foundation, Inc.

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

/*
 * Copyright (c) 1985, 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Grammar for FTP commands:
 *
 *   See RFC 959, RFC 1639, RFC 2389, RFC 2428,
 *   and RFC 3659 (MDTM, REST, SIZE).
 *
 * Security related details:
 *
 *   PORT, EPRT, and LPRT are only allowed for
 *   target ports greater than IPPORT_RESERVED.
 *   In addition, the network address of the data
 *   connection must be identical to the address
 *   of the standing control connection.  These
 *   have bearing on RFC 2577, sections 3 and 4.

 * TODO: Update with RFC 3659 (MLST, MLSD).
 *
 * TODO: RFC 2428 (EPSV ALL).
 *
 * FIXME: Rewrite with GNU standard formatting.  Legacy code is changed!
 */

%{

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/ftp.h>

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>	/* strtoimax */
#elif defined HAVE_STDINT_H
# include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/utsname.h>
/* Include glob.h last, because it may define "const" which breaks
   system headers on some platforms. */
#include <glob.h>

#include "extern.h"

#if !defined NBBY && defined CHAR_BIT
#define NBBY CHAR_BIT
#endif

off_t restart_point;

static char cbuf[512];           /* Command Buffer.  */
static char *fromname;
static int cmd_type;
static int cmd_form;
static int cmd_bytesz;

struct tab
{
  const char	*name;
  short	token;
  short	state;
  short	implemented;	/* 1 if command is implemented */
  const char	*help;
};

static struct tab cmdtab[];
static struct tab sitetab[];
static char *extlist[];
static char *copy         (char *);
static void help          (struct tab *, char *);
static struct tab *lookup (struct tab *, char *);
static void sizecmd       (char *);
static int yylex          (void);
static void yyerror       (const char *s);

%}

%union {
	intmax_t i;
	char   *s;
}

%token
	A	B	C	E	F	I
	L	N	P	R	S	T

	SP	CRLF	COMMA

	USER	PASS	ACCT	REIN	QUIT	PORT
	PASV	TYPE	STRU	MODE	RETR	STOR
	APPE	MLFL	MAIL	MSND	MSOM	MSAM
	MRSQ	MRCP	ALLO	REST	RNFR	RNTO
	ABOR	DELE	CWD	LIST	NLST	SITE
	STAT	HELP	NOOP	MKD	RMD	PWD
	CDUP	STOU	SMNT	SYST	SIZE	MDTM

	FEAT	OPTS

	EPRT	EPSV	LPRT	LPSV

	ADAT	AUTH	CCC	CONF	ENC	MIC
	PBSZ	PROT

	UMASK	IDLE	CHMOD

	LEXERR

%token	<s> STRING
%token	<i> NUMBER CHAR

%type	<i> check_login octal_number byte_size
%type	<i> struct_code mode_code type_code form_code
%type	<s> pathstring pathname password username
%type	<i> host_port net_proto tcp_port long_host_port
%type	<s> net_addr

%start	cmd_list

%%

cmd_list
	: /* empty */
	| cmd_list cmd
		{
			free (fromname);
			fromname = (char *) 0;
			restart_point = (off_t) 0;
		}
	| cmd_list rcmd
	;

cmd
	: USER SP username CRLF
		{
			user ($3);
			free ($3);
		}
	| PASS SP password CRLF
		{
			pass ($3);
			memset ($3, 0, strlen ($3));
			free ($3);
		}
	| PORT check_login SP host_port CRLF
		{
			if ($2)
			  {
			    if ($4
				&& ((his_addr.ss_family == AF_INET
				     && memcmp (&((struct sockaddr_in *) &his_addr)->sin_addr,
						&((struct sockaddr_in *) &data_dest)->sin_addr,
						sizeof (struct in_addr))
					== 0
				     && ntohs (((struct sockaddr_in *) &data_dest)->sin_port)
					> IPPORT_RESERVED)
				    ||
				    (his_addr.ss_family == AF_INET6
				     && memcmp (&((struct sockaddr_in6 *) &his_addr)->sin6_addr,
						&((struct sockaddr_in6 *) &data_dest)->sin6_addr,
						sizeof (struct in6_addr))
					== 0
				     && ntohs (((struct sockaddr_in6 *) &data_dest)->sin6_port)
					> IPPORT_RESERVED)
				   )
			       )
			      {
				usedefault = 0;
				if (pdata >= 0)
				  {
				    close (pdata);
				    pdata = -1;
				  }
				reply (200, "PORT command successful.");
			      }
			    else
			      {
				usedefault = 1;
				memset (&data_dest, 0, sizeof (data_dest));
				reply (500, "Illegal PORT Command");
			      }
			  }
		}
	| PASV check_login CRLF
		{
			if ($2)
			  passive (PASSIVE_PASV, AF_INET);
		}
	| TYPE SP type_code CRLF
		{
			switch (cmd_type)
			  {
			  case TYPE_A:
			    if (cmd_form == FORM_N)
			      {
				reply (200, "Type set to A.");
				type = cmd_type;
				form = cmd_form;
			      }
			    else
			      reply (504, "Form must be N.");
			    break;

			  case TYPE_E:
			    reply (504, "Type E not implemented.");
			    break;

			  case TYPE_I:
			    reply (200, "Type set to I.");
			    type = cmd_type;
			    break;

			  case TYPE_L:
#if defined NBBY && NBBY == 8
			    if (cmd_bytesz == 8)
			      {
				reply (200, "Type set to L (byte size 8).");
				type = cmd_type;
			      }
			    else
			      reply (504, "Byte size must be 8.");
#else /* NBBY == 8 */
			  UNIMPLEMENTED for NBBY != 8
#endif /* NBBY == 8 */
			  }
		}
	| STRU SP struct_code CRLF
		{
			switch ($3)
			  {
			  case STRU_F:
			    reply (200, "STRU F ok.");
			    break;

			  default:
			    reply (504, "Unimplemented STRU type.");
			  }
		}
	| MODE SP mode_code CRLF
		{
			switch ($3)
			  {
			  case MODE_S:
			    reply (200, "MODE S ok.");
			    break;

			  default:
			    reply (502, "Unimplemented MODE type.");
			  }
		}
	| ALLO SP NUMBER CRLF
		{
			reply (202, "ALLO command ignored.");
		}
	| ALLO SP NUMBER SP R SP NUMBER CRLF
		{
			reply (202, "ALLO command ignored.");
		}
	| RETR check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  retrieve ((char *) 0, $4);
			free ($4);
		}
	| STOR check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  store ($4, "w", 0);
			free ($4);
		}
	| APPE check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  store ($4, "a", 0);
			free ($4);
		}
	| NLST check_login CRLF
		{
			if ($2)
			  send_file_list (".");
		}
	| NLST check_login SP STRING CRLF
		{
			if ($2 && $4 != NULL)
			  send_file_list ($4);
			free ($4);
		}
	| LIST check_login CRLF
		{
			if ($2)
			  retrieve ("/bin/ls -lgA", "");
		}
	| LIST check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  retrieve ("/bin/ls -lgA %s", $4);
			free ($4);
		}
	| STAT check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  statfilecmd ($4);
			free ($4);
		}
	| STAT CRLF
		{
			statcmd ();
		}
	| DELE check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  delete ($4);
			free ($4);
		}
	| RNTO check_login SP pathname CRLF
		{
			if ($2)
			  {
			    if (fromname)
			      {
				renamecmd (fromname, $4);
				free (fromname);
				fromname = (char *) 0;
			      }
			    else
			      reply (503, "Bad sequence of commands.");
			  }
			free ($4);
		}
	| ABOR CRLF
		{
			reply (225, "ABOR command successful.");
		}
	| CWD check_login CRLF
		{
			if ($2)
			  cwd (cred.homedir);
		}
	| CWD check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  cwd ($4);
			free ($4);
		}
	| HELP CRLF
		{
			help (cmdtab, (char *) 0);
		}
	| HELP SP STRING CRLF
		{
			char *cp = $3;

			if (strncasecmp (cp, "SITE", 4) == 0)
			  {
			    cp = $3 + 4;
			    if (*cp == ' ')
			      cp++;
			    if (*cp)
			      help (sitetab, cp);
			    else
			      help (sitetab, (char *) 0);
			  }
			else
			  help (cmdtab, $3);
			free ($3);
		}
	| NOOP CRLF
		{
			reply (200, "NOOP command successful.");
		}
	| MKD check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  makedir ($4);
			free ($4);
		}
	| RMD check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  removedir ($4);
			free ($4);
		}
	| PWD check_login CRLF
		{
			if ($2)
			  pwd ();
		}
	| CDUP check_login CRLF
		{
			if ($2)
			  cwd ("..");
		}
	| FEAT check_login CRLF
		{
			if ($2)
			  {
			    char **name;

			    lreply (211, "Supported extensions:");
			    for (name = extlist; *name; name++)
			      printf (" %s\r\n", *name);
			    reply (211, "End");
			  }
		}
	/* Catch all variants of the above FEAT, and reject them.  */
	| FEAT check_login SP STRING CRLF
		{
			if ($2)
			  {
			    reply (501, "Not accepting arguments.");
			    free ($4);
			  }
		}
	/* Changable behaviour is yet to be implemented,
	 * so OPTS is a no-op for the time being.  It is
	 * mandatory by RFC 2389, since FEAT now exists.
	 */
	| OPTS check_login CRLF
		{
			if ($2)
			  {
			    reply (501, "Must have an argument.");
			  }
		}
	| OPTS check_login SP STRING CRLF
		{
			if ($2)
			  {
			    reply (501, "No options are available.");
			    free ($4);
			  }
		}
	| SITE SP HELP CRLF
		{
			help (sitetab, (char *) 0);
		}
	| SITE SP HELP SP STRING CRLF
		{
			help (sitetab, $5);
			free ($5);
		}
	| SITE SP UMASK check_login CRLF
		{
			int oldmask;

			if ($4)
			  {
			    oldmask = umask (0);
			    umask (oldmask);
			    reply (200, "Current UMASK is %03o", oldmask);
			  }
		}
	| SITE SP UMASK check_login SP octal_number CRLF
		{
			int oldmask;

			if ($4)
			  {
			    if (($6 == -1) || ($6 > 0777))
			      reply (501, "Bad UMASK value");
			    else
			      {
				oldmask = umask ($6);
				reply (200, "UMASK set to %03o (was %03o)",
				      $6, oldmask);
			      }
			  }
		}
	| SITE SP CHMOD check_login SP octal_number SP pathname CRLF
		{
			if ($4 && ($8 != NULL))
			  {
			    if ($6 > 0777)
			      reply (501,
				     "CHMOD: Mode value must be between 0 and 0777");
			    else if (chmod ($8, $6) < 0)
			      perror_reply (550, $8);
			    else
			      reply (200, "CHMOD command successful.");
			  }
			free ($8);
		}
	| SITE SP IDLE CRLF
		{
			reply (200,
			       "Current IDLE time limit is %d seconds; max %d",
			       timeout, maxtimeout);
		}
	| SITE SP IDLE check_login SP NUMBER CRLF
		{
			if ($4)
			  {
			    if ($6 < 30 || $6 > maxtimeout)
			      reply (501,
				     "Maximum IDLE time must be between 30 and %d seconds",
				     maxtimeout);
			    else
			      {
				timeout = $6;
				alarm ((unsigned) timeout);
				reply (200,
				       "Maximum IDLE time set to %d seconds",
				       timeout);
			      }
			  }
		}
	| STOU check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  store ($4, "w", 1);
			free ($4);
		}
	| SYST CRLF
		{
		        const char *sys_type; /* Official rfc-defined os type.  */
			char *version = 0; /* A more specific type. */

#ifdef HAVE_UNAME
			struct utsname u;

			if (uname (&u) >= 0)
			  {
			    version = malloc (strlen (u.sysname) + 1
					      + strlen (u.release) + 1);
			    if (version)
			      sprintf (version, "%s %s", u.sysname, u.release);
			  }
#else /* !HAVE_UNAME */
# ifdef BSD
			version = "BSD";
# endif /* BSD */
#endif /* !HAVE_UNAME */

#if defined unix || defined __unix || defined __unix__
			sys_type = "UNIX";
#else
			sys_type = "UNKNOWN";
#endif

			if (!no_version && version)
			  reply (215, "%s Type: L%d Version: %s",
				 sys_type, NBBY, version);
			else
			  reply (215, "%s Type: L%d", sys_type, NBBY);

#ifdef HAVE_UNAME
			free (version);
#endif
		}

		/*
		 * SIZE is in RFC 3659.
		 *
		 * Return size of file in a format suitable for
		 * using with RESTART (we just count bytes).
		 */
	| SIZE check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  sizecmd ($4);
			free ($4);
		}

		/*
		 * MDTM is in RFC 3659.
		 *
		 * Return modification time of file as an ISO 3307
		 * style time. E.g. YYYYMMDDHHMMSS or YYYYMMDDHHMMSS.xxx
		 * where xxx is the fractional second (of any precision,
		 * not necessarily 3 digits)
		 */
	| MDTM check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
			  {
			    struct stat stbuf;

			    if (stat ($4, &stbuf) < 0)
			      reply (550, "%s: %s", $4, strerror (errno));
			    else if (!S_ISREG (stbuf.st_mode))
			      reply (550, "%s: not a plain file.", $4);
			    else
			      {
				struct tm *t;

				t = gmtime (&stbuf.st_mtime);
				reply (213,
				       "%04d%02d%02d%02d%02d%02d",
				       1900 + t->tm_year, t->tm_mon+1,
				       t->tm_mday, t->tm_hour,
				       t->tm_min, t->tm_sec);
			      }
			  }
			free ($4);
		}

		/*
		 * EPRT is in RFC 2428.
		 */
	| EPRT check_login SP CHAR net_proto CHAR net_addr CHAR tcp_port CHAR CRLF
		{
			usedefault = 0;
			if (pdata >= 0)
			  {
			    close (pdata);
			    pdata = -1;
			  }
			/* A first sanity check.  */
			if ($2				/* valid login */
			    && ($5 > 0)			/* valid protocols */
			    && ($4 > 32 && $4 < 127)	/* legal first delimiter */
							/* identical delimiters */
			    && ($4 == $6 && $4 == $8 && $4 == $10))
			  {
			    /* We only accept connections using
			     * the same address family as is
			     * currently in use, unless we
			     * detect IPv4-mapped-to-IPv6.
			     */
			    if (his_addr.ss_family == $5
				|| ($5 == AF_INET6
				    && his_addr.ss_family == AF_INET)
				|| ($5 == AF_INET
				    && his_addr.ss_family == AF_INET6))
			      {
				int err;
				char p[8];
				struct addrinfo hints, *res;

				memset (&hints, 0, sizeof (hints));
				snprintf (p, sizeof (p), "%jd", $9 & 0xffff);
				hints.ai_family = $5;
				hints.ai_socktype = SOCK_STREAM;
				hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

				err = getaddrinfo ($7, p, &hints, &res);
				if (err)
				  reply (500, "Illegal EPRT Command");
				else if (/* sanity check */
					 (his_addr.ss_family == AF_INET
					  && memcmp (&((struct sockaddr_in *) &his_addr)->sin_addr,
						     &((struct sockaddr_in *) res->ai_addr)->sin_addr,
						     sizeof (struct in_addr))
					     == 0
					  && ntohs (((struct sockaddr_in *) res->ai_addr)->sin_port)
					     > IPPORT_RESERVED
					 )
					 ||
					 (his_addr.ss_family == AF_INET6
					  && memcmp (&((struct sockaddr_in6 *) &his_addr)->sin6_addr,
						     &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr,
						     sizeof (struct in6_addr))
					     == 0
					  && ntohs (((struct sockaddr_in6 *) res->ai_addr)->sin6_port)
					     > IPPORT_RESERVED
					 )
					 ||
					 (his_addr.ss_family == AF_INET
					  && res->ai_family == AF_INET6
					  && IN6_IS_ADDR_V4MAPPED (&((struct sockaddr_in6 *) res->ai_addr)->sin6_addr)
					  && memcmp (&((struct sockaddr_in *) &his_addr)->sin_addr,
						     &((struct in_addr *) &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr)[3],
						     sizeof (struct in_addr))
					     == 0
					  && ntohs (((struct sockaddr_in6 *) res->ai_addr)->sin6_port)
					     > IPPORT_RESERVED
					 )
					 ||
					 (his_addr.ss_family == AF_INET6
					  && res->ai_family == AF_INET
					  && IN6_IS_ADDR_V4MAPPED (&((struct sockaddr_in6 *) &his_addr)->sin6_addr)
					  && memcmp (&((struct in_addr *) &((struct sockaddr_in6 *) &his_addr)->sin6_addr)[3],
						     &((struct sockaddr_in *) res->ai_addr)->sin_addr,
						     sizeof (struct in_addr))
					     == 0
					  && ntohs (((struct sockaddr_in *) res->ai_addr)->sin_port)
					     > IPPORT_RESERVED
					 )
					)
				  {
				    /* In the case of IPv4 mapped as IPv6,
				     * the addresses were proven to coincide,
				     * only the extraction remains.
				     * Since non-mapped is the standard,
				     * test that situation first.
				     */
				    if (his_addr.ss_family == res->ai_family)
				      {
					memcpy (&data_dest, res->ai_addr,
						res->ai_addrlen);
					data_dest_len = res->ai_addrlen;
				      }
				    else if (his_addr.ss_family == AF_INET
					     && res->ai_family == AF_INET6)
				      {
					/* `his_addr' contains the reduced
					 * IPv4 address.
					 */
					memcpy (&data_dest, &his_addr,
						sizeof (struct sockaddr_in));
					data_dest_len =
					  sizeof (struct sockaddr_in);
					((struct sockaddr_in *) &data_dest)->sin_port =
					  ((struct sockaddr_in6 *) res->ai_addr)->sin6_port;
				      }
				    else
				      {
					/* `res->ai_addr' contains the reduced
					 * IPv4 address, but the connection
					 * stands on `his_addr', which is
					 * an IPv4-to-IPv6-mapped address.
					 */
					memcpy (&data_dest, &his_addr,
						sizeof (struct sockaddr_in6));
					data_dest_len =
					  sizeof (struct sockaddr_in6);
					((struct sockaddr_in6 *) &data_dest)->sin6_port =
					  ((struct sockaddr_in *) res->ai_addr)->sin_port;
				      }

				    freeaddrinfo (res);
				    reply (200, "EPRT command successful.");
				  }
				else
				  {
				    /* failed identity check */
				    if (res)
				      freeaddrinfo (res);
				    reply (500, "Illegal EPRT Command");
				  }
			      }
			    else
			      /* Not fit for established connection.  */
			      reply (522,
				     "Network protocol not supported, use (%d)",
				     ($5 == 1) ? 2 : 1);
			  }
			else if ($2 && ($5 <= 0))
			    reply (522,
				   "Network protocol not supported, use (1,2)");
			else if ($2)
			  /* Incorrect delimiters detected,
			   * the other conditions are met.
			   */
			  reply (500, "Illegal EPRT Command");
		}

		/*
		 * EPSV is in RFC 2428.
		 */
	| EPSV check_login CRLF
		{
			if ($2)
			  passive (PASSIVE_EPSV, AF_UNSPEC);
		}
	| EPSV check_login SP net_proto CRLF
		{
			if ($2)
			  {
			    if ($4 > 0)
			      passive (PASSIVE_EPSV, $4);
			    else
			      reply (522,
				     "Network protocol not supported, use (1,2)");
			  }
		}

		/*
		 * LPRT is in RFC 1639.
		 */
	| LPRT check_login SP long_host_port CRLF
		{
			if ($2)
			  {
			    if ($4 &&
				((his_addr.ss_family == AF_INET
				  && memcmp (&((struct sockaddr_in *) &his_addr)->sin_addr,
					     &((struct sockaddr_in *) &data_dest)->sin_addr,
					     sizeof (struct in_addr)) == 0
				  && ntohs (((struct sockaddr_in *) &data_dest)->sin_port)
					> IPPORT_RESERVED)
				 ||
				 (his_addr.ss_family == AF_INET6
				  && memcmp (&((struct sockaddr_in6 *) &his_addr)->sin6_addr,
					     &((struct sockaddr_in6 *) &data_dest)->sin6_addr,
					     sizeof (struct in6_addr)) == 0
				  && ntohs (((struct sockaddr_in6 *) &data_dest)->sin6_port)
					> IPPORT_RESERVED)
				)
			       )
			      {
				usedefault = 0;
				if (pdata >= 0)
				  {
				    close (pdata);
				    pdata = -1;
				  }
				  reply (200, "LPRT command successful.");
			      }
			    else
			      {
				usedefault = 1;
				memset (&data_dest, 0, sizeof (data_dest));
				reply (500, "Illegal LPRT Command");
			      }
			  } /* check_login */
		}

		/*
		 * LPSV is in RFC 1639.
		 */
	| LPSV check_login CRLF
		{
			if ($2)
			  passive (PASSIVE_LPSV, 0 /* not used */);
		}

	| QUIT CRLF
		{
			reply (221, "Goodbye.");
			dologout (0);
		}
	| error CRLF
		{
			yyerrok;
		}
	;
rcmd
	: RNFR check_login SP pathname CRLF
		{
			restart_point = (off_t) 0;
			if ($2 && $4)
			  {
			    free (fromname);
			    fromname = renamefrom ($4);
			  }
			if (fromname == (char *) 0 && $4)
			  free ($4);
		}

		/*
		 * REST is in RFC 3659.
		 */
	| REST SP byte_size CRLF
		{
		        free (fromname);
			fromname = (char *) 0;
			restart_point = $3;
			reply (350, "Restarting at %jd. %s",
			       (intmax_t) restart_point,
			       "Send STORE or RETRIEVE to initiate transfer.");
		}
	;

username
	: STRING
	;

password
	: /* empty */
		{
			$$ = (char *) calloc (1, sizeof (char));
		}
	| STRING
	;

byte_size
	: NUMBER
	;

net_proto
	: NUMBER
		{
			/* Rewrite as valid address family.  */
			if ($1 == 1)
			  $$ = AF_INET;
			else if ($1 == 2)
			  $$ = AF_INET6;
			else
			  $$ = -1;	/* Invalid protocol.  */
		}
	;

tcp_port
	: NUMBER
	;

net_addr
	: STRING
	;

host_port
	: NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER
		{
			int err;
			char a[INET6_ADDRSTRLEN], p[8];
			struct addrinfo hints, *res;

			snprintf (a, sizeof (a), "%jd.%jd.%jd.%jd",
				  $1 & 0xff, $3 & 0xff,
				  $5 & 0xff, $7 & 0xff);
			snprintf (p, sizeof (p), "%jd",
				  (($9 & 0xff) << 8) + ($11 & 0xff));
			memset (&hints, 0, sizeof (hints));
			hints.ai_family = his_addr.ss_family;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

			if (his_addr.ss_family == AF_INET6)
			  {
			    /* IPv4 mapped to IPv6.  */
			    hints.ai_family = AF_INET6;
#ifdef AI_V4MAPPED
			    hints.ai_flags |= AI_V4MAPPED;
#endif
			    snprintf (a, sizeof (a),
				      "::ffff:%jd.%jd.%jd.%jd",
				      $1 & 0xff, $3 & 0xff,
				      $5 & 0xff, $7 & 0xff);
			  }

			err = getaddrinfo (a, p, &hints, &res);
			if (err)
			  {
			    reply (550, "Address failure: %s,%s", a, p);
			    memset (&data_dest, 0, sizeof (data_dest));
			    data_dest_len = 0;
			    $$ = 0;
			  }
			else
			  {
			    memcpy (&data_dest, res->ai_addr, res->ai_addrlen);
			    data_dest_len = res->ai_addrlen;
			    freeaddrinfo (res);
			    $$ = 1;
			  }
		}
	;

long_host_port
	: NUMBER COMMA NUMBER COMMA /* af, hal */
		NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA /* h */
		NUMBER COMMA NUMBER COMMA NUMBER /* pal, p */
		{
			int err;
			char a[INET6_ADDRSTRLEN], p[8];
			struct addrinfo hints, *res;

			/* Well formed input for IPv4?  */
			if ($1 != 4 || $3 != 4 || $13 != 2
			    || $5 < 0 || $5 > 255 || $7 < 0 || $7 > 255
			    || $9 < 0 || $9 > 255 || $11 < 0 || $11 > 255
			    || $15 < 0 || $15 > 255
			    || $17 < 0 || $17 > 255)
			  {
			    reply (500, "Invalid address.");
			    memset (&data_dest, 0, sizeof (data_dest));
			    data_dest_len = 0;
			    $$ = 0;
			  }
			else
			  {
			    snprintf (a, sizeof (a), "%jd.%jd.%jd.%jd",
				      $5, $7, $9, $11);
			    snprintf (p, sizeof (p), "%jd", ($15 << 8) + $17);

			    memset (&hints, 0, sizeof (hints));
			    hints.ai_family = his_addr.ss_family;
			    hints.ai_socktype = SOCK_STREAM;
			    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

			    if (his_addr.ss_family == AF_INET6)
			      {
				/* IPv4 mapped to IPv6.  */
				hints.ai_family = AF_INET6;
#ifdef AI_V4MAPPED
				hints.ai_flags |= AI_V4MAPPED;
#endif
				snprintf (a, sizeof (a),
					  "::ffff:%jd.%jd.%jd.%jd",
					  $5, $7, $9, $11);
			      }

			    err = getaddrinfo (a, p, &hints, &res);
			    if (err)
			      {
				reply (550, "LPRT address failure: %s,%s",
				       a, p);
				memset (&data_dest, 0, sizeof (data_dest));
				data_dest_len = 0;
				$$ = 0;
			      }
			    else
			      {
				memcpy (&data_dest, res->ai_addr,
					res->ai_addrlen);
				data_dest_len = res->ai_addrlen;
				freeaddrinfo (res);
				$$ = 1;
			      }
			  }
		}
	| NUMBER COMMA NUMBER COMMA /* af, hal */
		NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA /* h */
		NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER COMMA NUMBER /* pal, p */
		{
			int err;
			char a[INET6_ADDRSTRLEN], p[8];
			struct addrinfo hints, *res;

			/* Well formed input for IPv6?  */
			if ($1 != 6 || $3 != 16 || $37 != 2
			    || $5 < 0 || $5 > 255 || $7 < 0 || $7 > 255
			    || $9 < 0 || $9 > 255 || $11 < 0 || $11 > 255
			    || $13 < 0 || $13 > 255 || $15 < 0 || $15 > 255
			    || $17 < 0 || $17 > 255 || $19 < 0 || $19 > 255
			    || $21 < 0 || $21 > 255 || $23 < 0 || $23 > 255
			    || $25 < 0 || $25 > 255 || $27 < 0 || $27 > 255
			    || $29 < 0 || $29 > 255 || $31 < 0 || $31 > 255
			    || $33 < 0 || $33 > 255 || $35 < 0 || $35 > 255
			    || $39 < 0 || $39 > 255 || $41 < 0 || $41 > 255)
			  {
			    reply (500, "Invalid address.");
			    memset (&data_dest, 0, sizeof (data_dest));
			    data_dest_len = 0;
			    $$ = 0;
			  }
			else
			  {
			    snprintf (a, sizeof (a),
				     "%02jx%02jx:%02jx%02jx:"
				     "%02jx%02jx:%02jx%02jx:"
				     "%02jx%02jx:%02jx%02jx:"
				     "%02jx%02jx:%02jx%02jx",
				      $5, $7, $9, $11,
				      $13, $15, $17, $19,
				      $21, $23, $25, $27,
				      $29, $31, $33, $35);
			    snprintf (p, sizeof (p), "%jd",
				      ($39 << 8) + $41);

			    memset (&hints, 0, sizeof (hints));
			    hints.ai_family = his_addr.ss_family;
			    hints.ai_socktype = SOCK_STREAM;
			    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

			    err = getaddrinfo (a, p, &hints, &res);
			    if (err)
			      {
				reply (550, "LPRT address failure: %s,%s",
				       a, p);
				memset (&data_dest, 0, sizeof (data_dest));
				data_dest_len = 0;
				$$ = 0;
			      }
			    else
			      {
				memcpy (&data_dest, res->ai_addr,
					res->ai_addrlen);
				data_dest_len = res->ai_addrlen;
				freeaddrinfo (res);
				$$ = 1;
			      }
			  }
		}
	;

form_code
	: N
		{
			$$ = FORM_N;
		}
	| T
		{
			$$ = FORM_T;
		}
	| C
		{
			$$ = FORM_C;
		}
	;

type_code
	: A
		{
			cmd_type = TYPE_A;
			cmd_form = FORM_N;
		}
	| A SP form_code
		{
			cmd_type = TYPE_A;
			cmd_form = $3;
		}
	| E
		{
			cmd_type = TYPE_E;
			cmd_form = FORM_N;
		}
	| E SP form_code
		{
			cmd_type = TYPE_E;
			cmd_form = $3;
		}
	| I
		{
			cmd_type = TYPE_I;
		}
	| L
		{
			cmd_type = TYPE_L;
			cmd_bytesz = NBBY;
		}
	| L SP byte_size
		{
			cmd_type = TYPE_L;
			cmd_bytesz = $3;
		}
		/* this is for a bug in the BBN ftp */
	| L byte_size
		{
			cmd_type = TYPE_L;
			cmd_bytesz = $2;
		}
	;

struct_code
	: F
		{
			$$ = STRU_F;
		}
	| R
		{
			$$ = STRU_R;
		}
	| P
		{
			$$ = STRU_P;
		}
	;

mode_code
	: S
		{
			$$ = MODE_S;
		}
	| B
		{
			$$ = MODE_B;
		}
	| C
		{
			$$ = MODE_C;
		}
	;

pathname
	: pathstring
		{
			/*
			 * Problem: this production is used for all pathname
			 * processing, but only gives a 550 error reply.
			 * This is a valid reply in some cases but not in others.
			 */
			if (cred.logged_in && $1 && *$1 == '~')
			  {
			    glob_t gl;
			    int flags = GLOB_NOCHECK;

#ifdef GLOB_BRACE
			    flags |= GLOB_BRACE;
#endif
#ifdef GLOB_QUOTE
			    flags |= GLOB_QUOTE;
#endif
#ifdef GLOB_TILDE
			    flags |= GLOB_TILDE;
#endif

			    memset (&gl, 0, sizeof (gl));
			    if (glob ($1, flags, NULL, &gl)
				|| gl.gl_pathc == 0)
			      {
				reply (550, "not found");
				$$ = NULL;
			      }
			    else
			      $$ = strdup (gl.gl_pathv[0]);

			    globfree (&gl);
			    free ($1);
			  }
			else
			  $$ = $1;
		}
	;

pathstring
	: STRING
	;

octal_number
	: NUMBER
		{
			int ret, dec, multby, digit;

			/*
			 * Convert a number that was read as decimal number
			 * to what it would be if it had been read as octal.
			 */
			dec = $1;
			multby = 1;
			ret = 0;
			while (dec)
			  {
			    digit = dec % 10;
			    if (digit > 7)
			      {
				ret = -1;
				break;
			      }
			    ret += digit * multby;
			    multby *= 8;
			    dec /= 10;
			  }
			$$ = ret;
		}
	;

check_login
	: /* empty */
		{
			if (cred.logged_in)
			  $$ = 1;
			else
			  {
			    reply (530, "Please login with USER and PASS.");
			    $$ = 0;
			  }
		}
	;

%%

#define	CMD	0	/* beginning of command */
#define	ARGS	1	/* expect miscellaneous arguments */
#define	STR1	2	/* expect SP followed by STRING */
#define	STR2	3	/* expect STRING (must be STR2 + 1)*/
#define	OSTR	4	/* optional SP then STRING */
#define	ZSTR1	5	/* SP then optional STRING */
#define	ZSTR2	6	/* optional STRING after SP (must be ZSTR1 + 1) */
#define	SITECMD	7	/* SITE command */
#define	NSTR	8	/* Number followed by a string */
#define	DLIST	9	/* SP and delimited list for EPRT/EPSV */

static struct tab cmdtab[] = {
  /* In the order defined by RFC 959.  See also RFC 1123.  */
  /* Access control commands.  */
  { "USER", USER, STR1, 1,	"<sp> username" },
  { "PASS", PASS, ZSTR1, 1,	"<sp> password" },
  { "ACCT", ACCT, STR1, 0,	"(specify account)" },
  { "CWD",  CWD,  OSTR, 1,	"[ <sp> directory-name ]" },
  { "CDUP", CDUP, ARGS, 1,	"(change to parent directory)" },
  { "SMNT", SMNT, ARGS, 0,	"(structure mount)" },
  { "REIN", REIN, ARGS, 0,	"(reinitialize server state)" },
  { "QUIT", QUIT, ARGS, 1,	"(terminate service)", },
  /* Transfer parameter commands.  */
  { "PORT", PORT, ARGS, 1,	"<sp> b0, b1, b2, b3, b4" },
  { "PASV", PASV, ARGS, 1,	"(set server in passive mode)" },
  { "TYPE", TYPE, ARGS, 1,	"<sp> [ A | E | I | L ]" },
  { "STRU", STRU, ARGS, 1,	"(specify file structure)" },
  { "MODE", MODE, ARGS, 1,	"(specify transfer mode)" },
  /* FTP service commands.  */
  { "RETR", RETR, STR1, 1,	"<sp> file-name" },
  { "STOR", STOR, STR1, 1,	"<sp> file-name" },
  { "STOU", STOU, STR1, 1,	"<sp> file-name" },
  { "APPE", APPE, STR1, 1,	"<sp> file-name" },
  { "ALLO", ALLO, ARGS, 1,	"allocate storage (vacuously)" },
  { "REST", REST, ARGS, 1,	"<sp> offset (restart command)" },
  { "RNFR", RNFR, STR1, 1,	"<sp> file-name" },
  { "RNTO", RNTO, STR1, 1,	"<sp> file-name" },
  { "ABOR", ABOR, ARGS, 1,	"(abort operation)" },
  { "DELE", DELE, STR1, 1,	"<sp> file-name" },
  { "RMD",  RMD,  STR1, 1,	"<sp> path-name" },
  { "MKD",  MKD,  STR1, 1,	"<sp> path-name" },
  { "PWD",  PWD,  ARGS, 1,	"(return current directory)" },
  { "LIST", LIST, OSTR, 1,	"[ <sp> path-name ]" },
  { "NLST", NLST, OSTR, 1,	"[ <sp> path-name ]" },
  { "SITE", SITE, SITECMD, 1,	"site-cmd [ <sp> arguments ]" },
  { "SYST", SYST, ARGS, 1,	"(get type of operating system)" },
  { "STAT", STAT, OSTR, 1,	"[ <sp> path-name ]" },
  { "HELP", HELP, OSTR, 1,	"[ <sp> <string> ]" },
  { "NOOP", NOOP, ARGS, 1,	"" },
  /* Experimental commands, as mentioned in RFC 1123.  Now obsolete.  */
  { "XMKD", MKD,  STR1, 1,	"<sp> path-name" },
  { "XRMD", RMD,  STR1, 1,	"<sp> path-name" },
  { "XPWD", PWD,  ARGS, 1,	"(return current directory)" },
  { "XCUP", CDUP, ARGS, 1,	"(change to parent directory)" },
  { "XCWD", CWD,  OSTR, 1,	"[ <sp> directory-name ]" },
  /* Commands in RFC 2389.  */
  { "FEAT", FEAT, OSTR, 1,	"(display command extensions)" },
  /* XXX: Replace OSTR once some functionality exists.  */
  { "OPTS", OPTS, OSTR, 1,	"<sp> cmd-name [ <sp> options ]" },
  /* Commands in RFC 3659.  */
  { "SIZE", SIZE, OSTR, 1,	"<sp> path-name" },
  { "MDTM", MDTM, OSTR, 1,	"<sp> path-name" },
  /* Unimplemented, but reserved in RFC ???.  */
  { "MLFL", MLFL, OSTR, 0,	"(mail file)" },
  { "MAIL", MAIL, OSTR, 0,	"(mail to user)" },
  { "MSND", MSND, OSTR, 0,	"(mail send to terminal)" },
  { "MSOM", MSOM, OSTR, 0,	"(mail send to terminal or mailbox)" },
  { "MSAM", MSAM, OSTR, 0,	"(mail send to terminal and mailbox)" },
  { "MRSQ", MRSQ, OSTR, 0,	"(mail recipient scheme question)" },
  { "MRCP", MRCP, STR1, 0,	"(mail recipient)" },
  /* Extended addressing in RFC 2428.  */
  { "EPRT", EPRT, DLIST, 1,	"<sp> <d> proto <d> addr <d> port <d>" },
  { "EPSV", EPSV, ARGS, 1,	"[ <sp> af ]" },
  /* Long addressing in RFC 1639.  Obsoleted in RFC 5797.  */
  { "LPRT", LPRT, ARGS, 1,	"<sp> af,hal,h0..hn,2,p0,p1" },
  { "LPSV", LPSV, ARGS, 1,	"(set server in long passive mode)" },
  /* Security extensions in RFC 2228.  */
  { "ADAT", ADAT, OSTR, 0,	"<sp> security-data" },
  { "AUTH", AUTH, OSTR, 0,	"<sp> mechanism" },
  { "CCC", CCC, ARGS, 0,	"(clear command channel)" },
  { "CONF", CONF, OSTR, 0,	"<sp> confidential-msg" },
  { "ENC", ENC, OSTR, 0,	"<sp> private-message" },
  { "MIC", MIC, OSTR, 0,	"<sp> safe-message" },
  { "PBSZ", PBSZ, OSTR, 0,	"<sp> buf-size" },
  { "PROT", PROT, OSTR, 0,	"<sp> char" },
  /* End of list.  */
  { NULL,   0,    0,    0,	NULL }
};

static struct tab sitetab[] = {
  { "CHMOD", CHMOD, NSTR, 1,	"<sp> mode <sp> file-name" },
  { "HELP", HELP, OSTR, 1,	"[ <sp> <string> ]" },
  { "IDLE", IDLE, ARGS, 1,	"[ <sp> maximum-idle-time ]" },
  { "UMASK", UMASK, ARGS, 1,	"[ <sp> umask ]" },
  { NULL,   0,    0,    0,	NULL }
};

/* Extensions beyond RFC 959 and RFC 2389.  Ordered as implemented.  */
static char *extlist[] = {
  "MDTM", "SIZE", "REST STREAM",
  "EPRT", "EPSV", "LPRT", "LPSV",
  NULL };

static struct tab *
lookup (struct tab *p, char *cmd)
{
  for (; p->name != NULL; p++)
    if (strcmp (cmd, p->name) == 0)
      return (p);
  return (0);
}

#include <arpa/telnet.h>

/*
 * getline - a hacked up version of fgets to ignore TELNET escape codes.
 */
char *
telnet_fgets (char *s, int n, FILE *iop)
{
  int c;
  register char *cs;

  cs = s;
/* tmpline may contain saved command from urgent mode interruption */
  for (c = 0; tmpline[c] != '\0' && --n > 0; ++c)
    {
      *cs++ = tmpline[c];
      if (tmpline[c] == '\n')
	{
	  *cs++ = '\0';
	  if (debug)
	    syslog (LOG_DEBUG, "command: %s", s);
	  tmpline[0] = '\0';
	  return (s);
	}

      if (c == 0)
	tmpline[0] = '\0';
    }

  while ((c = getc (iop)) != EOF)
    {
      c &= 0377;
      if (c == IAC)
	{
	  c = getc (iop);
	  if (c != EOF)
	    {
	      c &= 0377;
	      switch (c)
		{
		case WILL:
		case WONT:
		  c = getc (iop);
		  printf ("%c%c%c", IAC, DONT, 0377 & c);
		  fflush (stdout);
		  continue;

		case DO:
		case DONT:
		  c = getc (iop);
		  printf ("%c%c%c", IAC, WONT, 0377 & c);
		  fflush (stdout);
		  continue;

		case IAC:
		  break;

		default:
		  continue;	/* ignore command */
		}
	    }
	}

      *cs++ = c;
      if (--n <= 0 || c == '\n')
	break;
    }

  if (c == EOF && cs == s)
    return (NULL);

  *cs++ = '\0';

  if (debug)
    {
      if (!cred.guest && strncasecmp ("pass ", s, 5) == 0)
	{
	  /* Don't syslog passwords.  */
	  syslog (LOG_DEBUG, "command: %.5s ???", s);
	}
      else
	{
	  register char *cp;
	  register int len;

	  /* Don't syslog trailing CR-LF.  */
	  len = strlen (s);
	  cp = s + len - 1;

	  while (cp >= s && (*cp == '\n' || *cp == '\r'))
	    {
	      --cp;
	      --len;
	    }

	  syslog (LOG_DEBUG, "command: %.*s", len, s);
	}
    }
  return (s);
}

void
toolong (int signo)
{
  (void) signo;
  reply (421, "Timeout (%d seconds): closing control connection.",
	 timeout);
  if (logging)
    syslog (LOG_INFO, "User %s timed out after %d seconds",
	    (cred.name ? cred.name : "unknown"), timeout);
  dologout (1);
}

static int
yylex (void)
{
  static int cpos, state;
  char *cp, *cp2;
  struct tab *p;
  int n;
  char c;

  for (;;)
    {
      switch (state)
	{
	case CMD:
	  signal (SIGALRM, toolong);
	  alarm ((unsigned) timeout);
	  if (telnet_fgets (cbuf, sizeof (cbuf)-1, stdin) == NULL)
	    {
	      reply (221, "You could at least say goodbye.");
	      dologout (0);
	    }
	  alarm (0);

#ifdef HAVE_SETPROCTITLE
	  if (strncasecmp (cbuf, "PASS", 4) != 0)
	    setproctitle ("%s: %s", proctitle, cbuf);
#endif /* HAVE_SETPROCTITLE */

	  cp = strchr (cbuf, '\r');
	  if (cp)
	    {
	      *cp++ = '\n';
	      *cp = '\0';
	    }

	  cp = strpbrk (cbuf, " \n");
	  if (cp)
	    cpos = cp - cbuf;

	  if (cpos == 0)
	    cpos = 4;

	  c = cbuf[cpos];
	  cbuf[cpos] = '\0';
	  upper (cbuf);
	  p = lookup (cmdtab, cbuf);
	  cbuf[cpos] = c;

	  if (p != 0)
	    {
	      if (p->implemented == 0)
		{
		  nack (p->name);
		  longjmp (errcatch, 0);
		  /* NOTREACHED */
		}
	      state = p->state;
	      yylval.s = (char*) p->name;
	      return (p->token);
	    }
	  break;	/* Command not known.  */

	case SITECMD:
	  if (cbuf[cpos] == ' ')
	    {
	      cpos++;
	      return (SP);
	    }
	  cp = &cbuf[cpos];

	  cp2 = strpbrk (cp, " \n");
	  if (cp2)
	    cpos = cp2 - cbuf;

	  c = cbuf[cpos];
	  cbuf[cpos] = '\0';
	  upper (cp);
	  p = lookup (sitetab, cp);
	  cbuf[cpos] = c;

	  if (p != 0)
	    {
	      if (p->implemented == 0)
		{
		  state = CMD;
		  nack (p->name);
		  longjmp (errcatch, 0);
		  /* NOTREACHED */
		}

	      state = p->state;
	      yylval.s = (char*) p->name;
	      return (p->token);
	    }
	  state = CMD;
	  break;	/* Command not known.  */

	case OSTR:
	  if (cbuf[cpos] == '\n')
	    {
	      state = CMD;
	      return (CRLF);
	    }

	case STR1:
	case ZSTR1:
	dostr1:
	  if (cbuf[cpos] == ' ')
	    {
	      cpos++;
	      if (state == OSTR)
		state = STR2;
	      else
		++state;

	      return (SP);
	    }
	  /* Intentional continuation.  */

	case ZSTR2:
	  if (cbuf[cpos] == '\n')
	    {
	      state = CMD;
	      return (CRLF);
	    }

	case STR2:
	  cp = &cbuf[cpos];
	  n = strlen (cp);
	  cpos += n - 1;
	  /*
	   * Make sure the string is nonempty and newline terminated.
	   */
	  if (n > 1 && cbuf[cpos] == '\n')
	    {
	      cbuf[cpos] = '\0';
	      yylval.s = copy (cp);
	      cbuf[cpos] = '\n';
	      state = ARGS;
	      return (STRING);
	    }
	  break;	/* Empty string, missing NL.  */

	case NSTR:
	  if (cbuf[cpos] == ' ')
	    {
	      cpos++;
	      return (SP);
	    }
	  if (isdigit (cbuf[cpos]))
	    {
	      cp = &cbuf[cpos];
	      while (isdigit (cbuf[++cpos]))
		;

	      c = cbuf[cpos];
	      cbuf[cpos] = '\0';
	      yylval.i = atoi (cp);
	      cbuf[cpos] = c;
	      state = STR1;
	      return (NUMBER);
	    }
	  state = STR1;
	  goto dostr1;

	case DLIST:
	  /* Either numerical strings or
	   * address strings for IPv4 and IPv6.
	   * The consist of hexadecimal chars,
	   * colon and periods.  A period can
	   * not begin a valid address.  */
	  if (isxdigit (cbuf[cpos]) || cbuf[cpos] == ':')
	    {
	      int is_num = 1;	/* Only to turn off.  */

	      cp = &cbuf[cpos];
	      while (isxdigit (cbuf[cpos])
		     || cbuf[cpos] == ':'
		     || cbuf[cpos] == '.')
		{
		  if (!isdigit (cbuf[cpos]))
		    is_num = 0;
		  cpos++;
		}

	      c = cbuf[cpos];
	      cbuf[cpos] = '\0';
	      if (is_num)
		{
		  yylval.i = atoi (cp);
		  cbuf[cpos] = c;
		  return (NUMBER);
		}
	      else
		{
		  yylval.s = copy (cp);
		  cbuf[cpos] = c;
		  return (STRING);
		}
	    }

	  c = cbuf[cpos++];
	  switch (c)
	    {
	    case ' ':
	      return (SP);

	    case '\n':
	      state = CMD;
	      return (CRLF);

	    default:
	      yylval.i = c;
	      return (CHAR);
	    }
	  break;	/* Not reachable.  */

	case ARGS:
	  if (isdigit (cbuf[cpos]))
	    {
	      cp = &cbuf[cpos];
	      while (isdigit (cbuf[++cpos]))
		;

	      c = cbuf[cpos];
	      cbuf[cpos] = '\0';
	      yylval.i = strtoimax (cp, NULL, 10);	/* off_t */
	      cbuf[cpos] = c;
	      return (NUMBER);
	    }

	  switch (cbuf[cpos++])
	    {
	    case '\n':
	      state = CMD;
	      return (CRLF);

	    case ' ':
	      return (SP);

	    case ',':
	      return (COMMA);

	    case 'A':
	    case 'a':
	      return (A);

	    case 'B':
	    case 'b':
	      return (B);

	    case 'C':
	    case 'c':
	      return (C);

	    case 'E':
	    case 'e':
	      return (E);

	    case 'F':
	    case 'f':
	      return (F);

	    case 'I':
	    case 'i':
	      return (I);

	    case 'L':
	    case 'l':
	      return (L);

	    case 'N':
	    case 'n':
	      return (N);

	    case 'P':
	    case 'p':
	      return (P);

	    case 'R':
	    case 'r':
	      return (R);

	    case 'S':
	    case 's':
	      return (S);

	    case 'T':
	    case 't':
	      return (T);
	    }
	  break;	/* No number, not in [\n ,aAbBcCeEfFiIlLnNpPrRsSttT] */

	default:
	  fatal ("Unknown state in scanner.");
	}

      /*
       * Analysis: Cases when this point is reached.
       *
       *  CMD:      command not known
       *  SITECMD:  site command not known (state changed to CMD)
       *
       *  OSTR, STR1, ZSTR1, STR2, ZSTR2, NSTR:
       *            empty string or string without NL
       *
       *  ARGS:     not a number, not a special character
       */

      /*
       * Issue a new error message only if the parser has not
       * yet reported a complaint.  Without this precaution
       * two messages would be directed to the client, thus
       * upsetting all following exchange.
       */
      if (!yynerrs)
	yyerror ("command not recognized");

      state = CMD;
      longjmp (errcatch, 0);
    } /* for (;;) */
}

void
upper (char *s)
{
  while (*s != '\0')
    {
      if (islower (*s))
	*s = toupper (*s);
      s++;
    }
}

static char *
copy (char *s)
{
  char *p;

  p = malloc (strlen (s) + 1);
  if (p == NULL)
    fatal ("Ran out of memory.");

  strcpy (p, s);
  return (p);
}

static void
help (struct tab *ctab, char *s)
{
  struct tab *c;
  int width, NCMDS;
  const char *help_type;

  if (ctab == sitetab)
    help_type = "SITE ";
  else
    help_type = "";

  width = 0, NCMDS = 0;
  for (c = ctab; c->name != NULL; c++)
    {
      int len = strlen (c->name);

      if (len > width)
	width = len;

      NCMDS++;
    }

  width = (width + 8) &~ 7;

  if (s == 0)
    {
      int i, j, w;
      int columns, lines;

      lreply (214, "The following %scommands are recognized %s.",
	      help_type, "(* =>'s unimplemented)");

      columns = 76 / width;
      if (columns == 0)
	columns = 1;

      lines = (NCMDS + columns - 1) / columns;

      for (i = 0; i < lines; i++)
	{
	  printf ("   ");
	  for (j = 0; j < columns; j++)
	    {
	      c = ctab + j * lines + i;
	      printf ("%s%c", c->name, c->implemented ? ' ' : '*');

	      if (c + lines >= &ctab[NCMDS])
		break;

	      w = strlen (c->name) + 1;
	      while (w < width)
		{
		  putchar (' ');
		  w++;
		}
	    }
	  printf ("\r\n");
	}
      fflush (stdout);
      reply (214, "Direct comments to ftp-bugs@%s.", hostname);
      return;
    }

  upper (s);

  c = lookup (ctab, s);
  if (c == (struct tab *) 0)
    {
      reply (502, "Unknown command %s.", s);
      return;
    }

  if (c->implemented)
    reply (214, "Syntax: %s%s %s", help_type, c->name, c->help);
  else
    reply (214, "%s%-*s\t%s; unimplemented.", help_type,
	   width, c->name, c->help);
}

static void
sizecmd (char *filename)
{
  switch (type)
    {
    case TYPE_L:
    case TYPE_I:
      {
	struct stat stbuf;

	if (stat (filename, &stbuf) < 0 || !S_ISREG (stbuf.st_mode))
	  reply (550, "%s: not a plain file.", filename);
	else
	  reply (213, "%ju", (uintmax_t) stbuf.st_size);
	break;
      }

    case TYPE_A:
      {
	FILE *fin;
	int c;
	off_t count;
	struct stat stbuf;

	fin = fopen (filename, "r");
	if (fin == NULL)
	  {
	    perror_reply (550, filename);
	    return;
	  }

	if (fstat (fileno (fin), &stbuf) < 0 || !S_ISREG (stbuf.st_mode))
	  {
	    reply (550, "%s: not a plain file.", filename);
	    fclose (fin);
	    return;
	  }

	count = 0;
	while ((c = getc (fin)) != EOF)
	  {
	    if (c == '\n')	/* will get expanded to \r\n */
	      count++;
	    count++;
	  }
	fclose (fin);

	reply (213, "%jd", (intmax_t) count);
	break;
      }

    default:
      reply (504, "SIZE not implemented for Type %c.", "?AEIL"[type]);
    }
}

static void
yyerror (const char *s)
{
  char *cp;

  cp = strchr (cbuf, '\n');
  if (cp != NULL)
    *cp = '\0';

  reply (500, "'%s': %s", cbuf, (s ? s : "command not understood."));
}
