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

/*
 * RIPE-like servers.
 * All of them do not understand -V2.0Md with the exception of RA and RIPN.
 * 6bone-derived servers will accept the flag with a warning (the flag must
 * match /^V [a-zA-Z]{1,4}\d+[\d\.]{0,5}$/).
 */

/* servers which accept the new syntax (-V XXn.n) */
const char *ripe_servers[] = {
  "whois.ripe.net",
  "whois.apnic.net",
  "whois.oleane.net",
  "rr.arin.net",		/* does not accept the old syntax */
  "whois.6bone.net",		/* 3.0.0b1 */
  "whois.aunic.net",
  "whois.connect.com.au",	/* 3.0.0b1 */
  "whois.nic.fr",
  "whois.telstra.net",
  "whois.nic.net.sg",
  "whois.metu.edu.tr",
  "whois.restena.lu",
  "rr.level3.net",		/* 3.0.0a13 */
  "whois.arnes.si",
  NULL
};

/* servers which do not accept the new syntax */
const char *ripe_servers_old[] = {
  "whois.ra.net",
  "whois.nic.it",
  "whois.ans.net",
  "whois.cw.net",
  "whois.ripn.net",
  "whois.nic.ck",
  "whois.domain.kg",
  NULL
};

#if 0
const char *rwhois_servers[] = {
  "whois.isi.edu",		/* V-1.0B9.2 */
  "rwhois.rcp.net.pe",		/* V-1.5.3 */
  "ns.twnic.net",		/* V-1.0B9 */
  "dragon.seed.net.tw",		/* V-1.0B9.2 */
  NULL
};
#endif

const char *gtlds[] = {
  ".com",
  ".net",
  ".org",
  ".edu",
  NULL
};

const char *hide_strings[] = {
  "The Data in Network", "this query",
  "The data in Register", "By submitting",
  "The data contained in Dotster", "to these terms of usage and",
  "This whois service currently only", "top-level domains.",
  "Signature Domains' Whois Service", "agree to abide by the above",
  "Access to ASNIC", "by this policy.",
  "* Copyright (C) 1998 by SGNIC", "* modification.",
  NULL, NULL
};

const char *nic_handles[] = {
  "net-", "whois.arin.net",
  "netblk-", "whois.arin.net",
  "asn-", "whois.arin.net",
  "as-", "whois.ripe.net",
  "lim-", "whois.ripe.net",
  "coco-", "whois.corenic.net",
  "coho-", "whois.corenic.net",
  "core-", "whois.corenic.net",
  NULL, NULL
};

struct ip_del
{
  unsigned long net;
  unsigned long mask;
  const char *serv;
};

struct ip_del ip_assign[] = {
#include "ip_del.h"
  {0, 0, NULL}
};

struct as_del
{
  unsigned short first;
  unsigned short last;
  const char *serv;
};

struct as_del as_assign[] = {
#include "as_del.h"
  {0, 0, NULL}
};

const char *tld_serv[] = {
#include "tld_serv.h"
  NULL, NULL
};
