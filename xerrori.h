#ifndef XERRORI_H
#define XERRORI_H

#define _GNU_SOURCE   
#include <stdio.h>    
#include <stdlib.h>   
#include <assert.h>  
#include <string.h>  
#include <errno.h>
#include <search.h>
#include <signal.h>  
#include <unistd.h>  
#include <stdbool.h>
#include <semaphore.h>
#include <pthread.h> 
#include <fcntl.h>   
#include <arpa/inet.h>
#include <stdint.h> 
#include <pthread.h>

#define QUI __LINE__,__FILE__
#define PC_buffer_len 10// lunghezza dei buffer produttori/consumatori

// termina programma
void termina(const char *s); 

// operazioni su FILE *
FILE *xfopen(const char *path, const char *mode, int linea, char *file);

// operazioni su file descriptors
void xclose(int fd, int linea, char *file);

// semafori POSIX
int xsem_init(sem_t *sem, int pshared, unsigned int value, int linea, char *file);
int xsem_destroy(sem_t *sem, int linea, char *file);
int xsem_post(sem_t *sem, int linea, char *file);
int xsem_wait(sem_t *sem, int linea, char *file);

// thread
void xperror(int en, char *msg);

int xpthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg, int linea, char *file);
int xpthread_join(pthread_t thread, void **retval, int linea, char *file);

// mutex 
int xpthread_mutex_destroy(pthread_mutex_t *mutex, int linea, char *file);
int xpthread_mutex_lock(pthread_mutex_t *mutex, int linea, char *file);
int xpthread_mutex_unlock(pthread_mutex_t *mutex, int linea, char *file);

// condition variables
int xpthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr, int linea, char *file);
int xpthread_cond_destroy(pthread_cond_t *cond, int linea, char *file);
int xpthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, int linea, char *file);
int xpthread_cond_signal(pthread_cond_t *cond, int linea, char *file);

#endif  // XERRORI_H