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

/*
 * Structure of an icmp header.
 */
typedef struct icmp_header icmphdr_t;

struct icmp_header
{
  unsigned char icmp_type;		/* type of message, see below */
  unsigned char icmp_code;		/* type sub code */
  unsigned short icmp_cksum;		/* ones complement cksum of struct */
  union
  {
    unsigned char ih_pptr;		/* ICMP_PARAMPROB */
    struct in_addr ih_gwaddr;	/* ICMP_REDIRECT */
    struct ih_idseq
    {
      unsigned short icd_id;
      unsigned short icd_seq;
    } ih_idseq;
    int ih_void;

    /* ICMP_UNREACH_NEEDFRAG -- Path MTU discovery as per rfc 1191 */
    struct ih_pmtu
    {
      unsigned short ipm_void;
      unsigned short ipm_nextmtu;
    } ih_pmtu;

    /* ICMP_ROUTERADV -- RFC 1256 */
    struct ih_rtradv
    {
      unsigned char irt_num_addrs;	/* Number of addresses following the msg */
      unsigned char irt_wpa;		/* Address Entry Size (32-bit words) */
      unsigned short irt_lifetime;	/* Lifetime */
    } ih_rtradv;

  } icmp_hun;
#define icmp_pptr	icmp_hun.ih_pptr
#define icmp_gwaddr	icmp_hun.ih_gwaddr
#define icmp_id		icmp_hun.ih_idseq.icd_id
#define icmp_seq	icmp_hun.ih_idseq.icd_seq
#define icmp_void	icmp_hun.ih_void
#define icmp_pmvoid     icmp_hun.ih_pmtu.ipm_void
#define icmp_nextmtu    icmp_hun.ih_pmtu.ipm_nextmtu
#define icmp_num_addrs  icmp_hun.ih_rtradv.irt_num_addrs
#define icmp_wpa        icmp_hun.ih_rtradv.irt_wpa
#define icmp_lifetime   icmp_hun.ih_rtradv.irt_lifetime

  union
  {
    struct id_ts		/* ICMP_TIMESTAMP, ICMP_TIMESTAMPREPLY */
    {
      n_time its_otime;		/* Originate timestamp */
      n_time its_rtime;		/* Recieve timestamp */
      n_time its_ttime;		/* Transmit timestamp */
    } id_ts;
    struct id_ip		/* Original IP header */
    {
      struct ip idi_ip;
      /* options and then 64 bits of data */
    } id_ip;
    unsigned long id_mask;		/* ICMP_ADDRESS, ICMP_ADDRESSREPLY */
    char id_data[1];
  } icmp_dun;
#define icmp_otime	icmp_dun.id_ts.its_otime
#define icmp_rtime	icmp_dun.id_ts.its_rtime
#define icmp_ttime	icmp_dun.id_ts.its_ttime
#define icmp_ip		icmp_dun.id_ip.idi_ip
#define icmp_mask	icmp_dun.id_mask
#define icmp_data	icmp_dun.id_data
};

#define ICMP_ECHOREPLY		0	/* Echo Reply                   */
#define ICMP_DEST_UNREACH	3	/* Destination Unreachable      */
/* Codes for ICMP_DEST_UNREACH. */
#define ICMP_NET_UNREACH	0	/* Network Unreachable          */
#define ICMP_HOST_UNREACH	1	/* Host Unreachable             */
#define ICMP_PROT_UNREACH	2	/* Protocol Unreachable         */
#define ICMP_PORT_UNREACH	3	/* Port Unreachable             */
#define ICMP_FRAG_NEEDED	4	/* Fragmentation Needed/DF set  */
#define ICMP_SR_FAILED		5	/* Source Route failed		*/
#define ICMP_NET_UNKNOWN	6
#define ICMP_HOST_UNKNOWN	7
#define ICMP_HOST_ISOLATED	8
#define ICMP_NET_ANO		9
#define ICMP_HOST_ANO		10
#define ICMP_NET_UNR_TOS	11
#define ICMP_HOST_UNR_TOS	12
#define ICMP_PKT_FILTERED	13	/* Packet filtered */
#define ICMP_PREC_VIOLATION	14	/* Precedence violation */
#define ICMP_PREC_CUTOFF	15	/* Precedence cut off */
#define NR_ICMP_UNREACH	15	/* total subcodes */

#define ICMP_SOURCE_QUENCH	4	/* Source Quench                */
#define ICMP_REDIRECT		5	/* Redirect (change route)      */
/* Codes for ICMP_REDIRECT. */
#define ICMP_REDIR_NET	0	/* Redirect Net                 */
#define ICMP_REDIR_HOST	1	/* Redirect Host                */
#define ICMP_REDIR_NETTOS	2	/* Redirect Net for TOS         */
#define ICMP_REDIR_HOSTTOS	3	/* Redirect Host for TOS        */

#define ICMP_ECHO		8	/* Echo Request                 */
#define ICMP_ROUTERADV          9	/* Router Advertisement -- RFC 1256 */
#define ICMP_ROUTERDISCOVERY    10	/* Router Discovery -- RFC 1256 */
#define ICMP_TIME_EXCEEDED	11	/* Time Exceeded                */
/* Codes for TIME_EXCEEDED. */
#define ICMP_EXC_TTL		0	/* TTL count exceeded           */
#define ICMP_EXC_FRAGTIME	1	/* Fragment Reass time exceeded */

#define ICMP_PARAMETERPROB	12	/* Parameter Problem            */
#define ICMP_TIMESTAMP		13	/* Timestamp Request            */
#define ICMP_TIMESTAMPREPLY	14	/* Timestamp Reply              */
#define ICMP_INFO_REQUEST	15	/* Information Request          */
#define ICMP_INFO_REPLY		16	/* Information Reply            */
#define ICMP_ADDRESS		17	/* Address Mask Request         */
#define ICMP_ADDRESSREPLY	18	/* Address Mask Reply           */
#define NR_ICMP_TYPES		18



#define MAXIPLEN	60
#define MAXICMPLEN	76
#define ICMP_MINLEN	8	/* abs minimum */
#define ICMP_TSLEN	(8 + 3 * sizeof (n_time))	/* timestamp */
#define ICMP_MASKLEN	12	/* address mask */
#define ICMP_ADVLENMIN	(8 + sizeof (struct ip) + 8)	/* min */
#define ICMP_ADVLEN(p)	(8 + ((p)->icmp_ip.ip_hl << 2) + 8)
	/* N.B.: must separately check that ip_hl >= 5 */

unsigned short icmp_cksum (unsigned char * addr, int len);
int icmp_generic_encode (unsigned char * buffer, size_t bufsize, int type, int ident,
			 int seqno);
int icmp_generic_decode (unsigned char * buffer, size_t bufsize,
			 struct ip **ipp, icmphdr_t ** icmpp);

int icmp_echo_encode (unsigned char * buffer, size_t bufsize, int ident, int seqno);
int icmp_echo_decode (unsigned char * buffer, size_t bufsize,
		      struct ip **ip, icmphdr_t ** icmp);
int icmp_timestamp_encode (unsigned char * buffer, size_t bufsize,
			   int ident, int seqno);
int icmp_address_encode (unsigned char * buffer, size_t bufsize, int ident,
			 int seqno);
