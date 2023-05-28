#include "tabella.h"

#define Num_elem 1000000 //dimensione della tabella hash 
#define PC_buffer_len 10// lunghezza dei buffer produttori/consumatori
#define PORT 56515`// porta usata dal server dove `XXXX` sono le ultime quattro cifre del vostro numero di matricola. 
#define Max_sequence_length 2048 //massima lunghezza di una sequenza che viene inviata attraverso un socket o pipe

/*void handler(int s)
{
  printf("Segnale %d ricevuto dal processo %d\n", s, getpid());
}*/
// messaggio errore e stop
void termina(const char *messaggio){
  if(errno!=0) perror(messaggio);
	else fprintf(stderr,"%s\n", messaggio);
  exit(1);
}

/**************************************/

// crea un oggetto di tipo entry
// con chiave s e valore n
  ENTRY *entry(char *s, int n) {
  ENTRY *e = malloc(sizeof(ENTRY));
  if(e==NULL) termina("errore malloc entry 1");
  e->key = strdup(s); // salva copia di s
  e->data = (int *) malloc(sizeof(int));
  if(e->key==NULL || e->data==NULL)
    termina("errore malloc entry 2");
  *((int *)e->data) = n;
  return e;
}

void distruggi_entry(ENTRY *e)
{
  free(e->key); free(e->data); free(e);
}

/**************************************/

  // inserisce gli elementi sulla linea di comando
  void aggiungi (char *s) {
    ENTRY *e = entry(*s, 1);
    ENTRY *r = hsearch(*e,FIND);
    if(r==NULL) { // la entry è nuova
      r = hsearch(*e,ENTER);
      if(r==NULL) termina("errore o tabella piena");
      n++;
    }
    else {
      // la stringa è gia' presente incremento il valore
      assert(strcmp(e->key,r->key)==0);
      int *d = (int *) r->data;
      *d +=1;
      distruggi_entry(e); // questa non la devo memorizzare
    }
  }

/**************************************/

 // ritorna il valore associato alla stringa se presente, sennò 0
  int conta(char *s) {
    ENTRY *e = entry(*s, 1);
    ENTRY *r = hsearch(*e,FIND);
    if(r==NULL) return 0;
    else return (*((int *) r->data));
    distruggi_entry(e);
  }

/**************************************/

// inizializza rw, ne scrittori ne lettori 
void rw_init(rw *z)
{
  z->readers = 0;
  z->writing = 0;
  pthread_cond_init(&z->cond,NULL);
  pthread_mutex_init(&z->mutex,NULL);
  pthread_mutex_init(&z->ordering,NULL);
}

/**************************************/

void pc_init(rw *z)
{
  sem_init(&z->sem_free_slots,0,PC_buffer_len);
  sem_init(&z->sem_data_items,0,0);
}

/**************************************/

// inizio uso da parte di un reader
void read_lock(rw *z)
{
  pthread_mutex_lock(&z->ordering);  // coda di ingresso
  pthread_mutex_lock(&z->mutex);
  while(z->writing>0)
    pthread_cond_wait(&z->cond, &z->mutex);   // attende fine scrittura
  z->readers++;
  pthread_mutex_unlock(&z->ordering); // faccio passare il prossimo se in coda
  pthread_mutex_unlock(&z->mutex);
}

// fine uso da parte di un reader
void read_unlock(rw *z)
{
  assert(z->readers>0);  // ci deve essere almeno un reader (me stesso)
  assert(!z->writing);   // non ci devono essere writer 
  pthread_mutex_lock(&z->mutex);
  z->readers--;                  // cambio di stato       
  if(z->readers==0) 
    pthread_cond_signal(&z->cond); // da segnalare ad un solo writer
  pthread_mutex_unlock(&z->mutex);
}

/**************************************/
  
// inizio uso da parte di writer  
void write_lock(rw *z)
{
  pthread_mutex_lock(&z->ordering);    // coda di ingresso
  pthread_mutex_lock(&z->mutex);
  while(z->writing>0 || z->readers>0)
    // attende fine scrittura o lettura
    pthread_cond_wait(&z->cond, &z->mutex);   
  assert(z->writing==0);
  z->writing++;
  pthread_mutex_unlock(&z->ordering);
  pthread_mutex_unlock(&z->mutex);
}

// fine uso da parte di un writer
void write_unlock(rw *z)
{
  assert(z->writing>0);
  pthread_mutex_lock(&z->mutex);
  z->writing--;               // cambio stato
  // segnala a tutti quelli in attesa 
  pthread_cond_signal(&z->cond);  
  pthread_mutex_unlock(&z->mutex);
}

/**************************************/

/*typedef struct _Foo Foo;

Foo *foo_new();
void foo_do_something(Foo *foo);

struct _Foo {
   int bar;
};

Foo *foo_new() {
    Foo *foo = malloc(sizeof(Foo));
    foo->bar = 0;
    return foo;
}

void foo_do_something(Foo *foo) {
    foo->bar++;
}
*/


