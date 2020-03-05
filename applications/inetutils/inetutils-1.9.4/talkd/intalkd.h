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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <protocols/talkd.h>
#include <netdb.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#ifndef INADDR_NONE
# define INADDR_NONE -1
#endif

#define USER_ACL_NAME ".talkrc"

#define ACL_ALLOW  0
#define ACL_DENY   1

extern int debug;
extern int logging;
extern int strict_policy;
extern unsigned int timeout;
extern time_t max_idle_time;
extern time_t max_request_ttl;
extern char *hostname;

#define os2sin_addr(cp) (((struct sockaddr_in *)&(cp))->sin_addr)

extern CTL_MSG *find_request (CTL_MSG * request);
extern CTL_MSG *find_match (CTL_MSG * request);
extern int process_request (CTL_MSG * mp, struct sockaddr_in *sa_in,
			    CTL_RESPONSE * rp);

extern int print_request (const char *cp, CTL_MSG * mp);
extern int print_response (const char *cp, CTL_RESPONSE * rp);

extern int insert_table (CTL_MSG * request, CTL_RESPONSE * response);
extern int delete_invite (unsigned long id_num);
extern int new_id (void);
extern void read_acl (char *config_file, int system);
extern int acl_match (CTL_MSG * msg, struct sockaddr_in *sa_in);
extern int announce (CTL_MSG * request, char *remote_machine);
