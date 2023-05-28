//#include "tabella.h"
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


pthread_t capoWrite;  // Puntatore al thread capo scrittore e lettore 
pthread_t capoRead; 
//static int n=0;
int n=0;
FILE* file;


#define Num_elem 1000000 //dimensione della tabella hash 
#define PC_buffer_len 10// lunghezza dei buffer produttori/consumatori
#define PORT 56515`// porta usata dal server dove `XXXX` sono le ultime quattro cifre del vostro numero di matricola. 
#define Max_sequence_length 2048 //massima lunghezza di una sequenza che viene inviata attraverso un socket o pipe

/*void handler(int s)
{
  printf("Segnale %d ricevuto dal processo %d\n", s, getpid());
}*/
// messaggio errore e stop
void termina(const char *messaggio){
  if(errno!=0) perror(messaggio);
	else fprintf(stderr,"%s\n", messaggio);
  exit(1);
}

/**************************************/

// crea un oggetto di tipo entry
// con chiave s e valore n
  ENTRY *entry(char *s, int n) {
  ENTRY *e = malloc(sizeof(ENTRY));
  if(e==NULL) termina("errore malloc entry 1");
  e->key = strdup(s); // salva copia di s
  e->data = (int *) malloc(sizeof(int));
  if(e->key==NULL || e->data==NULL)
    termina("errore malloc entry 2");
  *((int *)e->data) = n;
  return e;
}

void distruggi_entry(ENTRY *e)
{
  free(e->key); free(e->data); free(e);
}

/**************************************/

  // inserisce gli elementi sulla linea di comando
  void aggiungi (char *s) {
    ENTRY *e = entry(s, 1);
    ENTRY *r = hsearch(*e,FIND);
    if(r==NULL) { // la entry è nuova
      r = hsearch(*e,ENTER);
      if(r==NULL) termina("errore o tabella piena");
      n++;
    }
    else {
      // la stringa è gia' presente incremento il valore
      assert(strcmp(e->key,r->key)==0);
      int *d = (int *) r->data;
      *d +=1;
      distruggi_entry(e); // questa non la devo memorizzare
    }
  }

/**************************************/

 // ritorna il valore associato alla stringa se presente, sennò 0
  int conta(char *s) {
    ENTRY *e = entry(s, 1);
    ENTRY *r = hsearch(*e,FIND);
    if(r==NULL) return 0;
    else return (*((int *) r->data));
    distruggi_entry(e);
  }

/**************************************/

// inizializza rw, ne scrittori ne lettori 
void rw_init(rw *z)
{
  z->readers = 0;
  z->writing = 0;
  pthread_cond_init(&z->cond,NULL);
  pthread_mutex_init(&z->mutex,NULL);
  pthread_mutex_init(&z->ordering,NULL);
}

/**************************************/

void pc_init(rw *z)
{
  sem_init(z->sem_free_slots,0,PC_buffer_len);
  sem_init(z->sem_data_items,0,0);
}

/**************************************/

// inizio uso da parte di un reader
void read_lock(rw *z)
{
  pthread_mutex_lock(&z->ordering);  // coda di ingresso
  pthread_mutex_lock(&z->mutex);
  while(z->writing>0)
    pthread_cond_wait(&z->cond, &z->mutex);   // attende fine scrittura
  z->readers++;
  pthread_mutex_unlock(&z->ordering); // faccio passare il prossimo se in coda
  pthread_mutex_unlock(&z->mutex);
}

// fine uso da parte di un reader
void read_unlock(rw *z)
{
  assert(z->readers>0);  // ci deve essere almeno un reader (me stesso)
  assert(!z->writing);   // non ci devono essere writer 
  pthread_mutex_lock(&z->mutex);
  z->readers--;                  // cambio di stato       
  if(z->readers==0) 
    pthread_cond_signal(&z->cond); // da segnalare ad un solo writer
  pthread_mutex_unlock(&z->mutex);
}

/**************************************/
  
// inizio uso da parte di writer  
void write_lock(rw *z)
{
  pthread_mutex_lock(&z->ordering);    // coda di ingresso
  pthread_mutex_lock(&z->mutex);
  while(z->writing>0 || z->readers>0)
    // attende fine scrittura o lettura
    pthread_cond_wait(&z->cond, &z->mutex);   
  assert(z->writing==0);
  z->writing++;
  pthread_mutex_unlock(&z->ordering);
  pthread_mutex_unlock(&z->mutex);
}

