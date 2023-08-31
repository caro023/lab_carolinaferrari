#include "tabella.h"
#include "rw.h"
#include "buffer.h"


#define Num_elem 1000000 //dimensione della tabella hash 
#define PC_buffer_len 10// lunghezza dei buffer produttori/consumatori

FILE *file;
  // Puntatore al thread capo scrittore e lettore 
  pthread_t capoWrite; 
  pthread_t capoRead; 

/**************************************/


void *gbody(void *arg) {
  // si mette in attesa di tutti i segnali
   sigset_t *mask = (sigset_t *)arg;
 
  int s;
  while(true) {
    int e = sigwait(mask,&s);
    if(e!=0) perror("Errore sigwait");
    printf("Thread gestore svegliato dal segnale %d\n",s);
    
    if(s==SIGINT) {
      fprintf(stderr,"numero elementi nella tabella %d\n",size());
    }    

    if(s==SIGTERM) {  
      pthread_join(capoWrite, NULL);
      pthread_join(capoRead, NULL);
      fprintf(stdout,"numero elementi nella tabella %d\n",size());
      hdestroy();
      pthread_exit(NULL);
    } 
  }  
 // return NULL;
}

/**************************************/

int main(int argc, char *argv[])
{ 
  if(argc!=3) {
    printf("errore parametri");
    exit(1);
  }

  // crea tabella hash
  int ht = hcreate(Num_elem);
  if(ht==0 ) termina("Errore creazione HT");

  //creo la maschera per i segnali
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
 
  //thread dei segnali
  pthread_t gestore;
  pthread_create(&gestore, NULL, &gbody,&mask);

  //creo file lettori.log
  file = fopen("lettori.log", "w");
  if (file == NULL) 
      termina("Errore apertura file di log");  

  int r = atoi(argv[1]);
  int w = atoi(argv[2]);
  assert(r>0);
  assert(w>0);


  char* rbuffer[10];
  char* wbuffer[10];
  //index per i thread capi
  int rpindex=0, rcindex=0; 
  //index per lettori scrittori
  int wpindex=0, wcindex=0;
  //mutex per accesso al buffer
  pthread_mutex_t rmubuf = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t wmubuf = PTHREAD_MUTEX_INITIALIZER;
  //mutex per accesso a file lettori.log
  pthread_mutex_t fdmutex = PTHREAD_MUTEX_INITIALIZER; 
  //semafori per accesso al buffer
  sem_t sem_free_slots1, sem_data_items1;
  sem_init(&sem_free_slots1,0,PC_buffer_len);
  sem_init(&sem_data_items1,0,0);
  sem_t sem_free_slots2, sem_data_items2;
  sem_init(&sem_free_slots2,0,PC_buffer_len);
  sem_init(&sem_data_items2,0,0);
  //variabili per accesso tabella hash
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t ordering = PTHREAD_MUTEX_INITIALIZER; 
  pthread_cond_t cond;
  pthread_cond_init(&cond,NULL);


  //apertura delle named pipe in lettura
  int fd1= open("capolet",O_RDONLY);
  if (fd1 < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe");
  int fd2= open("caposc",O_RDONLY);
  if (fd2 < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe");

  capo_buffer read_c;
  read_c.buffer = rbuffer; 
  read_c.index = &rpindex;
  read_c.sem_data_items = &sem_data_items1; 
  read_c.sem_free_slots = &sem_free_slots1;

  capo_buffer write_c;
  write_c.sem_data_items = &sem_data_items2; 
  write_c.sem_free_slots = &sem_free_slots2;
  write_c.buffer = wbuffer;
  write_c.index = &wpindex;


  //thread capo lettore
  capi cr;
  cr.buf = read_c;
  cr.threads = r;//numero di reader
  cr.fd = fd1;
  if((pthread_create(&capoRead,NULL,Capo,&cr))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
  }  
  
  //thread capo scrittore 
  capi cw;
  cw.buf = write_c;
  cw.threads = w;//numero di writer
  cw.fd=fd2;
  if((pthread_create(&capoWrite,NULL,Capo,&cw))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
  }

  hash init;
  init.readers = 0;
  init.writing = 0;
  init.cond = &cond;
  init.mutex = &mutex;
  init.ordering = &ordering;

  write_c.index=&wcindex;
  buffer buffer_w;
  buffer_w.buf_c = &write_c;
  buffer_w.pmutex_buf = &wmubuf;

  read_c.index = &rcindex;  
  buffer buffer_r;
  buffer_r.buf_c = &read_c;
  buffer_r.pmutex_buf = &rmubuf;

  //thread scrittori e lettori
  rw rc[r];
  rw wc[w];
  pthread_t read[r];    
  pthread_t write[w];

  // creo tutti i lettori
  for(int i=0;i<r;i++) {
   // faccio partire il thread i
    rc[i].access = init;
    rc[i].buf = buffer_r;
    rc[i].mutex_fd = &fdmutex;
    rc[i].file = file;
    if((pthread_create(&read[i],NULL,Reader,rc+i))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
    }
  }
  // creo tutti gli scrittori
  for(int i=0;i<w;i++) {
   // faccio partire il thread i
    wc[i].access = init;
    wc[i].buf = buffer_w;
    if((pthread_create(&write[i],NULL,Writer,wc+i))!=0){
      fprintf(stderr, "pthread_create Writer failed\n");
      return -1;
    }
  }


  //join del thread gestore
  pthread_join(gestore, NULL);

  //chiusura del file
  fclose(file);

  //join reader e writer
  for(int i=0;i<w;i++) {
    pthread_join(write[i], NULL);
  }

  for(int i=0;i<r;i++) {
    pthread_join(read[i], NULL);
  }  

  //libero la memoria
  pthread_mutex_destroy(&mutex);
  pthread_mutex_destroy(&ordering);
  pthread_mutex_destroy(&fdmutex);
  pthread_mutex_destroy(&rmubuf);
  pthread_mutex_destroy(&wmubuf);
  pthread_cond_destroy(&cond);
  sem_destroy(&sem_free_slots1);  
  sem_destroy(&sem_free_slots2); 
  sem_destroy(&sem_data_items1);  
  sem_destroy(&sem_data_items2);
  free(*rbuffer);
  free(*wbuffer);
}



 