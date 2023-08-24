#include "tabella.h"
#define Num_elem 1000000 //dimensione della tabella hash 
#define PC_buffer_len 10// lunghezza dei buffer produttori/consumatori
#define PORT 56515// porta usata dal server dove `XXXX` sono le ultime quattro cifre del vostro numero di matricola. 
#define Max_sequence_length 2048 //massima lunghezza di una sequenza che viene inviata attraverso un socket o pipe



void* Capo(void* arg) {
  capi *a = ( capi *)arg;
    u_int16_t size;
    ssize_t e;
    char* copy;
    char *str; 
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
      printf(" stringa %s  \n",copy);
      str = strtok_r(copy, ".,:; \n\r\t",&saveprint); 
      while (str != NULL) {
        sem_wait(a->sem_free_slots);
        a->buffer[*(a->index) % PC_buffer_len] = strdup(str);
        *(a->index) +=1;
        sem_post(a->sem_data_items);        
        str = strtok_r(NULL, ".,:; \n\r\t",&saveprint); 
      } 
      free(copy);   
    }    
  
  printf("ho finito");
  char* fine=NULL;
  for(int i=0;i<a->threads;i++) {
    sem_wait(a->sem_free_slots);
    a->buffer[*(a->index)++ % PC_buffer_len] = fine;//strdup(fine);
    sem_post(a->sem_data_items);
  }
  close(a->fd); 
  printf("CHIUSA PIPE"); 
  free(a);
  pthread_exit(NULL);
}

void *Writer(void* arg) {
    rw *a = ( rw *)arg;
    char* str =malloc(Max_sequence_length*sizeof(char));

    //accesso al buffer 
    //do 
    while(str!=NULL){
      sem_wait(a->sem_data_items);
      pthread_mutex_lock(a->pmutex_buf);
      str = (a->buffer[*(a->index) % PC_buffer_len]);
      *(a->index) +=1;
      pthread_mutex_unlock(a->pmutex_buf);
      sem_post(a->sem_free_slots);
      if(str==NULL) break;
      write_lock(&a->access);
      aggiungi(str);  
      write_unlock(&a->access);

    } //while(str!=NULL);
  free(str);
  free(a);
  
  pthread_exit(NULL);
}

int main(int argc, char *argv[]){
// crea tabella hash
  int ht = hcreate(Num_elem);
  if(ht==0 ) termina("Errore creazione HT");


  int w = atoi(argv[2]);
  assert(w>0);

  printf("avviato archivio\n");
  char* wbuffer[10];
  int wpindex=0, wcindex=0;
  pthread_mutex_t wmubuf = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t fdmutex = PTHREAD_MUTEX_INITIALIZER; 
  sem_t sem_free_slots2, sem_data_items2;
  sem_init(&sem_free_slots2,0,PC_buffer_len);
  sem_init(&sem_data_items2,0,0);
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t ordering = PTHREAD_MUTEX_INITIALIZER; 
  pthread_cond_t cond;
  pthread_cond_init(&cond,NULL);

  pthread_t capoWrite; 
   int fd2= open("caposc",O_RDONLY);
  if (fd2 < 0) // se il file non esiste termina con errore
    termina("Errore apertura named pipe");

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

  rw wc[w];    
  pthread_t write[w];

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
   pthread_join(capoWrite, NULL);
  
  for(int i=0;i<w;i++) {
    pthread_join(write[i], NULL);
    printf("ho finito write");
  }
   pthread_mutex_destroy(&mutex);
  pthread_mutex_destroy(&ordering);
  pthread_mutex_destroy(&fdmutex);
  pthread_mutex_destroy(&wmubuf);
  destroy();
  pthread_cond_destroy(&cond); 
  sem_destroy(&sem_free_slots2);  
  sem_destroy(&sem_data_items2);
  free(*wbuffer);

  free(wc);

  printf("terminato archivio\n");
}