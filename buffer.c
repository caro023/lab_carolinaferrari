#include "tabella.h"


char* get(buffer *z) {
    char* str = NULL;
    capo_buffer *a = ( capo_buffer*)z->buf_c;
    //accesso al buffer
      sem_wait(a->sem_data_items);
      pthread_mutex_lock(z->pmutex_buf);
      str = (a->buffer[*(a->index) % PC_buffer_len]);
      *(a->index) +=1;
      pthread_mutex_unlock(z->pmutex_buf);
      sem_post(a->sem_free_slots);
    return str;
  }

void put(capo_buffer *a,char *str) {
    sem_wait(a->sem_free_slots);
    a->buffer[*(a->index) % PC_buffer_len] = str;
    *(a->index) +=1;
    sem_post(a->sem_data_items);
}