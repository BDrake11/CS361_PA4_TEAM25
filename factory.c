//---------------------------------------------------------------------
// Assignment : PA-04 Multi-Threaded UDP Server
// Date       : 12/01/25
// Author     : Braden Drake, Aiden Smith
// File Name  : factory.c
//---------------------------------------------------------------------

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "wrappers.h"
#include "message.h"

#define MAXSTR     200
#define IPSTRLEN    50
#define SEM_NAME "/Team25_mutex"

// Mutex used when making parts
sem_t *mutex;

typedef struct sockaddr SA ;

int minimum( int a , int b)
{
    return ( a <= b ? a : b ) ; 
}

void subFactory( int factoryID , int myCapacity , int myDuration ) ;

void factLog( char *str )
{
    printf( "%s" , str );
    fflush( stdout ) ;
}

/*-------------------------------------------------------*/

// Global Variable for Future Thread to Shared
int   remainsToMake , // Must be protected by a Mutex
      actuallyMade ;  // Actually manufactured items

int   numActiveFactories = 1 , orderSize ;

int   sd ;      // Server socket descriptor
struct sockaddr_in  
             srvrSkt,       /* the address of this server   */
             clntSkt;       /* remote client's socket       */

//------------------------------------------------------------
//  Handle Ctrl-C or KILL 
//------------------------------------------------------------
void goodbye(int sig) 
{
    /* Mission Accomplished */
    printf( "\n### I (%d) have been nicely asked to TERMINATE. "
           "goodbye\n\n" , getpid() );

    // Send a protocol error to the client
    msgBuf errorBuf;
    errorBuf.purpose = htonl(PROTOCOL_ERR);
    sendto(sd, &errorBuf, sizeof(errorBuf), 0, (SA *) &clntSkt, sizeof(clntSkt));
    
    // Close and unlink mutex
    Sem_close(mutex);
    Sem_unlink(SEM_NAME);

    // Close socket
    if (close(sd) < 0) {
        perror("Error closing socket.");
        exit(1);
    }

    exit(0);
}

