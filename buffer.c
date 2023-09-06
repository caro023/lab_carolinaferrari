#include "buffer.h"

char* get(buffer *z) {
    char* str = NULL;
    capo_buffer *a = ( capo_buffer*)z->buf_c;
    //accesso al buffer
    xsem_wait(a->sem_data_items,QUI);
    pthread_mutex_lock(z->pmutex_buf);
    str = (a->buffer[*(a->index) % PC_buffer_len]);
    *(a->index) +=1;
    pthread_mutex_unlock(z->pmutex_buf);
    xsem_post(a->sem_free_slots,QUI);
    return str;
  }

void put(capo_buffer *a,char *str) {
    xsem_wait(a->sem_free_slots,QUI);
    a->buffer[*(a->index) % PC_buffer_len] = str;
    *(a->index) +=1;
    xsem_post(a->sem_data_items,QUI);
}