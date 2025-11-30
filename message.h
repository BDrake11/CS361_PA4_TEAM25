//---------------------------------------------------------------------
// Assignment : PA-04 Multi-Threaded UDP Server
// Date       :
// Author     : Mohamed Aboutabl
//----------------------------------------------------------------------

#ifndef  MESSAGE_H
#define  MESSAGE_H
#include <sys/types.h>

#define MAXFACTORIES    20

typedef enum 
{
    PRODUCTION_MSG = 1 , COMPLETION_MSG , REQUEST_MSG , ORDR_CONFIRM , PROTOCOL_ERR 
} msgPurpose_t;

typedef struct {

    int       purpose ;  /* Purpose of this message to Supervisor */

    unsigned  orderSize ,      /* Initial requested order size */
              numFac    ,      /* number of Factory Threads serving the client */
              facID     ,      /* sender's Factory ID */
              capacity  ,      /* #of parts made in most recent iteration */
              partsMade ,      /* #of parts made in most recent iteration */
              duration  ;      /* how long it took to make them */

} msgBuf ;

void printMsg( msgBuf *m ) ;

#endif