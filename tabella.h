#ifndef TABELLA_H
#define TABELLA_H

#include "xerrori.h"
#include "buffer.h"


typedef struct{
  //servono per paradigma lettori scrittori
  int readers;
  int writing;
  pthread_cond_t *cond;   // condition variable
  pthread_mutex_t *mutex; // mutex associato alla condition variable
  pthread_mutex_t *ordering; //serve per dare fairness
}hash;


ENTRY *entry(char *s, int n);
void distruggi_entry(ENTRY *e);

void aggiungi (char *s);
int conta(char *s);
int size(void);


void read_lock(hash *z);
void read_unlock(hash *z);
void write_lock(hash *z);
void write_unlock(hash *z);
#endif  // TABELLA_H
