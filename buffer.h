#ifndef BUFFER_H
#define BUFFER_H

typedef struct {
  //accesso al buffer 
  char **buffer;  
  sem_t *sem_free_slots;
  sem_t *sem_data_items;
  int *index;
}capo_buffer;

typedef struct {
  //accesso al buffer 
  capo_buffer *buf_c;
  pthread_mutex_t *pmutex_buf;
}buffer;

char* get(buffer *a);
void put(capo_buffer *a,char *str);


#endif  // BUFFER_H