/*-------------------------------------------------------*/
int main( int argc , char *argv[] )
{
    char  *myName = "Braden Drake, Aiden Smith" ; 
    unsigned short port = 50015 ;      /* service port number  */
    int    N = 1 ;                     /* Num threads serving the client */

    // buffer
    char buf[MAXSTR];
    // from-address length
    unsigned int alen;

    printf("\nThis is the FACTORY server developed by %s\n\n" , myName ) ;
    char myUserName[30] ;
    getlogin_r ( myUserName , 30 ) ;
    time_t  now;
    time( &now ) ;
    fprintf( stdout , "Logged in as user '%s' on %s\n" , myUserName ,  ctime( &now)  ) ;
    fflush( stdout ) ;

    //Ctrl-C and Kill handlers
    sigactionWrapper(SIGINT, goodbye);
    sigactionWrapper(SIGTERM, goodbye);

	switch (argc) 
	{
      case 1:
        break ;     // use default port with a single factory thread
      
      case 2:
        N = atoi( argv[1] ); // get from command line
        port = 50015;            // use this port by default
        break;

      case 3:
        N    = atoi( argv[1] ) ; // get from command line
        port = atoi( argv[2] ) ; // use port from command line
        break;

      default:
        printf( "FACTORY Usage: %s [numThreads] [port]\n" , argv[0] );
        exit( 1 ) ;
    }

    // Create socket
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
        err_sys("Could not create socket.");
    
    // Prepare the server's socket address structure
    memset((void *) &srvrSkt, 0, sizeof(srvrSkt));
    srvrSkt.sin_family = AF_INET;
    srvrSkt.sin_addr.s_addr = htonl(INADDR_ANY);
    srvrSkt.sin_port = htons(port);

    // Bind the server to the socket
    if (bind(sd, (SA *) &srvrSkt, sizeof(srvrSkt)) < 0) {
        snprintf(buf, MAXSTR, "Could not bind to port %d", port);
        err_sys(buf);
    }

    // Print IP and port of server
    char ipStr[IPSTRLEN];
    inet_ntop(AF_INET, (void *) &srvrSkt.sin_addr.s_addr, ipStr, IPSTRLEN);
    printf("\nBound socket %d to IP %s Port %d\n", sd, ipStr, ntohs(srvrSkt.sin_port));

    // Open mutex
    mutex = Sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);

    int forever = 1;
    while ( forever )
    {
        // Message buf to receive order request
        alen = sizeof(clntSkt);
        msgBuf msg1;
        memset(&msg1, 0, sizeof(msg1));
        printf( "\nFACTORY server waiting for Order Requests\n\n\n" ) ; 

        // Receive order request from client
        if (recvfrom(sd, &msg1, sizeof(msg1), 0, (SA *) &clntSkt, &alen) < 0) {
            err_sys("Error during recvfrom()");
        }

        // Print that the factory received an
        // order request and print the clients IP and port
        printf("FACTORY server received: ");
        printMsg(&msg1);
        puts("");
        inet_ntop(AF_INET, (void *) &clntSkt.sin_addr.s_addr, ipStr, IPSTRLEN);
        printf("        From IP %s Port %d\n", ipStr, ntohs(clntSkt.sin_port));

        // Get the order size and assign it to remainsToMake
        orderSize = ntohl(msg1.orderSize);
        remainsToMake = orderSize;

        // Send an order confirmation to the client with the number of factories
        msg1.purpose = htonl(ORDR_CONFIRM);
        msg1.numFac = htonl(N);
        sendto(sd, &msg1, sizeof(msg1), 0, (SA *) &clntSkt, alen);

        // Print order confirmation
        printf("\n\nFACTORY sent this Order Confirmation to the client " );
        printMsg(&msg1);
        puts("");
        
        subFactory( 1 , 50 , 350 ) ;  // Single factory, ID=1 , capacity=50, duration=350 ms
    }

    // Close and unlink semaphores
    Sem_close(mutex);
    Sem_unlink(SEM_NAME);

    // Close socket
    if (close(sd) < 0) {
        perror("Error closing socket.");
        exit(1);
    }

    return 0 ;
}

void subFactory( int factoryID , int myCapacity , int myDuration )
{
    char    strBuff[ MAXSTR ] ;   // snprint buffer
    int     partsImade = 0 , myIterations = 0 ;
    msgBuf  msg;


    while ( 1 )
    {   
        // Reset message buf
        memset(&msg, 0, sizeof(msg));
        int toMake = 0;

        // See if there are still any parts to manufacture
        if ( remainsToMake <= 0 )
            break ;   // Not anymore, exit the loop
        
        // Use mutual exclusion when making parts
        // Increment num of iterations and add total parts made by the factory
        Sem_wait(mutex);
        toMake = (remainsToMake >= myCapacity) ? myCapacity : remainsToMake;
        remainsToMake -= toMake;
        partsImade += toMake;
        myIterations++;
        Sem_post(mutex);

        // Sleep for the duration
        Usleep((useconds_t) myDuration * 1000);

        // Build production message
        msg.purpose = htonl(PRODUCTION_MSG);
        msg.facID = htonl(factoryID);
        msg.capacity = htonl(myCapacity);
        msg.partsMade = htonl(toMake);
        msg.duration = htonl(myDuration);

        // Send a Production Message to client
        sendto(sd, &msg, sizeof(msg), 0, (SA *) &clntSkt, sizeof(clntSkt));
        printf("Factory # %2d: Going to make %5d parts in %4d mSec\n", factoryID, toMake, myDuration);
    }

    // Completion message buf to send to client
    msgBuf done;
    memset(&done, 0, sizeof(done));

    // Build completion message and send it to client
    done.purpose = htonl(COMPLETION_MSG);
    done.facID = htonl(factoryID);
    sendto(sd, &done, sizeof(done), 0, (SA *) &clntSkt, sizeof(clntSkt));

    snprintf( strBuff , MAXSTR , ">>> Factory # %-3d: Terminating after making total of %-5d parts in %-4d iterations\n" 
          , factoryID, partsImade, myIterations);
    factLog( strBuff ) ;
}

