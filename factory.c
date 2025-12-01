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
#include <sys/time.h>

#include "wrappers.h"
#include "message.h"

#define MAXSTR      200
#define IPSTRLEN    50
#define SEM_NAME    "/Team25_mutex"

typedef struct sockaddr SA;

/* ---------------- Per-sub-factory info that main collects ---------------- */
typedef struct {
    int factoryID;      // 1..N
    int capacity;       // max parts per iteration (10..50)
    int duration;       // msec per iteration (500..1200)
    int partsMade;      // total parts this factory made
    int iterations;     // number of iterations this factory ran
} FactoryInfo;

/* ------------- Globals shared with threads (protected as needed) --------- */

// Mutex used when accessing/adjusting remaining work
sem_t *mutex = NULL;

int   remainsToMake = 0;     // protected by mutex
int   actuallyMade   = 0;
int   numActiveFactories = 1;
int   orderSize = 0;

int sd;                      // server socket descriptor
struct sockaddr_in srvrSkt;  // address of this server
struct sockaddr_in clntSkt;  // remote client's socket

/* ------------------------------------------------------------------------ */

int minimum(int a, int b)
{
    return (a <= b ? a : b);
}

void factLog(char *str)
{
    printf("%s", str);
    fflush(stdout);
}

/* ----------------------------- Signal handler --------------------------- */

void goodbye(int sig)
{
    /* Mission Accomplished */
    printf("\n### I (%d) have been nicely asked to TERMINATE. goodbye\n\n",
           getpid());

    // Tell the current client (if any) that the protocol ended abruptly
    msgBuf errorBuf;
    memset(&errorBuf, 0, sizeof(errorBuf));
    errorBuf.purpose = htonl(PROTOCOL_ERR);
    sendto(sd, &errorBuf, sizeof(errorBuf), 0,
           (SA *) &clntSkt, sizeof(clntSkt));

    // Close and unlink mutex
    if (mutex != NULL) {
        Sem_close(mutex);
        Sem_unlink(SEM_NAME);
    }

    // Close socket
    if (close(sd) < 0) {
        perror("Error closing socket.");
        exit(1);
    }

    exit(0);
}

/* ------------------------ Thread routine prototype ---------------------- */
void *subFactory(void *arg);

/* ======================================================================== */

