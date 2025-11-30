//---------------------------------------------------------------------
// Assignment : PA-04 Multi-Threaded UDP Server
// Date       : 12/01/25
// Author     : Braden Drake, Aiden Smith
// File Name  : procurement.c
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

#define MAXFACTORIES    20

typedef struct sockaddr SA ;

/*-------------------------------------------------------*/
int main( int argc , char *argv[] )
{
    int     numFactories ,      // Total Number of Factory Threads
            activeFactories ,   // How many are still alive and manufacturing parts
            iters[ MAXFACTORIES+1 ] = {0} ,  // num Iterations completed by each Factory
            partsMade[ MAXFACTORIES+1 ] = {0} , totalItems = 0;

    char  *myName = "Braden Drake, Aiden Smith" ; 
    printf("\nPROCUREMENT: Started. Developed by %s\n\n" , myName );    

    char myUserName[30] ;
    getlogin_r ( myUserName , 30 ) ;
    time_t  now;
    time( &now ) ;
    fprintf( stdout , "Logged in as user '%s' on %s\n\n" , myUserName ,  ctime( &now)  ) ;
    fflush( stdout ) ;
    
    if ( argc < 4 )
    {
        printf("PROCUREMENT Usage: %s  <order_size> <FactoryServerIP>  <port>\n" , argv[0] );
        exit( -1 ) ;  
    }

    unsigned        orderSize  = atoi( argv[1] ) ;
    char	       *serverIP   = argv[2] ;
    unsigned short  port       = (unsigned short) atoi( argv[3] ) ;
 
    printf("Attempting Factory server at '%s' : %d\n", serverIP, port);

    /* Set up local and remote sockets */
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
        err_sys("Could not create socket.");

    // Prepare the server's socket address structure
    struct sockaddr_in srvrSkt;
    memset((void *) &srvrSkt, 0, sizeof(srvrSkt));
    srvrSkt.sin_family = AF_INET;
    srvrSkt.sin_port = htons(port);

    // Give the srvrSkt the provided IP address
    if (inet_pton(AF_INET, serverIP, (void *) &srvrSkt.sin_addr.s_addr) != 1)
        err_sys("Invalid server IP address");

    // Build and send the initial request to the Factory Server
    msgBuf  msg1;
    memset(&msg1, 0, sizeof(msg1));
    msg1.purpose = htonl(REQUEST_MSG);
    msg1.orderSize = htonl(orderSize);
    sendto(sd, &msg1, sizeof(msg1), 0, (SA *) &srvrSkt, sizeof(srvrSkt));

    // Print message
    printf("\nPROCUREMENT Sent this message to the FACTORY server: "  );
    printMsg( & msg1 );
    puts("");

    /* Now, wait for order confirmation from the Factory server */
    msgBuf  msg2;
    memset(&msg2, 0, sizeof(msg2));
    unsigned int alen = sizeof(srvrSkt);

    printf ("\nPROCUREMENT is now waiting for order confirmation ...\n" );

    // Receive order confirmation
    if (recvfrom(sd, &msg2, sizeof(msg2), 0, (SA *) &srvrSkt, &alen) < 0)
        err_sys("Error during recvfrom()");
    
    printf("PROCUREMENT received this from the FACTORY server: "  );
    printMsg( & msg2 ); 
    puts("\n");

    // Get the number of factories and assign it to activeFactories
    numFactories = ntohl(msg2.numFac);
    activeFactories = numFactories;

    // Incoming production message buf
    msgBuf incomingMessage;
    unsigned int srvrLen = sizeof(srvrSkt);
    // Monitor all Active Factory Lines & Collect Production Reports
    while ( activeFactories > 0 ) // wait for messages from sub-factories
    {
        // Clear the buffer
        memset(&incomingMessage, 0, sizeof(incomingMessage));

        // Receive a message
        if (recvfrom(sd, &incomingMessage, sizeof(incomingMessage), 0, (SA *) &srvrSkt, &srvrLen) < 0)
            err_sys("Error during recvfrom()");

        // Get the purpose and factory ID
        int purpose = ntohl(incomingMessage.purpose);
        int facID = ntohl(incomingMessage.facID);

        if (purpose == PRODUCTION_MSG) {
            // If it's a production message, print the factory ID,
            // number of parts made, and the duration
            int parts = ntohl(incomingMessage.partsMade);
            int duration = ntohl(incomingMessage.duration);
            printf("PROCUREMENT: Factory #%-2d  produced %-5d parts in %-4d  milliSecs\n", facID, parts, duration);

            // Also increase iterations and the parts made from the factory
            iters[facID]++;
            partsMade[facID] += parts;
        } else if (purpose == COMPLETION_MSG) {
            // If it's a completion message, simply print that
            // the factory completed and decrement activeFactories
            printf("PROCUREMENT: Factory #%-2d        COMPLETED its task\n\n", facID);
            activeFactories--;
        } else if (purpose == PROTOCOL_ERR) {
            // If it's a protocol error message, that means the server stopped (Caught a SIGINT or SIGTERM).
            // Print a protocol error message, close the socket, and exit
            printf("PROCUREMENT: Received invalid msg ");
            printMsg(&incomingMessage);
            puts("\n");
            
            if (close(sd) < 0)
                perror("Error closing socket.");
            
            exit(1);
        }       
    } 

    // Print the summary report
    totalItems  = 0 ;
    printf("\n****** PROCUREMENT Summary Report ******\n");

    // Get the total number of items made and prints how many
    // parts each factory produced and in how many iterations
    for (int i = 1; i <= numFactories; i++) {
        totalItems += partsMade[i];

        printf("Factory # %2d made a total of %5d parts in %5d iterations\n", i, partsMade[i], iters[i]);
    }

    printf("==============================\n") ;

    printf("Grand total parts made = %5d   vs  order size of %5d\n", totalItems, orderSize);

    printf( "\n>>> PROCURMENT Terminated\n");

    // Close socket
    if (close(sd) < 0) {
        perror("Error closing socket.");
        exit(1);
    }

    return 0 ;
}