// fine uso da parte di un writer
void write_unlock(rw *z)
{
  assert(z->writing>0);
  pthread_mutex_lock(&z->mutex);
  z->writing--;               // cambio stato
  // segnala a tutti quelli in attesa 
  pthread_cond_signal(&z->cond);  
  pthread_mutex_unlock(&z->mutex);
}


 

void *Reader(void* arg) {
  rw *a = ( rw *)arg;
  printf("ciao 1");
  char* str;
  int tot;
  //accesso al buffer
  do {
      sem_wait(a->sem_data_items);
      pthread_mutex_lock(a->pmutex_buf);
      str = &(a->buffer[*(a->index) % PC_buffer_len]);
      *(a->index) +=1;
      pthread_mutex_unlock(a->pmutex_buf);
      sem_post(a->sem_free_slots);
      //while(stop>0) {

      read_lock(a);    
      //da un buffer prod cons leggono le stringhe e fanno conta
      tot = conta(str);
      read_unlock(a);
     
      pthread_mutex_lock(a->mutex_fd);
      fprintf(file,"%s %d \n", str, tot);
      pthread_mutex_unlock(a->mutex_fd);
  
      free(str);
      //}
   } while(str!=NULL);
  pthread_exit(NULL);
  return NULL;
}

/**************************************/


void *Writer(void* arg) {
  printf("ciao 2");
    rw *a = ( rw *)arg;
    char* str;
    //accesso al buffer 
    do {
      sem_wait(a->sem_data_items);
      pthread_mutex_lock(a->pmutex_buf);
      str = &(a->buffer[*(a->index) % PC_buffer_len]);
      *(a->index) +=1;
      pthread_mutex_unlock(a->pmutex_buf);
      sem_post(a->sem_free_slots);
      write_lock(a);
      aggiungi(str);  
      write_unlock(a);

      free(str);
    } while(str!=NULL);
  pthread_exit(NULL);

  //printf("WRITER%ld TERMINATO\n", id);  
  return NULL;
}

/**************************************/