int main(int argc, char *argv[])
{
    char *myName = "Braden Drake, Aiden Smith";
    unsigned short port = 50015;   /* service port number  */
    int N = 1;                     /* Num sub-factory threads per order */

    char buf[MAXSTR];
    unsigned int alen;

    printf("\nThis is the FACTORY server developed by %s\n\n", myName);
    char myUserName[30];
    getlogin_r(myUserName, 30);
    time_t now;
    time(&now);
    fprintf(stdout, "Logged in as user '%s' on %s\n",
            myUserName, ctime(&now));
    fflush(stdout);

    // Install Ctrl-C and Kill handlers
    sigactionWrapper(SIGINT,  goodbye);
    sigactionWrapper(SIGTERM, goodbye);

    /* ------------- Command line: [numThreads] [port] -------------------- */
    switch (argc) {
        case 1:
            break;  // use default port, N=1
        case 2:
            N = atoi(argv[1]);
            port = 50015;
            break;
        case 3:
            N = atoi(argv[1]);
            port = (unsigned short) atoi(argv[2]);
            break;
        default:
            printf("FACTORY Usage: %s [numThreads] [port]\n", argv[0]);
            exit(1);
    }

    if (N <= 0)
        N = 1;
    if (N > MAXFACTORIES)
        N = MAXFACTORIES;

    /* ------------------------ Set up UDP socket ------------------------- */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
        err_sys("Could not create socket.");

    memset((void *) &srvrSkt, 0, sizeof(srvrSkt));
    srvrSkt.sin_family      = AF_INET;
    srvrSkt.sin_addr.s_addr = htonl(INADDR_ANY);
    srvrSkt.sin_port        = htons(port);

    if (bind(sd, (SA *) &srvrSkt, sizeof(srvrSkt)) < 0) {
        snprintf(buf, MAXSTR, "Could not bind to port %d", port);
        err_sys(buf);
    }

    char ipStr[IPSTRLEN];
    inet_ntop(AF_INET, (void *) &srvrSkt.sin_addr.s_addr, ipStr, IPSTRLEN);
    printf("\nBound socket %d to IP %s Port %d\n", sd, ipStr,
           ntohs(srvrSkt.sin_port));

    /* --------------------- Open named semaphore ------------------------ */
    mutex = Sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);

    // Seed the random number generator once
    srand((unsigned int) time(NULL));

    int forever = 1;
    while (forever) {
        msgBuf msg1;
        alen = sizeof(clntSkt);
        memset(&msg1, 0, sizeof(msg1));

        printf("\nFACTORY server waiting for Order Requests\n\n");

        /* ---------------------- Receive REQUEST_MSG -------------------- */
        if (recvfrom(sd, &msg1, sizeof(msg1), 0,
                     (SA *) &clntSkt, &alen) < 0) {
            err_sys("Error during recvfrom()");
        }

        printf("FACTORY server received: ");
        printMsg(&msg1);
        puts("");
        inet_ntop(AF_INET, (void *) &clntSkt.sin_addr.s_addr,
                  ipStr, IPSTRLEN);
        printf("        From IP %s Port %d\n",
               ipStr, ntohs(clntSkt.sin_port));

        /* --------------------- Initialize order state ------------------ */
        orderSize      = (int) ntohl(msg1.orderSize);
        remainsToMake  = orderSize;
        numActiveFactories = N;

        /* -------------------- Send ORDR_CONFIRM ------------------------ */
        msg1.purpose = htonl(ORDR_CONFIRM);
        msg1.numFac  = htonl(N);
        sendto(sd, &msg1, sizeof(msg1), 0, (SA *) &clntSkt, alen);

        printf("\n\nFACTORY sent this Order Confirmation to the client ");
        printMsg(&msg1);
        puts("");

        /* ----------------------- Start timing -------------------------- */
        struct timeval startTime, endTime;
        gettimeofday(&startTime, NULL);

        /* -------- Create N sub-factory threads with random params ------ */
        pthread_t   tids[MAXFACTORIES + 1];
        FactoryInfo finfo[MAXFACTORIES + 1];

        for (int i = 1; i <= N; i++) {
            finfo[i].factoryID = i;
            finfo[i].capacity  = 10 + (rand() % 41);   // [10,50]
            finfo[i].duration  = 500 + (rand() % 701); // [500,1200]
            finfo[i].partsMade = 0;
            finfo[i].iterations = 0;

            printf("Created Factory Thread # %d with capacity = %3d parts"
                   " & duration = %4d mSec\n",
                   i, finfo[i].capacity, finfo[i].duration);

            Pthread_create(&tids[i], NULL, subFactory, &finfo[i]);
        }

        /* ------------------- Wait for all sub-factories ---------------- */
        for (int i = 1; i <= N; i++) {
            Pthread_join(tids[i], NULL);
        }

        /* ------------------------ Stop timing -------------------------- */
        gettimeofday(&endTime, NULL);
        double elapsed_ms =
            (endTime.tv_sec  - startTime.tv_sec)  * 1000.0 +
            (endTime.tv_usec - startTime.tv_usec) / 1000.0;

        /* ---------------------- Print summary report ------------------- */
        int grandTotal = 0;

        printf("\n****** FACTORY Server ( by Aiden Smith and Braden Drake ) Summary Report ******\n");
        printf("Sub-Factory   Parts Made   Iterations\n");

        for (int i = 1; i <= N; i++) {
            grandTotal += finfo[i].partsMade;
            printf("%6d %13d %11d\n",
                   finfo[i].factoryID,
                   finfo[i].partsMade,
                   finfo[i].iterations);
        }

        printf("=========================================\n");
        printf("Grand total parts made = %5d   vs  order size of %5d\n",
               grandTotal, orderSize);
        printf("Order-to-Completion time = %.1f milliSeconds\n\n",
               elapsed_ms);
    }

    /* --------------- Clean up if we ever break out of loop ------------- */
    Sem_close(mutex);
    Sem_unlink(SEM_NAME);

    if (close(sd) < 0) {
        perror("Error closing socket.");
        exit(1);
    }

    return 0;
}

/* ======================================================================== */
/*                         Sub-factory thread routine                       */
/* ======================================================================== */

void *subFactory(void *arg)
{
    FactoryInfo *info = (FactoryInfo *) arg;
    char   strBuff[MAXSTR];
    msgBuf msg;

    while (1) {
        int toMake = 0;

        /* --------- Decide how many parts to make this iteration -------- */
        Sem_wait(mutex);

        if (remainsToMake <= 0) {
            // No more work left for anybody
            Sem_post(mutex);
            break;
        }

        toMake = minimum(remainsToMake, info->capacity);
        remainsToMake -= toMake;

        info->partsMade  += toMake;
        info->iterations += 1;

        Sem_post(mutex);

        /* ------------- Simulate manufacturing time -------------------- */
        Usleep((useconds_t) info->duration * 1000);

        /* ------------------ Send PRODUCTION_MSG ----------------------- */
        memset(&msg, 0, sizeof(msg));
        msg.purpose  = htonl(PRODUCTION_MSG);
        msg.facID    = htonl(info->factoryID);
        msg.capacity = htonl(info->capacity);
        msg.partsMade= htonl(toMake);
        msg.duration = htonl(info->duration);

        sendto(sd, &msg, sizeof(msg), 0,
               (SA *) &clntSkt, sizeof(clntSkt));

        printf("Factory # %2d: Going to make %5d parts in %4d mSec\n",
               info->factoryID, toMake, info->duration);
        fflush(stdout);
    }

    /* ------------------ Send COMPLETION_MSG --------------------------- */
    msgBuf done;
    memset(&done, 0, sizeof(done));
    done.purpose = htonl(COMPLETION_MSG);
    done.facID   = htonl(info->factoryID);

    sendto(sd, &done, sizeof(done), 0,
           (SA *) &clntSkt, sizeof(clntSkt));

    snprintf(strBuff, MAXSTR,
             ">>> Factory # %-3d : Terminating after making total of %-5d"
             " parts in %-4d iterations\n",
             info->factoryID, info->partsMade, info->iterations);
    factLog(strBuff);

    return NULL;
}
