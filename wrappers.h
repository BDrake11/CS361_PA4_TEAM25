/************************************************
 * Wrappers for system call functions
 ************************************************/
#ifndef WRAPPERS_H
#define WRAPPERS_H

#include <pthread.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>   
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>


void    unix_error(char *msg) ;
void    err_sys( const char* x ) ;
void    err_quit( const char* x ) ;
void    posix_error( int code, char *msg) ;

pid_t   Fork(void);
int     Usleep( useconds_t usec );

typedef void Sigfunc( int ) ;
Sigfunc * sigactionWrapper( int signo, Sigfunc *func ) ;

int     Msgget( key_t key, int msgflg );

int     Shmget( key_t key, size_t size, int shmflg );
void   *Shmat( int shmid, const void *shmaddr, int shmflg );
int     Shmdt( const void *shmaddr ) ;

int     Sem_init( sem_t *sem, int pshared, unsigned int value ) ;
int     Sem_wait( sem_t *sem );
int     Sem_post( sem_t *sem ) ;
int     Sem_destroy( sem_t *sem ) ;
sem_t  *Sem_open( const char *name, int oflag, mode_t mode, unsigned int value );
sem_t  *Sem_open2( const char *name, int oflag );
int     Sem_close( sem_t *sem );
int     Sem_unlink( const char *name );

void    Pthread_create( pthread_t *tidp, pthread_attr_t *attrp , void * (*routine)(void *), void *argp ) ;
void    Pthread_cancel( pthread_t tid ) ;
void    Pthread_join( pthread_t tid, void **thread_return ) ;
void    Pthread_detach( pthread_t tid ) ;
pthread_t   Pthread_self( void ) ;
void    Pthread_exit( void *retval );


#endif