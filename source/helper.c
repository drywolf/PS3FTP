/*

  HELPER.C
  ========
  (c) Paul Griffiths, 1999
  Email: mail@paulgriffiths.net

  Implementation of sockets helper functions.

  Many of these functions are adapted from, inspired by, or 
  otherwise shamelessly plagiarised from "Unix Network 
  Programming", W Richard Stevens (Prentice Hall).

*/

#include "helper.h"
#include <sys/socket.h>
#include <net/net.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


/*  Read a line from a socket  */

ssize_t Readline(int sockd, char* vptr, size_t maxlen){
    ssize_t n, rc;
    char    c, *buffer;

    buffer = vptr;

    for ( n = 1; n < (ssize_t)maxlen; n++ ){
		if ( (rc = recv(sockd, &c, 1,0)) == 1 ){
			if(c == '\r') continue;
			if ( c == '\n') break;
			*buffer++ = c;
		}else if ( rc == 0 ){
			if ( n == 1 )
				return 0;
			else
				break;
		}else{
			if ( errno == EINTR )
				continue;
			return -1;
		}
    }

    *buffer = 0;
    return n;
}


/*  Write a line to a socket  */

ssize_t Writeline(int sockd, const char* vptr, size_t n){
    size_t      nleft;
    ssize_t     nwritten;
    const char *buffer;

    buffer = vptr;
    nleft  = n;

    while ( nleft > 0 ){
		if ( (nwritten = send(sockd, buffer, nleft,0)) <= 0 ){
			if ( errno == EINTR )
				nwritten = 0;
			else
				return -1;
		}
		nleft  -= nwritten;
		buffer += nwritten;
    }
	return n;
}

ssize_t WritelineEx(int sockd, const char *vptr, int debug)
{
	if(debug)
		printf("Response: %s", vptr);
	return Writeline(sockd, vptr, strlen(vptr));
}
