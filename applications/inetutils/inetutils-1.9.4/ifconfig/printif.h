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

/* Written by Marcus Brinkmann.  */

#ifndef IFCONFIG_PRINTIF_H
# define IFCONFIG_PRINTIF_H

# include <netinet/in.h>
# include <net/if.h>
# include <arpa/inet.h>
# include "ifconfig.h"

/* The column position of the cursor.  */
extern int *column;

/* Whenever you output something, set this to true.  */
extern int had_output;

struct format_data
{
  const char *name;		/* Name of interface as specified on the
				   command line.  */
  struct ifreq *ifr;
  int sfd;			/* Socket file descriptor to use.  */
  int first;			/* This is the first interface.  */
  const char *format;		/* The format string.  */
  int depth;			/* Depth of nesting in parsing.  */
};

typedef struct format_data *format_data_t;

typedef void (*format_handler_t) (format_data_t, int, char **);

struct format_handle
{
  const char *name;		/* The name of the handler.  */
  format_handler_t handler;
};

extern struct format_handle format_handles[];

/* Each TAB_STOP characters is a default tab stop, which is also used
   by '\t'.  */
# define TAB_STOP 8

void put_char (format_data_t form, char c);
void put_string (format_data_t form, const char *s);
void put_int (format_data_t form, int argc, char *argv[], int nr);
void put_ulong (format_data_t form, int argc, char *argv[], unsigned long val);
void select_arg (format_data_t form, int argc, char *argv[], int nr);
void put_addr (format_data_t form, int argc, char *argv[],
	       struct sockaddr *sa);
void put_flags (format_data_t form, int argc, char *argv[], int flags);

/* Format handler can mangle form->format, so update it after calling
   here.  */
void format_handler (const char *name, format_data_t form, int argc,
		     char *argv[]);

void fh_nothing (format_data_t form, int argc, char *argv[]);
void fh_format_query (format_data_t form, int argc, char *argv[]);
void fh_docstr (format_data_t form, int argc, char *argv[]);
void fh_defn (format_data_t form, int argc, char *argv[]);
void fh_foreachformat (format_data_t form, int argc, char *argv[]);
void fh_verbose_query (format_data_t form, int argc, char *argv[]);
void fh_newline (format_data_t form, int argc, char *argv[]);
void fh_tabulator (format_data_t form, int argc, char *argv[]);
void fh_rep (format_data_t form, int argc, char *argv[]);
void fh_first (format_data_t form, int argc, char *argv[]);
void fh_ifdisplay_query (format_data_t form, int argc, char *argv[]);
void fh_tab (format_data_t form, int argc, char *argv[]);
void fh_join (format_data_t form, int argc, char *argv[]);
void fh_exists_query (format_data_t form, int argc, char *argv[]);
void fh_format (format_data_t form, int argc, char *argv[]);
void fh_error (format_data_t form, int argc, char *argv[]);
void fh_progname (format_data_t form, int argc, char *argv[]);
void fh_exit (format_data_t form, int argc, char *argv[]);
void fh_name (format_data_t form, int argc, char *argv[]);
void fh_index_query (format_data_t form, int argc, char *argv[]);
void fh_index (format_data_t form, int argc, char *argv[]);
void fh_addr_query (format_data_t form, int argc, char *argv[]);
void fh_addr (format_data_t form, int argc, char *argv[]);
void fh_netmask_query (format_data_t form, int argc, char *argv[]);
void fh_netmask (format_data_t form, int argc, char *argv[]);
void fh_brdaddr_query (format_data_t form, int argc, char *argv[]);
void fh_brdaddr (format_data_t form, int argc, char *argv[]);
void fh_dstaddr_query (format_data_t form, int argc, char *argv[]);
void fh_dstaddr (format_data_t form, int argc, char *argv[]);
void fh_flags_query (format_data_t form, int argc, char *argv[]);
void fh_flags (format_data_t form, int argc, char *argv[]);
void fh_mtu_query (format_data_t form, int argc, char *argv[]);
void fh_mtu (format_data_t form, int argc, char *argv[]);
void fh_metric_query (format_data_t form, int argc, char *argv[]);
void fh_metric (format_data_t form, int argc, char *argv[]);
void fh_map_query (format_data_t form, int argc, char *argv[]);
void fh_irq_query (format_data_t form, int argc, char *argv[]);
void fh_irq (format_data_t form, int argc, char *argv[]);
void fh_baseaddr_query (format_data_t form, int argc, char *argv[]);
void fh_baseaddr (format_data_t form, int argc, char *argv[]);
void fh_memstart_query (format_data_t form, int argc, char *argv[]);
void fh_memstart (format_data_t form, int argc, char *argv[]);
void fh_memend_query (format_data_t form, int argc, char *argv[]);
void fh_memend (format_data_t form, int argc, char *argv[]);
void fh_dma_query (format_data_t form, int argc, char *argv[]);
void fh_dma (format_data_t form, int argc, char *argv[]);
void fh_media_query (format_data_t form, int argc, char *argv[]);
void fh_media (format_data_t form, int argc, char *argv[]);
void fh_status_query (format_data_t form, int argc, char *argv[]);
void fh_status (format_data_t form, int argc, char *argv[]);

/* Used for recursion by format handlers.  */
void print_interfaceX (format_data_t form, int quiet);

void print_interface (int sfd, const char *name, struct ifreq *ifr,
		      const char *format);

#endif
