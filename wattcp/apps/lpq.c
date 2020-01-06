/*
 * Lpq - query printer
 *
 *   Copyright (C) 1991 Erick Engelke
 *
 *   Portions Copyright (C) 1990, National Center for Supercomputer Applications
 *   and portions copyright (c) 1990, Clarkson University
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it, but you may not sell it.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but without any warranty; without even the implied warranty of
 *   merchantability or fitness for a particular purpose.
 *
 *       Erick Engelke                   or via E-Mail
 *       Faculty of Engineering
 *       University of Waterloo          Erick@development.watstar.uwaterloo.ca
 *       200 University Ave.,
 *       Waterloo, Ont., Canada
 *       N2L 3G1
 *
 *  If you want to use this, make sure you are in /etc/host.lpr or better.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcp.h>

#define SHORT_LIST 3
#define LONG_LIST  4

#define LPQ_PORT 515
#define LOCAL_PORT 722

extern int errno;

void usage( void )
{
	printf("usage: lpq [-Pprinter] [-Sserver] [-l]\n");
	exit(1);
}

int main(int argc, char **argv)
{
    char buffer[513];
    char printer[35];
    char host[35];
    char *cptr;

    static tcp_Socket socketdata;
    tcp_Socket *s;
    longword host_ip;
    int status;
    int connected = 0;
    int localport;
    int len;
    int i;
    int verbose=0;
    /* */

    /* Set defaults from the environment */
    *host = *printer = '\0';
    if( (cptr = getenv("LPRSRV")) != NULL )
        strncpy(host,cptr,35);
    if( (cptr = getenv("PRINTER")) != NULL )
        strncpy(printer,cptr,35);


    /* Parse the command line arguments */
    for(i=1; i<=argc ; i++)
    {
	if(argv[i][0] == '-')
	{
		switch(argv[i][1])
	  	{
			case 'P':
				if(strlen(argv[i])>2)
				{
					strcpy(printer,&argv[i][2]);
				}
				else
				{
					i++;
					strcpy(printer,argv[i]);
				}
				break;
			case 'S':
				if(strlen(argv[i])>2)
				{
					strcpy(host,&argv[i][2]);
				}
				else
				{
					i++;
					strcpy(host,argv[i]);
				}
				break;
			case 'l':
				verbose=1;
				break;
			default:
				usage();
		}
	}
    }

   /* Verify that we have enough data to procede */
   if(strlen(printer)==0)
   {
	printf("A printer must be specified! Use either the command line\n");
	printf("flag, -Pprinter, or the environment variable, PRINTER.\n");
	exit(1);
   }

   if(strlen(host)==0)
   {
	printf("A LPR Server must be specified! Use either the command line\n");
	printf("flag, -Sserver, or the environment variable, LPRSRV.\n");
	exit(1);
   }
	
   sock_init();

   s = &socketdata;
   if (!(host_ip = resolve( host ))) {
      fprintf(stderr, "lpq: unknown host %s\n\r",host);
      exit(1);
   }

   localport = 255 + (MsecClock() & 255);
   localport = LOCAL_PORT;

   if ( !tcp_open( s, localport, host_ip, LPQ_PORT, NULL)) {
      fprintf(stderr,"Unable to open socket.");
      exit(1);
   }

   sock_wait_established( s, sock_delay , NULL, &status );
   connected = 1;

   if (sock_dataready( s )) {
       sock_fastread( s, buffer, sizeof( buffer ));
       buffer[ sizeof( buffer ) - 1] = 0;
       printf("Response: %s\n", buffer );
       sock_tick( s, &status );	/* in case above message closed port */
   }

   if (verbose)
   {
	sprintf(buffer,"%c%s\n",LONG_LIST,printer);
   }
   else
   {
	sprintf(buffer,"%c%s\n",SHORT_LIST,printer);
   }
   sock_write(s, buffer, strlen(buffer));

   while ( 1 ) {
	sock_wait_input( s, sock_delay, NULL, &status );
	len = sock_read( s, buffer, sizeof( buffer ));
	printf("%*.*s",len,len,buffer);
   }


sock_err:
   switch ( status) {
	case 1 : status = 0;
		 break;
	case -1: fprintf( stderr, "Host closed connection.\n\r");
		 status = 3;
		 break;
   }
   if (!connected)
       fprintf( stderr , "\n\rCould not get connected.  You may not be in the /etc/hosts.lpd file!\n\r");


   exit( status );
   return (0);   /* not reached */
}
