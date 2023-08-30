#include "tabella.h"
#include "buffer.h"

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
  str = get(&a->buf);
  while(str!=NULL){    
    //accesso tabella hash
      read_lock(&a->access);    
      tot = conta(str);
      read_unlock(&a->access);
      printf("%s %d \n", str, tot);
     
    //scrittura sul file
      pthread_mutex_lock(a->mutex_fd);
       fprintf(a->file,"%s %d \n", str, tot);       
      pthread_mutex_unlock(a->mutex_fd);
      //fflush(a->file);
      //accesso al buffer
      str= get(&a->buf);
   }
  free(str);
  printf("sono reader pre close file\n");  
  pthread_exit(NULL);
}

/**************************************/


void *Writer(void* arg) {
    rw *a = ( rw *)arg;
    char* str =malloc(Max_sequence_length*sizeof(char));
    //accesso al buffer
    str = get(&a->buf);
    while(str!=NULL){
      //accesso tabella hash
      write_lock(&a->access);
      aggiungi(str);  
      write_unlock(&a->access);
      //accesso al buffer
     str= get(&a->buf);
    } 
  free(str);
  printf("sono writer pre close file\n");
  pthread_exit(NULL);
}

/**************************************/

void* Capo(void* arg) {
  capi *a = ( capi *)arg;
    u_int16_t size;
    ssize_t e;
    char* copy;
    char *str;  
    char* saveprint; 
    
    //lettura dalla named pipe
    while ((e = read(a->fd, &size, sizeof(u_int16_t))) > 0) {
      if (e != sizeof(u_int16_t)) 
          termina("Errore nella lettura della lunghezza\n");
      u_int16_t length = ntohs(size);
      char token[length+1];
      e = read(a->fd, token, length); 
      if (e != length)  
        termina("Errore nella lettura del carattere\n");
      token[length] = '\0'; 
      copy = strdup(token);
     //tokenizzazione stringa 
      str = strtok_r(copy, ".,:; \n\r\t",&saveprint); 
      while (str != NULL) {
        //accesso al buffer 
        put(&a->buf,str);
        str = strtok_r(NULL, ".,:; \n\r\t",&saveprint); 
      } 
      free(copy);   
    }
    //free(saveprint);           
    free(str);    
  
  printf("ho finito\n");
  //manda segnale di terminazione ai threads
  char* fine=NULL;
  for(int i=0;i<a->threads;i++) {
    last_put(&a->buf,fine);
  }
  //chiusura della pipe
  close(a->fd); 
  printf("CHIUSA PIPE\n"); 
  pthread_exit(NULL);
}



/**************************************/
