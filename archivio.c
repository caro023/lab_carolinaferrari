#include "tabella.h"


#define Num_elem 1000000 //dimensione della tabella hash 
#define PC_buffer_len 10// lunghezza dei buffer produttori/consumatori
#define PORT 56515// porta usata dal server dove `XXXX` sono le ultime quattro cifre del vostro numero di matricola. 
#define Max_sequence_length 2048 //massima lunghezza di una sequenza che viene inviata attraverso un socket o pipe


//pthread_t capoWrite;  // Puntatore al thread capo scrittore e lettore 
//pthread_t capoRead; 
//static int n=0;
FILE* file;
//int fd1;
//int fd2;

 

void *Reader(void* arg) {
  rw *a = ( rw *)arg;
  char* str =malloc(Max_sequence_length*sizeof(char));
  int tot;
  //accesso al buffer
  do {
      sem_wait(a->sem_data_items);
      pthread_mutex_lock(a->pmutex_buf);
      str = (a->buffer[*(a->index) % PC_buffer_len]);
      *(a->index) +=1;
      pthread_mutex_unlock(a->pmutex_buf);
      sem_post(a->sem_free_slots);

      if(str==NULL) break;
    
      read_lock(a);    
      tot = conta(str);
      read_unlock(a);
      printf("%s %d \n", str, tot);
     
      pthread_mutex_lock(a->mutex_fd);
       int f = fprintf(file,"%s %d \n", str, tot);
       if(f<0) printf("non ho scritto\n");
       
      pthread_mutex_unlock(a->mutex_fd);
      fflush(file);
   } while(str!=NULL);
  free(str);
  free(a);
  pthread_exit(NULL);
  return NULL;
}

/**************************************/


void *Writer(void* arg) {
    rw *a = ( rw *)arg;
    char* str;//=malloc(Max_sequence_length*sizeof(char));
    //accesso al buffer 
    do {
      sem_wait(a->sem_data_items);
      pthread_mutex_lock(a->pmutex_buf);
      str = (a->buffer[*(a->index) % PC_buffer_len]);
      *(a->index) +=1;
      pthread_mutex_unlock(a->pmutex_buf);
      sem_post(a->sem_free_slots);
      if(str==NULL) break;
      write_lock(a);
      aggiungi(str);  
      write_unlock(a);

     // free(*str);
    } while(str!=NULL);
  free(str);
  free(a);
  pthread_exit(NULL);
}

/**************************************/

