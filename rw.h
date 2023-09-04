#ifndef RW_H
#define RW_H

#include "tabella.h"

typedef struct {
  hash access;
  buffer buf;
  //per lettori un mutex per il file 
  pthread_mutex_t *mutex_fd;
  FILE *file;
}rw;

typedef struct {
  capo_buffer buf;
  int threads;
  int fd;
}capi;

void *Reader(void* arg);
void *Writer(void* arg);
void* Capo(void* arg);
void rw_init(hash *z);

#endif  // RW_H