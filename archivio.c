#include "tabella.h"
#include "rw.h"


#define Num_elem 1000000 //dimensione della tabella hash 
#define PC_buffer_len 10// lunghezza dei buffer produttori/consumatori
#define PORT 56515// porta usata dal server dove `XXXX` sono le ultime quattro cifre del vostro numero di matricola. 
#define Max_sequence_length 2048 //massima lunghezza di una sequenza che viene inviata attraverso un socket o pipe
 

/**************************************/


void *gbody(void *arg) {
  // si mette in attesa di tutti i segnali
   sigset_t *mask = (sigset_t *)arg;
 
  int s;
  while(true) {
    printf("dentro il while del segnale\n");
    int e = sigwait(mask,&s);
    if(e!=0) perror("Errore sigwait");
    printf("Thread gestore svegliato dal segnale %d\n",s);
    
    if(s==SIGINT) {
      fprintf(stderr,"numero elementi nella tabella %d\n",size());
    }    

    if(s==SIGTERM) {  
      //close(fd1);
      //close(fd2);
      fprintf(stdout,"numero elementi nella tabella %d\n",size());
     // printf("numero elementi nella tabella %d\n",size());
      hdestroy();
     // exit(0);
      pthread_exit(NULL);
    } 
  }
  
  return NULL;
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

  // Puntatore al thread capo scrittore e lettore 
  pthread_t capoWrite; 
  pthread_t capoRead; 

  int fd1= open("capolet",O_RDONLY);
  if (fd1 < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe");
  int fd2= open("caposc",O_RDONLY);
  if (fd2 < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe");

  //thread capo lettore
  capi cr;
  cr.buffer = rbuffer; 
  cr.index = &rpindex;
  cr.sem_data_items = &sem_data_items1; 
  cr.sem_free_slots = &sem_free_slots1;
  cr.threads = r;//mettere numero di reader
  cr.fd = fd1;
  if((pthread_create(&capoRead,NULL,Capo,&cr))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
  }
  
  
  //thread capo scrittore 
  capi cw;
  cw.sem_data_items = &sem_data_items2; 
  cw.sem_free_slots = &sem_free_slots2;
  cw.buffer = wbuffer;
  cw.index = &wpindex;
  cw.threads = w;
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

  

  //thread scrittori e lettori
  rw rc[r];
  rw wc[w];
  pthread_t read[r];    
  pthread_t write[w];

  // creo tutti i lettori
  for(int i=0;i<r;i++) {
   // faccio partire il thread i
    rc[i].access = init;
    rc[i].sem_data_items = &sem_data_items1; //funzione pc_init
    rc[i].sem_free_slots = &sem_free_slots1;
    rc[i].index = &rcindex;
    rc[i].buffer = rbuffer;  
    rc[i].pmutex_buf = &rmubuf;
    rc[i].mutex_fd = &fdmutex;
    if((pthread_create(&read[i],NULL,Reader,rc+i))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
    }
  }
  // creo tutti gli scrittori
  for(int i=0;i<w;i++) {
   // faccio partire il thread i
    wc[i].access = init;
    wc[i].sem_data_items = &sem_data_items2; //funzione pc_init
    wc[i].sem_free_slots = &sem_free_slots2;
    wc[i].index = &wcindex;
    wc[i].buffer = wbuffer;  
    wc[i].pmutex_buf = &wmubuf;
    if((pthread_create(&write[i],NULL,Writer,wc+i))!=0){
      fprintf(stderr, "pthread_create Writer failed\n");
      return -1;
    }
  }

  //creo la maschera per i segnali
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
 
  //thread dei segnali
  pthread_t gestore;
  pthread_create(&gestore, NULL, &gbody,&mask);

  //join dei threads

  pthread_join(gestore, NULL);
  pthread_join(capoWrite, NULL);
  pthread_join(capoRead, NULL);

  for(int i=0;i<r;i++) {
    pthread_join(read[i], NULL);
    printf("ho finito read");
  }

  for(int i=0;i<w;i++) {
    pthread_join(write[i], NULL);
    printf("ho finito write");
  }

  //libero la memoria
  pthread_mutex_destroy(&mutex);
  pthread_mutex_destroy(&ordering);
  pthread_mutex_destroy(&fdmutex);
  pthread_mutex_destroy(&rmubuf);
  pthread_mutex_destroy(&wmubuf);
  destroy();
  pthread_cond_destroy(&cond);
  sem_destroy(&sem_free_slots1);  
  sem_destroy(&sem_free_slots2); 
  sem_destroy(&sem_data_items1);  
  sem_destroy(&sem_data_items2);
  free(*rbuffer);
  free(*wbuffer);

  free(rc);
  free(wc);
}



 