void* Capo(void* arg) {
  capi *a = ( capi *)arg;
 /* int fd= open(a->pipeName,O_RDONLY);  
  if (fd < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe");*/
    u_int16_t size;
    ssize_t e;
    char* copy;
    char *str; //= malloc(Max_sequence_length * sizeof(char)); 
    char* saveprint; 

    while ((e = read(a->fd, &size, sizeof(u_int16_t))) > 0) {
      if (e != (int) sizeof(u_int16_t)) 
       termina("Errore nella lettura della lunghezza\n");
      u_int16_t length = ntohs(size);
         
      char token[length+1];
      e = read(a->fd, token, length); //va messo &token???? NO è gia un puntatore
      if ((int)e != length)  
        termina("Errore nella lettura del carattere\n");
      token[length] = '\0'; 
      copy = strdup(token);
     // free(token);
      printf(" stringa %s  \n",copy);
     // str = malloc(Max_sequence_length * sizeof(char));  
      str = strtok_r(copy, ".,:; \n\r\t",&saveprint); 
      while (str != NULL) {
        sem_wait(a->sem_free_slots);
        a->buffer[*(a->index) % PC_buffer_len] = strdup(str);
      //  printf("%s stringa scritta\n",a->buffer[*(a->index) % PC_buffer_len]);
        *(a->index) +=1;
        sem_post(a->sem_data_items);        
        str = strtok_r(NULL, ".,:; \n\r\t",&saveprint); 
      }     
    }
    free(saveprint);           
    free(str);
    free(copy);
       
  char* fine=NULL;
  for(int i=0;i<a->threads;i++) {
    sem_wait(a->sem_free_slots);
    a->buffer[*(a->index)++ % PC_buffer_len] = fine;
    sem_post(a->sem_data_items);
  }
  close(a->fd); 
  printf("CHIUSA PIPE"); 
  free(a);
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
    if(e!=0) perror("Errore sigwait");
    printf("Thread gestore svegliato dal segnale %d\n",s);

    if(s==SIGINT) {
      fprintf(stderr,"numero elementi nella tabella %d\n",size());
    }
    

    if(s==SIGTERM) {  
      //close(fd1);
      //close(fd2);
     // pthread_join(capoWrite, NULL);
     // pthread_join(capoRead, NULL);
      fprintf(stdout,"numero elementi nella tabella %d\n",size());
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
  int rpindex=0, rcindex=0; //rp,wp index ai theread capi
  int wpindex=0, wcindex=0;
  pthread_mutex_t rmubuf = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t wmubuf = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t fdmutex = PTHREAD_MUTEX_INITIALIZER; //mutex per accesso a file lettori.log
  sem_t sem_free_slots1, sem_data_items1;
  sem_init(&sem_free_slots1,0,10);//mettere al posto di 256 pc_buf_len
  sem_init(&sem_data_items1,0,0);
  sem_t sem_free_slots2, sem_data_items2;
  sem_init(&sem_free_slots2,0,10);
  sem_init(&sem_data_items2,0,0);
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t ordering = PTHREAD_MUTEX_INITIALIZER; 
  pthread_cond_t cond;
  pthread_cond_init(&cond,NULL);

  pthread_t capoWrite;  // Puntatore al thread capo scrittore e lettore 
  pthread_t capoRead; 

  int fd1= open("capolet",O_RDONLY);
  if (fd1 < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe");
  int fd2= open("caposc",O_RDONLY);
  if (fd2 < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe");
  //thread capo lettore
  capi cr;
 // pthread_t capoRead;
  //pc_init(&cr);
  cr.buffer = rbuffer; 
  //cr.pmutex_buf = rmupbuf;
  cr.index = &rpindex;
  cr.sem_data_items = &sem_data_items1; //funzione pc_init
  cr.sem_free_slots = &sem_free_slots1;
  cr.threads = r;//mettere numero di reader
  //cr.pipeName = "capolet";
  cr.fd = fd1;
  if((pthread_create(&capoRead,NULL,Capo,&cr))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
  }
  
  
  //thread capo scrittore 
  capi cw;
  //pthread_t capoWrite;
  // pc_init(&cw);
  cw.sem_data_items = &sem_data_items2; //funzione pc_init
  cw.sem_free_slots = &sem_free_slots2;
  cw.buffer = wbuffer;
  //cw.pmutex_buf = &wmupbuf;
  cw.index = &wpindex;
  cw.threads = w;
  //cw.pipeName = "caposc";
  cw.fd=fd2;
  if((pthread_create(&capoWrite,NULL,Capo,&cw))!=0){
      fprintf(stderr, "pthread_create Reader failed\n");
      return -1;
  }


  //thread scrittori e lettori
  rw rc[r];
  rw wc[w];
  pthread_t read[r];    
  pthread_t write[w];

  // creo tutti gli scrittori
  for(int i=0;i<r;i++) {
   // faccio partire il thread i
    rc[i].readers = 0;
    rc[i].writing = 0;
    rc[i].cond = &cond;
    rc[i].mutex = &mutex;
    rc[i].ordering = &ordering;
   // rw_init(&rc[i]);
   // pc_init(&rc[i]);
    rc[i].sem_data_items = &sem_data_items1; //funzione pc_init
    rc[i].sem_free_slots = &sem_free_slots1;
    rc[i].index = &rcindex;
    rc[i].buffer = rbuffer;  
    rc[i].pmutex_buf = &rmubuf;
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
   // rw_init(&wc[i]);
   // pc_init(&wc[i]);
    wc[i].readers = 0;
    wc[i].writing = 0;
    wc[i].cond = &cond;
    wc[i].mutex = &mutex;
    wc[i].ordering = &ordering;
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
  sigset_t mask;
  sigfillset(&mask);  // insieme di tutti i segnali
 // sigdelset(&mask,SIGQUIT); // elimino sigquit da mask
  pthread_sigmask(SIG_BLOCK,&mask,NULL); // blocco tutto 
 
  pthread_t gestore;
  pthread_create(&gestore, NULL, &gbody,NULL);

  pthread_join(gestore, NULL);

  pthread_join(capoWrite, NULL);
   pthread_join(capoRead, NULL);

  for(int i=0;i<r;i++) {
    pthread_join(read[i], NULL);
    printf("ho finito read");
  }
  fclose(file);

  for(int i=0;i<w;i++) {
    pthread_join(write[i], NULL);
    printf("ho finito read");
  }
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



 