//---------------------------------------------------------------------
// Assignment : PA-04 Multi-Threaded UDP Server
// Date       :
// Author     : Mohamed Aboutabl
//----------------------------------------------------------------------
#include <stdio.h>
#include <arpa/inet.h>

#include "message.h"

/*--------------------------------------------------------------------
   Print a message buffer
----------------------------------------------------------------------*/
void printMsg( msgBuf *m )
{  
    switch ( ntohl( m->purpose ) )
    {
       case PRODUCTION_MSG :
            printf( "{ PRODUCTION ,FacID=%-3d, Capacity=%-3d, Made=%-4d, duration=%-4dms) }"
                   , ntohl(m->facID) , ntohl(m->capacity) 
                   , ntohl(m->partsMade) , ntohl(m->duration) ) ;
            break ;
    
        case COMPLETION_MSG :
            printf( "{ COMPLETION , FacID=%-3d }" , ntohl(m->facID) ) ;
            break ;

        case REQUEST_MSG :
            printf( "{ REQUEST    , OrderSz=%-3d }" , ntohl(m->orderSize) ) ;
            break ;

        case ORDR_CONFIRM :
            printf( "{ ORDR_CNFRM , numFacThrds=%-3d }" , ntohl(m->numFac) ) ;
            break ;

        case PROTOCOL_ERR :
            printf( "{ PROTOCOL_ERROR }" ) ;
            break ;

        default :
            printf( "{ UNDEFINED_MSG }" ) ;
            break ;
    }

}

