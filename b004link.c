
/*
 --   ---------------------------------------------------------------------------
 --
 --      ISERVER  -  INMOS standard file server
 --
 --      b004link.c
 --
 --      Link module for B004 type boards in IBM PCs running Linux
 --
 --      Copyright (c) INMOS Ltd., 1988.
 --      Copyright (c) Christoph Niemann, 1993
 --      All Rights Reserved.
 --
 --   ---------------------------------------------------------------------------
*/

#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <stdio.h>

#include "link-driver.h"
#include "linkio.h"
#include "inmos.h"
#include "iserver.h"


#define NULL_LINK -1

PRIVATE LINK ActiveLink = NULL_LINK;

/*
 *   Open Link
 */
PUBLIC LINK OpenLink ( Name )
	char *Name;
{

	if ( ActiveLink != NULL_LINK )
	{
		return( ER_LINK_CANT );
	}
	if ( *Name == '\0')
	{
		if ( ( ActiveLink = open( "/dev/link0" , O_RDWR ) ) < 0 )
		{
			perror( "/dev/link");
			return( ER_LINK_BUSY );
		}
	}
	else
	{
		if ( ( ActiveLink = open( Name , O_RDWR ) ) < 0 )
		{
			perror( Name );
			return( ER_LINK_BUSY );
		}
	}
	
	return(ActiveLink);
}

/*
 *   Close Link
 */
int CloseLink ( LinkId )
	int LinkId;
{
	if ( LinkId != ActiveLink )
	{
		perror("Error - ispy - Link not active ");
		return( -1 );
	}
	close(ActiveLink);
	ActiveLink = NULL_LINK;
	return(TRUE);
}

/* Timeout is in tenths of a second, as on the native host adapter.
 * This driver has no poll/nonblocking implementation; some versions even
 * ignore their timeout ioctls. Transfer one byte only after its readiness
 * ioctl succeeds, so a blocking syscall cannot wait for the rest of a packet.
 * The descriptor must have a single reader and a single writer.
 */
static int transfer(LINK link, char *buffer, unsigned int count,
                    int timeout, int writing)
{
    struct timespec start, now, pause;
    unsigned int done = 0;
    int ready, n;
    if (timeout < 0 || count > INT_MAX) { errno = EINVAL; return -1; }
    if (count == 0) return 0;
    if (clock_gettime(CLOCK_MONOTONIC, &start) < 0) return -1;
    pause.tv_sec = 0;
    pause.tv_nsec = 1000000L;
    while (done < count) {
        if (timeout != 0) {
            if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
            if ((double)(now.tv_sec - start.tv_sec) +
                (double)(now.tv_nsec - start.tv_nsec) / 1000000000.0 >=
                (double)timeout / 10.0) break;
        }
        ready = ioctl(link, writing ? LINKWRITEABLE : LINKREADABLE, 0);
        if (ready < 0) return -1;
        if (!ready) { (void)nanosleep(&pause, NULL); continue; }
        n = writing ? (int)write(link, buffer + done, 1) :
                      (int)read(link, buffer + done, 1);
        /* A driver error may follow a transfer it did not account for. Do
         * not retry it: doing so could duplicate bytes in the wire stream. */
        if (n < 0) return -1;
        if (n == 0) break;
        done += (unsigned int)n;
    }
    return (int)done;
}

int ReadLink(LINK link, char *buffer, unsigned int count, int timeout)
{
    return transfer(link, buffer, count, timeout, 0);
}

int WriteLink(LINK link, char *buffer, unsigned int count, int timeout)
{
    return transfer(link, buffer, count, timeout, 1);
}

/*
 *   Reset Link
 */
PUBLIC int ResetLink ( LinkId )
	LINK LinkId;
{
	if ( LinkId != ActiveLink )
	{
		perror("Error - ispy - Link not active ");
		return( -1 );
	}
	if ( ioctl( LinkId, LINKRESET, 0) != 0 )
	{
		perror("Error - ispy - ioctl failed ");
		return( -1 );
	}
	return( 1 );
}

/*
 *   Analyse Link
 */ 
PUBLIC int AnalyseLink ( LinkId )
	LINK LinkId;
{
	if ( LinkId != ActiveLink )
	{
		perror("Error - ispy - Link not active ");
		return( -1 );
	}
	if ( ioctl( LinkId, LINKANALYSE, 0) != 0 )
	{
		perror("Error - ispy - ioctl failed ");
		return( -1 );
	}
	return( 1 );
}

/*
 *   Test Error
 */ 
PUBLIC int TestError ( LinkId )
	LINK LinkId;
{
	if ( LinkId != ActiveLink )
	{
		perror("Error - ispy - Link not active ");
		return( -1 );
	}
	return ( ioctl( LinkId, LINKERROR, 0 ) );
}

/*
 *   Test Read
 */
PUBLIC int TestRead ( LinkId )
	LINK LinkId;
{
	if ( LinkId != ActiveLink )
	{
		perror("Error - ispy - Link not active ");
		return( -1 );
	}
	return ( ioctl( LinkId, LINKREADABLE, 0 ) );
}




/*
 *   Test Write
 */

PUBLIC int TestWrite ( LinkId )
	LINK LinkId;
{
	if ( LinkId != ActiveLink )
	{
		perror("Error - ispy - Link not active ");
		return( -1 );
	}
	return ( ioctl( LinkId, LINKWRITEABLE, 0 ) );
}



/*
 *   Eof
 */
