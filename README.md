# Progetto Laboratorio 2

Il progetto consiste in un server che riceve l'input da due tipi diversi di client e interagisce con un programma archivio.c.

## Scelte implementative

- I file in python client1, client2 e server.py sono eseguibili.
 Per il file archivio.c va creato l'eseguibile attraverso il comando make che dipenderà dalle librerie "rw.h", "buffer.h" e "tabella.h".
- In tabella.h sono presenti le librerie necessarie per avviare il programma, la struct con gli elementi per accedere in modo safe alla tabella hash, le funzioni per il paradigma lettori scrittori e le funzioni relative alla tabella hash.
- In rw.h sono presenti le funzioni e le struct utilizzate per i threads lettori, scrittori e i rispettivi capi.
- In buffer.h sono presenti le struct per accedere al buffer da parte dei capi e dei thread lettori scrittori e le funzioni con le due possibili operazioni da fare sul buffer ossia inserimento e rimozione di una stringa.


### client1

Riceve in input un file e invia una alla volta le linee di testo al server con una connessione di tipo A. 
Per segnalare al server il tipo di connessione viene inviata inizialmente una sequenza di bytes che rappresentano la lettera "a". 
Il file di testo viene letto una riga alla volta che sarà inviata attraverso la funzione send_line(sock, line). Questa funzione trasforma la linea di testo in una sequenza di bytes e ne calcola la lunghezza che sarà inviata al server prima della sequenza stessa.

### client2

Il client2 riceve in input uno o più file di testo.
Nel main viene creato un thread per ogni file passato da riga di comando. 
I threads attraverso la funzione send_file(file_name) si connettono al server con una connessione di tipo B segnalata inizialmente con la prima sequenza di bytes. Successivamente inviano, attraverso la funzione send_file(sock, line), che si differenzia dalla precendente solo per l'utilizzo di una variabile di lock necessaria per la presenza di diversi threads, le varie righe del file di testo.
Infine inviano una sequenza di lunghezza 0 per segnalare la fine della connessione.

### Server

Il server, usando il modulo logging, crea un file "server.log" di livello DEBUG in cui scrive i vari tipi di connessione, l'ora, il giorno in cui sono avvenute e il numero di bytes scritti nelle named pipe 'capolet' e 'caposc' che vengono create se non sono già presenti.

Con il modulo argparse gestisce gli argomenti passati dalla linea di comando. Richiede come argomento obbligatorio il numero massimo di threads che il server può utilizzare contemporaneamente. 
Prima di avviare il main lancia come sottoprocesso il programma archivio, con valgrind se specificato  attraverso l'opzione '-v', e passando due paramentri opzionali, con valore di default 3, -r e -w rispettivamente il numero di threds lettori e scrittori. 

Il server si mette in attesa sulla porta 56515, quando riceve una connessione ne riconosce il tipo attraverso la prima sequenza inviata e avvia un thread con la funzione gestisci_connessione(conn,addr,fd) passando la rispettiva named pipe.

Il thread riceve prima la lunghezza della sequenza di bytes e chiamando la funzione recv_all vista a lezione attende la sequenza stessa. 
Dopo aver ricevuto le sequenze vengono scritte sulla rispettiva FIFO utilizzando una variabile di lock per garantire che siano inserite nell'ordine corretto vista la possibile presenza di altre scritture concorrenti. Quando riceve l'ultima sequenza scrive sul file 'server.log' il tipo di connessione e quanti bytes sono stati ricevuti.

Il server quando riceve il segnale KeyboardInterrupt, corrispondente a SIGINT, chiude e cancella le FIFO, invia al programma 'archivio' il segnale SIGTERM e chiude il socket.


### Archivio

Il file archivio.c riceve da riga di comando il numero di threads scrittori e lettori che verranno creati.

Crea i threads "capo lettore" e "capo scrittore". Essi attraverso la funzione Capo, definita nella libreria "tabella.h", leggono gli input dalla rispettiva FIFO e effettuano una tokenizzazione attraverso la funione strtok_r in quanto è thread safe. 
Inseriscono una copia del token ottenuto in un buffer condiviso attraverso la funzione put(capo_buffer *a,char *str) definita in "buffer.h".
Quando la FIFO viene chiusa in scrittura inviano un carattere di terminazione (NULL) ai threads lettori e scrittori prima di chiuderla anche in lettura e terminare.
La gestione del buffer produttori-consumatori avviene attraverso l'uso di semafori.

I threads scrittori leggono una stringa dal buffer e chiamano la funzione aggiungi(char *s) che inserisce la stringa nella tabella hash se non presente e aggiorna il numero di elementi totali nella tabella contenuto in una variabile globale, altrimenti incrementa il valore associato a essa.
L'accesso alla tabella avviene attraverso un meccanismo lettori scrittori che garantisce l'ordinamento grazie al mutex ordering e quindi fair per entrambi.

I threads lettori leggono una stringa dal buffer e chiamano la funzione conta(char *s) che restituisce il numero di occorrenze prensenti nella tabella hash della stringa e scrive il risultato nel file 'lettori.log'.

I segnali SIGINT e SIGTERM ricevuti dal programma archivio vengono gestiti dal thread gestore. 
Quando viene ricevuto il segnale SIGINT viene stampato su stderr il numero di elementi presenti nella tabella chiamando la funzione size().
Quando viene ricevuto il segnale SIGTERM viene attesa la terminazione dei thread capoWrite e capoRead e viene stampato su stdout il numero di elementi presenti nella tabella hash che viene deallocata prima di far terminare il programma. 

Dopo aver atteso la terminazione del thread gestore e dei threads lettori e scrittori, viene chiuso il file 'lettori.log' e deallocata la memoria.

Avviando il programma con valgrind si avranno dei leak di memoria dovuti alla mancata deallocazione della memoria utilizzata all'interno della tabella hash.