void* Capo(void* arg) {
  printf("ciao 3");
  capi *a = ( capi *)arg;
   // FILE* fifo;
   // char buffer[256];
  int fd = open(a->pipeName,O_RDONLY);
  if (fd < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe"); 
  
  //while(true) {
    int length;
    ssize_t e;
    while ((e = read(fd, &length, sizeof(int))) > 0) {
      if (e != sizeof(int)) 
            termina("Errore nella lettura della lunghezza\n");
      
      char token[length + 1];
      e = read(fd, token, length); //va messo &token???? NO è gia un puntatore
      if (e != length) 
          termina("Errore nella lettura del carattere\n");
      token[length] = '\0'; //perche \
        //token[length] = '0'; ?????????????????
        //   length++;         ????????????????

      
      char* copy = strdup(token);
      char* str = strtok(copy, ".,:; \n\r\t");
      while (str != NULL) {
        sem_wait(a->sem_free_slots);
        a->buffer[*(a->index)++ % PC_buffer_len] = *strdup(str); //rimuovere * ???
        sem_post(a->sem_data_items);
        str = strtok(NULL, ".,:; \n\r\t"); //???????????????????
      }
      free(copy);
    }
  close(fd);       

  for(int i=0;i<a->threads;i++) {
    sem_wait(a->sem_free_slots);
    a->buffer[*(a->index)++ % PC_buffer_len] = (char *)NULL;
    sem_post(a->sem_data_items);
  }
  
  pthread_exit(NULL);
}


/**************************************/


void *gbody(void *arg) {
  // recupera argomento passato al thread
  //dati *d = (dati *) arg;
  // si mette in attesa di tutti i segnali
  sigset_t mask;
  sigfillset(&mask);
  //sigdelset(&mask,SIGINT); //?????
  int s;
  while(true) {
    int e = sigwait(&mask,&s);
    if(e!=0) perror("Errore sigwait");//
    printf("Thread gestore svegliato dal segnale %d\n",s);//
    if(s==SIGINT) {
      fprintf(stderr,"%d\n",n);
    } 
    if(s==SIGTERM) {
      pthread_join(capoWrite, NULL);
      pthread_join(capoRead, NULL);
      fprintf(stdout,"%d\n",n);
      hdestroy();
      exit(0);
    } 

  }
  return NULL;
}

/**************************************/


int main(int argc, char *argv[])
{ 
  if(argc!=3) {
    printf("Uso:\n\t%s num_reader, num_writer \n",argv[0]);
    exit(1);
  }

  printf("ciao");

  // crea tabella hash
  int ht = hcreate(Num_elem);
  if(ht==0 ) termina("Errore creazione HT");

  //creo file lettori.log
  file = fopen("lettori.log", "w");
  if (file == NULL) 
        termina("Errore apertura file di log");

  printf("ciao");

  int r = atoi(argv[1]);
  int w = atoi(argv[2]);
  assert(r>0);
  assert(w>0);
  printf("ciao");

  char* rbuffer[PC_buffer_len];
  char* wbuffer[PC_buffer_len];
  int rpindex=0, rcindex=0; //rp,wp index ai theread capi
  int wpindex=0, wcindex=0;
  pthread_mutex_t rmupbuf = PTHREAD_MUTEX_INITIALIZER; //capo read
  pthread_mutex_t rmucbuf = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t wmupbuf = PTHREAD_MUTEX_INITIALIZER; // capo write
  pthread_mutex_t wmucbuf = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t fdmutex = PTHREAD_MUTEX_INITIALIZER; //mutex per accesso a file lettori.log
  sem_t sem_free_slots, sem_data_items;
  sem_init(&sem_free_slots,0,PC_buffer_len);
  sem_init(&sem_data_items,0,0);
  
  printf("ciao");

  //thread capo lettore
  capi cr;
  printf("ciao");
 // pthread_t capoRead;
  //pc_init(&cr);
  cr.buffer = rbuffer[0]; //assegna puntatore al primo elemento dell'array 
  //cr.pmutex_buf = &rmupbuf;
  printf("ciao");
  cr.index = &rpindex;
  cr.sem_data_items = &sem_data_items; //funzione pc_init
  cr.sem_free_slots = &sem_free_slots;
  cr.threads = r;
  cr.pipeName = "capolet";
  printf("ciao 4");
  if((pthread_create(&capoRead,NULL,Capo,NULL))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
  }
  printf("ciao 6");

  //thread capo scrittore 
  capi cw;
  //pthread_t capoWrite;
  // pc_init(&cw);
  cw.sem_data_items = &sem_data_items; //funzione pc_init
  cw.sem_free_slots = &sem_free_slots;
  cw.buffer = wbuffer[0];
  //cw.pmutex_buf = &wmupbuf;
  cw.index = &wpindex;
  cw.threads = w;
  cw.pipeName = "caposc";
  if((pthread_create(&capoWrite,NULL,Capo,NULL))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
  }

  printf("ciao 7");

  //thread scrittori e lettori
  rw rc[r];
  rw wc[w];
  pthread_t read[r];    
  pthread_t write[w];

  // creo tutti gli scrittori
  for(int i=0;i<r;i++) {
   // faccio partire il thread i
    rw_init(&rc[i]);
    pc_init(&rc[i]);
    rc[i].index = &rcindex;
    rc[i].buffer = rbuffer[0];  
    rc[i].pmutex_buf = &rmucbuf;
    rc[i].mutex_fd = &fdmutex;
    //rc[i].sem_data_items = &sem_data_items;
    //rc[i].sem_free_slots = &sem_free_slots;
    if((pthread_create(&read[i],NULL,Reader,rc+i))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
    }
  }
  // creo tutti i letto
  for(int i=0;i<w;i++) {
   // faccio partire il thread i
    rw_init(&wc[i]);
    pc_init(&wc[i]);
    wc[i].index = &wcindex;
    wc[i].buffer = wbuffer[0];  
    wc[i].pmutex_buf = &wmucbuf;
    if((pthread_create(&write[i],NULL,Writer,wc+i))!=0){
      fprintf(stderr, "pthread_create Writer failed\n");
      return -1;
    }
  }

  for(int i=0;i<r;i++) {
    pthread_join(read[i], NULL);
  }
  fclose(file);

  for(int i=0;i<w;i++) {
    pthread_join(write[i], NULL);
  }

  sigset_t mask;
  sigfillset(&mask);  // insieme di tutti i segnali
 // sigdelset(&mask,SIGQUIT); // elimino sigquit da mask
  pthread_sigmask(SIG_BLOCK,&mask,NULL); // blocco tutto 
  
  //dati d;
  //d.tot_segnali = 0;
  //d.continua = true;

  pthread_t gestore;
  pthread_create(&gestore, NULL, &gbody,NULL);

  pthread_join(gestore, NULL);

  //pthread_join(capoWrite, NULL);
  //pthread_join(capoRead, NULL);
  
  return 0;
}



 