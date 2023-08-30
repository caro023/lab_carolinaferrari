
#include "tabella.h"

#define Num_elem 1000000 //dimensione della tabella hash 
#define PC_buffer_len 10// lunghezza dei buffer produttori/consumatori
#define PORT 56515`// porta usata dal server dove `XXXX` sono le ultime quattro cifre del vostro numero di matricola. 
#define Max_sequence_length 2048 //massima lunghezza di una sequenza che viene inviata attraverso un socket o pipe

//variabile globale per il numero di elementi nella tabella hash
static int n;
// messaggio errore e stop
void termina(const char *messaggio){
  if(errno!=0) perror(messaggio);
	else fprintf(stderr,"%s\n", messaggio);
  exit(1);
}

/**************************************/

// crea un oggetto di tipo entry con chiave s e valore n
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

/*************************************/

  // inserisce gli elementi passati come argomento
  void aggiungi (char *s) {
    ENTRY *e = entry(s, 1);
    ENTRY *r = hsearch(*e,FIND);
    if(r==NULL) { // la entry è nuova
      r = hsearch(*e,ENTER);
      if(r==NULL) termina("errore o tabella piena");
      //aggiorna il numero di elementi nella tabella
      n++;
    }
    else {
      //la stringa è gia' presente incremento il valore
      assert(strcmp(e->key,r->key)==0);
      int *d = (int *) r->data;
      *d +=1;
      distruggi_entry(e); // questa non la devo memorizzare
    }
  }

/**************************************/
 // ritorna il valore associato alla stringa se presente, sennò 0
  int conta(char *s) {   
    ENTRY *e = entry(s, 1);
    ENTRY *r = hsearch(*e,FIND);
    distruggi_entry(e);
    if(r==NULL) return 0;
    else return (*((int *) r->data));    
  }


// inizio uso da parte di un reader
void read_lock(hash *z)
{
  pthread_mutex_lock(z->ordering);  // coda di ingresso
  pthread_mutex_lock(z->mutex);
  while(z->writing>0)
    pthread_cond_wait(z->cond, z->mutex);  // attende fine scrittura
  z->readers++;
  pthread_mutex_unlock(z->ordering); // faccio passare il prossimo se in coda
  pthread_mutex_unlock(z->mutex);
}

// fine uso da parte di un reader
void read_unlock(hash *z)
{
  assert(z->readers>0);  // ci deve essere almeno un reader (me stesso)
  assert(!z->writing);   // non ci devono essere writer 
  pthread_mutex_lock(z->mutex);
  z->readers--;                  // aggiorno il numero dei readers       
  if(z->readers==0) 
    pthread_cond_signal(z->cond); //segnala a chi è in attesa
  pthread_mutex_unlock(z->mutex);
}

/**************************************/
  
// inizio uso da parte di writer  
void write_lock(hash *z)
{
  pthread_mutex_lock(z->ordering);    // coda di ingresso
  pthread_mutex_lock(z->mutex);
  while(z->writing>0 || z->readers>0)    
    pthread_cond_wait(z->cond, z->mutex);   // attende fine scrittura o lettura
  assert(z->writing==0);
  z->writing++;
  pthread_mutex_unlock(z->ordering); // faccio passare il prossimo se in coda
  pthread_mutex_unlock(z->mutex);
}

// fine uso da parte di un writer
void write_unlock(hash *z)
{
  assert(z->writing>0); // ci deve essere almeno un writer
  pthread_mutex_lock(z->mutex);
  z->writing--;               // aggiorno numero dei writers
  pthread_cond_signal(z->cond);  //segnala a chi è in attesa
  pthread_mutex_unlock(z->mutex);
}

// ritorna il numero di elementi della tabella hash
int size() {return n;}

/**************************************/


