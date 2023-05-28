#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>    // permette di usare scanf printf etc ...
#include <stdlib.h>   // conversioni stringa/numero exit() etc ...
#include <stdbool.h>  // gestisce tipo bool (variabili booleane)
#include <assert.h>   // permette di usare la funzione assert
#include <string.h>   // confronto/copia/etc di stringhe
#include <errno.h>
#include <search.h>
#include <signal.h>
#include <unistd.h>  // per sleep 
#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>  
#include <sys/stat.h>  
#include <sys/types.h>

typedef struct {
  //servono per paradigma lettori scrittori
  int readers;
  int writing;
  pthread_cond_t cond;   // condition variable
  pthread_mutex_t mutex; // mutex associato alla condition variable
  pthread_mutex_t ordering; //serve per dare fairness
  //accesso al buffer 
  char *buffer;
  pthread_mutex_t *pmutex_buf;
  sem_t *sem_free_slots;
  sem_t *sem_data_items;
  int *index;
  //per lettori un mutex per il file 
  pthread_mutex_t *mutex_fd;
}rw;

typedef struct {
  //accesso al buffer 
  char *buffer;
  //pthread_mutex_t *pmutex_buf;
  sem_t *sem_free_slots;
  sem_t *sem_data_items;
  int *index;
  int threads;
  char* pipeName;
}capi;


#define Num_elem 1000000 //dimensione della tabella hash 
#define PC_buffer_len 10// lunghezza dei buffer produttori/consumatori
#define PORT 56515`// porta usata dal server dove `XXXX` sono le ultime quattro cifre del vostro numero di matricola. 
#define Max_sequence_length 2048 //massima lunghezza di una sequenza che viene inviata attraverso un socket o pipe

void termina(const char *s); 

ENTRY *entry(char *s, int n);
void distruggi_entry(ENTRY *e);

void aggiungi (char *s);
int conta(char *s);




//typedef struct dati *dati;
//typedef struct rw *rw;
//typedef struct capi *capi;

void rw_init(rw *z);
void pc_init(rw *z);

void read_lock(rw *z);
void read_unlock(rw *z);
void write_lock(rw *z);
void write_unlock(rw *z);

