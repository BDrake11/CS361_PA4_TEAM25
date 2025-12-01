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
#include <sys/time.h>

#include "wrappers.h"
#include "message.h"

typedef struct sockaddr SA;

/*-------------------------------------------------------*/
int main(int argc, char *argv[])
{
    int numFactories,           // total number of factory threads
        activeFactories,        // how many are still running
        iters[MAXFACTORIES + 1]     = {0},  // iterations per factory
        partsMade[MAXFACTORIES + 1] = {0},  // parts per factory
        totalItems = 0;

    char *myName = "Braden Drake, Aiden Smith";
    printf("\nPROCUREMENT: Started. Developed by %s\n\n", myName);

    char myUserName[30];
    getlogin_r(myUserName, 30);
    time_t now;
    time(&now);
    fprintf(stdout, "Logged in as user '%s' on %s\n\n",
            myUserName, ctime(&now));
    fflush(stdout);

    if (argc < 4) {
        printf("PROCUREMENT Usage: %s <order_size> <FactoryServerIP> <port>\n",
               argv[0]);
        exit(-1);
    }

    unsigned       orderSize = (unsigned) atoi(argv[1]);
    char          *serverIP  = argv[2];
    unsigned short port      = (unsigned short) atoi(argv[3]);

    printf("Attempting Factory server at '%s' : %d\n", serverIP, port);

    /* ------------------------- Set up socket --------------------------- */
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
        err_sys("Could not create socket.");

    struct sockaddr_in srvrSkt;
    memset((void *) &srvrSkt, 0, sizeof(srvrSkt));
    srvrSkt.sin_family = AF_INET;
    srvrSkt.sin_port   = htons(port);

    if (inet_pton(AF_INET, serverIP,
                  (void *) &srvrSkt.sin_addr.s_addr) != 1)
        err_sys("Invalid server IP address");

    /* ---------------------- Send REQUEST_MSG --------------------------- */
    msgBuf msg1;
    memset(&msg1, 0, sizeof(msg1));
    msg1.purpose   = htonl(REQUEST_MSG);
    msg1.orderSize = htonl(orderSize);

    sendto(sd, &msg1, sizeof(msg1), 0, (SA *) &srvrSkt, sizeof(srvrSkt));

    printf("\nPROCUREMENT Sent this message to the FACTORY server: ");
    printMsg(&msg1);
    puts("");

    /* ---------------- Wait for ORDR_CONFIRM from server ---------------- */
    msgBuf msg2;
    memset(&msg2, 0, sizeof(msg2));
    unsigned int alen = sizeof(srvrSkt);

    printf("\nPROCUREMENT is now waiting for order confirmation ...\n");

    if (recvfrom(sd, &msg2, sizeof(msg2), 0,
                 (SA *) &srvrSkt, &alen) < 0)
        err_sys("Error during recvfrom()");

    printf("PROCUREMENT ( by AIDEN SMITH, BRADEN DRAKE ) received this from the FACTORY server: ");
    printMsg(&msg2);
    puts("\n");

    /* ---------------- Start timing at order confirmation --------------- */
    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);

    numFactories    = (int) ntohl(msg2.numFac);
    if (numFactories > MAXFACTORIES)
        numFactories = MAXFACTORIES;
    activeFactories = numFactories;

    /* ------- Collect PRODUCTION & COMPLETION messages from factories --- */
    msgBuf incomingMessage;
    unsigned int srvrLen = sizeof(srvrSkt);

    while (activeFactories > 0) {
        memset(&incomingMessage, 0, sizeof(incomingMessage));

        if (recvfrom(sd, &incomingMessage, sizeof(incomingMessage), 0,
                     (SA *) &srvrSkt, &srvrLen) < 0)
            err_sys("Error during recvfrom()");

        int purpose = ntohl(incomingMessage.purpose);
        int facID   = (int) ntohl(incomingMessage.facID);

        if (purpose == PRODUCTION_MSG) {
            int parts    = (int) ntohl(incomingMessage.partsMade);
            int duration = (int) ntohl(incomingMessage.duration);

            printf("PROCUREMENT  ( by AIDEN SMITH, BRADEN DRAKE ): Factory #%-2d  produced %-5d parts"
                   " in %-4d milliSecs\n",
                   facID, parts, duration);

            iters[facID]++;
            partsMade[facID] += parts;
        }
        else if (purpose == COMPLETION_MSG) {
            printf("PROCUREMENT  ( by AIDEN SMITH, BRADEN DRAKE ): Factory #%-2d       COMPLETED its task\n",
                   facID);
            activeFactories--;
        }
        else if (purpose == PROTOCOL_ERR) {
            printf("PROCUREMENT ( by AIDEN SMITH, BRADEN DRAKE ): Received invalid msg ");
            printMsg(&incomingMessage);
            puts("\n");

            if (close(sd) < 0)
                perror("Error closing socket.");
            exit(1);
        }
        else {
            // Ignore any unexpected message types
        }
    }

    /* ---------------------- Stop timing -------------------------------- */
    gettimeofday(&endTime, NULL);
    double elapsed_ms =
        (endTime.tv_sec  - startTime.tv_sec)  * 1000.0 +
        (endTime.tv_usec - startTime.tv_usec) / 1000.0;

    /* ---------------------- Print summary report ----------------------- */
    totalItems = 0;
    printf("\n\n****** PROCUREMENT  ( by AIDEN SMITH, BRADEN DRAKE ) Summary Report ******\n");
    printf("    Sub-Factory      Parts Made      Iterations\n");

    for (int i = 1; i <= numFactories; i++) {
        totalItems += partsMade[i];
        printf("           %4d        %8d            %4d\n",
               i, partsMade[i], iters[i]);
    }

    printf("===================================================\n");
    printf("Grand total parts made   = %5d   vs  order size of %5d\n",
           totalItems, orderSize);
    printf("\nOrder-to-Completion time = %.1f milliSeconds\n",
           elapsed_ms);

    printf("\n>>> PROCUREMENT  ( by AIDEN SMITH, BRADEN DRAKE ) Terminated\n");

    if (close(sd) < 0) {
        perror("Error closing socket.");
        exit(1);
    }

    return 0;
}
