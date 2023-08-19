#include "tabella.h"


#define Num_elem 1000000 //dimensione della tabella hash 
#define PC_buffer_len 10// lunghezza dei buffer produttori/consumatori
#define PORT 56515// porta usata dal server dove `XXXX` sono le ultime quattro cifre del vostro numero di matricola. 
#define Max_sequence_length 2048 //massima lunghezza di una sequenza che viene inviata attraverso un socket o pipe

FILE* file;
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
    
      read_lock(&a->access);    
      tot = conta(str);
      read_unlock(&a->access);
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
      //creo file lettori.log
    file = fopen("lettori.log", "w");
    if (file == NULL) 
        termina("Errore apertura file di log");
    //accesso al buffer 
    do {
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

     // free(*str);
    } while(str!=NULL);
  free(str);
  free(a);
  fclose(file);
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
  
  printf("ho finito");
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