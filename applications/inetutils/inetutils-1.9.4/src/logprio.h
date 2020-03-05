/*
  Copyright (C) 2011, 2012, 2013, 2014, 2015 Free Software Foundation, Inc.

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
  along with this program.  If not, see `http://www.gnu.org/licenses/'.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 * 4. Neither the name of the University nor the names of its contributors
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
  Separate tables of facility names and priority names.
  These are not always present in the headers of known
  library implementations.  They are brought in as needed.
 */

#ifndef _IU_LOGPRIO_H
#  define _IU_LOGPRIO_H	1

typedef struct _code {
	char	*c_name;
	int	c_val;
} CODE;

/* Insert any missing macros.  */

#ifndef LOG_PRI
#  define LOG_PRI(p)	((p) & LOG_PRIMASK)
#endif

#ifndef LOG_MAKEPRI
#  define LOG_MAKEPRI(fac, p)	((fac) | (p))
#endif

#ifndef INTERNAL_NOPRI
#  define INTERNAL_NOPRI 0x10	/* the "no priority" priority */
#endif

#ifndef INTERNAL_MARK	/* mark "facility", first unused value */
#  define INTERNAL_MARK	LOG_MAKEPRI(LOG_NFACILITIES << 3, 0)
#endif

#ifndef LOG_FACMASK
#  define LOG_FACMASK	0x03f8
#endif

#ifndef LOG_FAC
#  define LOG_FAC(p)	(((p) & LOG_FACMASK) >> 3)
#endif

/* OpenSolaris needs the next two.  */
#ifndef LOG_FTP
#  define LOG_FTP	(11 << 3)	/* A guess */
#endif

#ifndef LOG_AUTHPRIV
#  define LOG_AUTHPRIV	LOG_AUTH
#endif

CODE prioritynames[] =
  {
    { "alert", LOG_ALERT },
    { "crit", LOG_CRIT },
    { "debug", LOG_DEBUG },
    { "emerg", LOG_EMERG },
    { "err", LOG_ERR },
    { "error", LOG_ERR },		/* DEPRECATED */
    { "info", LOG_INFO },
    { "none", INTERNAL_NOPRI },		/* INTERNAL */
    { "notice", LOG_NOTICE },
    { "panic", LOG_EMERG },		/* DEPRECATED */
    { "warn", LOG_WARNING },		/* DEPRECATED */
    { "warning", LOG_WARNING },
    { NULL, -1 }
  };

CODE facilitynames[] =
  {
    { "auth", LOG_AUTH },
    { "authpriv", LOG_AUTHPRIV },
    { "cron", LOG_CRON },
    { "daemon", LOG_DAEMON },
    { "ftp", LOG_FTP },
    { "kern", LOG_KERN },
    { "lpr", LOG_LPR },
    { "mail", LOG_MAIL },
    { "mark", INTERNAL_MARK },		/* INTERNAL */
    { "news", LOG_NEWS },
    { "security", LOG_AUTH },		/* DEPRECATED */
    { "syslog", LOG_SYSLOG },
    { "user", LOG_USER },
    { "uucp", LOG_UUCP },
    { "local0", LOG_LOCAL0 },
    { "local1", LOG_LOCAL1 },
    { "local2", LOG_LOCAL2 },
    { "local3", LOG_LOCAL3 },
    { "local4", LOG_LOCAL4 },
    { "local5", LOG_LOCAL5 },
    { "local6", LOG_LOCAL6 },
    { "local7", LOG_LOCAL7 },
    { NULL, -1 }
  };
#endif /* _IU_LOGPRIO_H */
