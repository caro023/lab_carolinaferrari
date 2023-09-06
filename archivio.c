#include "rw.h"

#define Num_elem 1000000 //dimensione della tabella hash 

FILE *file;
// Puntatore al thread capo scrittore e lettore 
pthread_t capoWrite; 
pthread_t capoRead; 


void *gbody(void *arg) {
  // si mette in attesa di tutti i segnali
  sigset_t mask;
  sigfillset(&mask);
  int s;
  //attende l'arrivo di un segnale 
  while(true) {
    int e = sigwait(&mask,&s);
    if(e!=0) perror("Errore sigwait");
    printf("Thread gestore svegliato dal segnale %d\n",s);
    
    if(s==SIGINT) {
      fprintf(stderr,"numero elementi nella tabella %d\n",size());
    }    

    if(s==SIGTERM) {  
      xpthread_join(capoWrite, NULL,QUI);
      xpthread_join(capoRead, NULL,QUI);
      fprintf(stdout,"numero elementi nella tabella %d\n",size());
      hdestroy();
      pthread_exit(NULL);
    } 
  }  
}



int main(int argc, char *argv[]) { 

  if(argc!=3) {
    printf("errore parametri");
    exit(1);
  }

  // crea tabella hash
  int ht = hcreate(Num_elem);
  if(ht==0 ) termina("Errore creazione tabella hash");

  //creo la maschera per i segnali
  sigset_t mask;
  sigfillset(&mask);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
 
  //thread dei segnali
  pthread_t gestore;
  xpthread_create(&gestore, NULL, &gbody,NULL,QUI);

  //creo file lettori.log
  file = xfopen("lettori.log", "w", QUI);
  if (file == NULL) 
      termina("Errore apertura file di log");  

  //numero thread lettori e scrittori
  int r = atoi(argv[1]);
  int w = atoi(argv[2]);
  assert(r>0);
  assert(w>0);

  
  //creazione buffer lettori e scrittori
  char* rbuffer[10];
  char* wbuffer[10];
  //index per i thread capi
  int rpindex=0, rcindex=0; 
  //index per lettori scrittori
  int wpindex=0, wcindex=0;
  //mutex per accesso al buffer
  pthread_mutex_t rmubuf = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t wmubuf = PTHREAD_MUTEX_INITIALIZER;
  //mutex per accesso al file lettori.log
  pthread_mutex_t fdmutex = PTHREAD_MUTEX_INITIALIZER; 
  //semafori per accesso al buffer
  sem_t sem_free_slots1, sem_data_items1;
  xsem_init(&sem_free_slots1,0,PC_buffer_len,QUI);
  xsem_init(&sem_data_items1,0,0,QUI);
  sem_t sem_free_slots2, sem_data_items2;
  xsem_init(&sem_free_slots2,0,PC_buffer_len,QUI);
  xsem_init(&sem_data_items2,0,0,QUI);
  //variabili per accesso alla tabella hash
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t ordering = PTHREAD_MUTEX_INITIALIZER; 
  pthread_cond_t cond;
  xpthread_cond_init(&cond,NULL,QUI);


  //apertura delle named pipe in lettura
  int fd1= open("capolet",O_RDONLY);
  if (fd1 < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe");
  int fd2= open("caposc",O_RDONLY);
  if (fd2 < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe");

  //variabili di accesso al buffer per capo lettore
  capo_buffer read_c;
  read_c.buffer = rbuffer; 
  read_c.index = &rpindex;
  read_c.sem_data_items = &sem_data_items1; 
  read_c.sem_free_slots = &sem_free_slots1;

  //variabili di accesso al buffer per capo scrittore
  capo_buffer write_c;
  write_c.sem_data_items = &sem_data_items2; 
  write_c.sem_free_slots = &sem_free_slots2;
  write_c.buffer = wbuffer;
  write_c.index = &wpindex;


  //thread capo lettore
  capi cr;
  cr.buf = read_c;
  cr.threads = r;//numero di lettori
  cr.fd = fd1;
  if((xpthread_create(&capoRead,NULL,Capo,&cr,QUI))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
  }  
  
  //thread capo scrittore 
  capi cw;
  cw.buf = write_c;
  cw.threads = w;//numero di scrittori
  cw.fd=fd2;
  if((xpthread_create(&capoWrite,NULL,Capo,&cw,QUI))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
  }

  //inzializzo variabili per paradigma lettori scrittori
  hash init;
  init.readers = 0;
  init.writing = 0;
  init.cond = &cond;
  init.mutex = &mutex;
  init.ordering = &ordering;

  //variabili di accesso al buffer per gli scrittori
  write_c.index=&wcindex;
  buffer buffer_w;
  buffer_w.buf_c = &write_c;
  buffer_w.pmutex_buf = &wmubuf;

  //variabili di l'accesso al buffer per i lettori
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
    // faccio partire il thread lettore i-esimo
    rc[i].access = init;
    rc[i].buf = buffer_r;
    rc[i].mutex_fd = &fdmutex;
    rc[i].file = file;
    if((xpthread_create(&read[i],NULL,Reader,rc+i,QUI))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
    }
  }
  // creo tutti gli scrittori
  for(int i=0;i<w;i++) {
    // faccio partire il thread scrittore i-esimo
    wc[i].access = init;
    wc[i].buf = buffer_w;
    if((xpthread_create(&write[i],NULL,Writer,wc+i,QUI))!=0){
      fprintf(stderr, "pthread_create Writer failed\n");
      return -1;
    }
  }


  //join del thread gestore
  xpthread_join(gestore, NULL,QUI);  

  //join lettori e scrittori
  for(int i=0;i<w;i++) {
    xpthread_join(write[i], NULL,QUI);
  }

  for(int i=0;i<r;i++) {
    xpthread_join(read[i], NULL,QUI);
  }  
  
  //chiusura del file
  fclose(file);

  //liberazione della memoria
  xpthread_mutex_destroy(&mutex,QUI);
  xpthread_mutex_destroy(&ordering,QUI);
  xpthread_mutex_destroy(&fdmutex,QUI);
  xpthread_mutex_destroy(&rmubuf,QUI);
  xpthread_mutex_destroy(&wmubuf,QUI);
  xpthread_cond_destroy(&cond,QUI);
  xsem_destroy(&sem_free_slots1,QUI);  
  xsem_destroy(&sem_free_slots2,QUI); 
  xsem_destroy(&sem_data_items1,QUI); 
  xsem_destroy(&sem_data_items2,QUI);
  free(*rbuffer);
  free(*wbuffer);
}



 