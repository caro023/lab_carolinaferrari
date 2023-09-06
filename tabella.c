
#include "tabella.h"


//variabile globale per il numero di elementi nella tabella hash
static int n;

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

void distruggi_entry(ENTRY *e) {
  free(e->key); free(e->data); free(e);
}


// inserisce l'elemento nella tabella hash passato come argomento
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
    distruggi_entry(e); 
  }
}


// ritorna il valore associato alla stringa se presente, altrimenti 0
int conta(char *s) {   
  ENTRY *e = entry(s, 1);
  ENTRY *r = hsearch(*e,FIND);
  distruggi_entry(e);
  if(r==NULL) return 0;
  else return (*((int *) r->data));    
}


// inizio uso da parte di un reader
void read_lock(hash *z) {
  xpthread_mutex_lock(z->ordering,QUI);  // coda di ingresso
  xpthread_mutex_lock(z->mutex,QUI);
  while(z->writing>0)
    xpthread_cond_wait(z->cond, z->mutex,QUI);  // attende fine scrittura
  z->readers++;
  xpthread_mutex_unlock(z->ordering,QUI); // faccio passare il prossimo se in coda
  xpthread_mutex_unlock(z->mutex,QUI);
}

// fine uso da parte di un reader
void read_unlock(hash *z) {
  assert(z->readers>0);  // ci deve essere almeno un reader (me stesso)
  assert(!z->writing);   // non ci devono essere writer 
  xpthread_mutex_lock(z->mutex,QUI);
  z->readers--;   // aggiorno il numero dei readers       
  if(z->readers==0) 
    xpthread_cond_signal(z->cond,QUI); //segnala a chi è in attesa
  xpthread_mutex_unlock(z->mutex,QUI);
}


  
// inizio uso da parte di writer  
void write_lock(hash *z) {
  xpthread_mutex_lock(z->ordering,QUI);   // coda di ingresso
  xpthread_mutex_lock(z->mutex,QUI);
  while(z->writing>0 || z->readers>0)    
    xpthread_cond_wait(z->cond, z->mutex,QUI);  // attende fine scrittura o lettura
  assert(z->writing==0);
  z->writing++;
  xpthread_mutex_unlock(z->ordering,QUI); // faccio passare il prossimo se in coda
  xpthread_mutex_unlock(z->mutex,QUI);
}

// fine uso da parte di un writer
void write_unlock(hash *z) {
  assert(z->writing>0); // ci deve essere almeno un writer
  xpthread_mutex_lock(z->mutex,QUI);
  z->writing--;               // aggiorno numero dei writers
  xpthread_cond_signal(z->cond,QUI);  //segnala a chi è in attesa
  xpthread_mutex_unlock(z->mutex,QUI);
}

// ritorna il numero di elementi della tabella hash
int size() {return n;}



