/*
 *  sctpclnt.c
 *
 *  SCTP multi-stream client.
 *
 *  M. Tim Jones <mtj@mtjones.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include "common.h"

int main()
{
  int connSock, in, i, ret, flags;
  struct sockaddr_in servaddr;
  struct sctp_status status;
  struct sctp_sndrcvinfo sndrcvinfo;
  struct sctp_event_subscribe events;
  struct sctp_initmsg initmsg;
  char buffer[MAX_BUFFER+1];

  /* Create an SCTP TCP-Style Socket */
  connSock = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP );

  /* Specify that a maximum of 5 streams will be available per socket */
  memset( &initmsg, 0, sizeof(initmsg) );
  initmsg.sinit_num_ostreams = 5;
  initmsg.sinit_max_instreams = 5;
  initmsg.sinit_max_attempts = 4;
  ret = setsockopt( connSock, IPPROTO_SCTP, SCTP_INITMSG,
                     &initmsg, sizeof(initmsg) );

  /* Specify the peer endpoint to which we'll connect */
  bzero( (void *)&servaddr, sizeof(servaddr) );
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(MY_PORT_NUM);
  servaddr.sin_addr.s_addr = inet_addr( "127.0.0.1" );

  /* Connect to the server */
  ret = connect( connSock, (struct sockaddr *)&servaddr, sizeof(servaddr) );

  /* Enable receipt of SCTP Snd/Rcv Data via sctp_recvmsg */
  memset( (void *)&events, 0, sizeof(events) );
  events.sctp_data_io_event = 1;
  ret = setsockopt( connSock, SOL_SCTP, SCTP_EVENTS,
                     (const void *)&events, sizeof(events) );

  /* Read and emit the status of the Socket (optional step) */
  in = sizeof(status);
  ret = getsockopt( connSock, SOL_SCTP, SCTP_STATUS,
                     (void *)&status, (socklen_t *)&in );

  printf("assoc id  = %d\n", status.sstat_assoc_id );
  printf("state     = %d\n", status.sstat_state );
  printf("instrms   = %d\n", status.sstat_instrms );
  printf("outstrms  = %d\n", status.sstat_outstrms );

  /* Expect two messages from the peer */

  for (i = 0 ; i < 2 ; i++) {

    in = sctp_recvmsg( connSock, (void *)buffer, sizeof(buffer),
                        (struct sockaddr *)NULL, 0, &sndrcvinfo, &flags );

    if (in > 0) {
      buffer[in] = 0;
      if (sndrcvinfo.sinfo_stream == LOCALTIME_STREAM) {
        printf("(Local) %s\n", buffer);
      } else if (sndrcvinfo.sinfo_stream == GMT_STREAM) {
        printf("(GMT  ) %s\n", buffer);
      }
    }

  }

  /* Close our socket and exit */
  close(connSock);

  return 0;
}

