#! /usr/bin/env python3
import subprocess, signal
import os, struct,logging, argparse,socket
import concurrent.futures, threading,errno

HOST = "127.0.0.1"  
PORT = 56515   

# configurazione del logging
logging.basicConfig(filename= 'server.log',
                    level=logging.DEBUG, datefmt='%d/%m/%y %H:%M:%S',
                    format='%(asctime)s - %(levelname)s - %(message)s')


# Variabili globali con i nomi delle pipe da usare
Pipesc = "caposc"
Pipelet = "capolet"

#args = shlex.split(command_line)

def main(max):
   
   #se non esistono crea le pipe
   if not os.path.exists(Pipelet):
    os.mkfifo(Pipelet)
   if not os.path.exists(Pipesc):
    os.mkfifo(Pipesc)

   #apre le pipe in scrittura
   fd1 = os.open(Pipesc,os.O_WRONLY)
   fd2 = os.open(Pipelet,os.O_WRONLY)


  
   with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    try:  
      s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)            
      s.bind((HOST, PORT))
      s.listen()
      assert max>0, "Il numero di thread deve essere maggiore di 0"
      with concurrent.futures.ThreadPoolExecutor(max_workers=max) as executor:
        while True:
          print("In attesa di un client...")
          logging.debug("Inizia esecuzione di main")
          #deve scrivere tipo di connessione e num byte
          # mi metto in attesa di una connessione
          conn, addr = s.accept()
          data = recv_all(conn,1)
          tconn = data.decode()
          if tconn == "a":
          # l'esecuzione di submit non è bloccante
          # fino a quando ci sono thread liberi
            executor.submit(gestisci_connessione, conn,addr,fd1)
            print('connessione tipo A')
          elif tconn == "b":
            executor.submit(gestisci_connessione, conn,addr,fd2)
            print('connessione tipo B')
          else: print('connessione da client ne tipo A ne B')
    except KeyboardInterrupt:
      print('Va bene smetto...')
      os.unlink(Pipesc)
      os.unlink(Pipelet)
      p.send_signal(signal.SIGTERM)
      s.shutdown(socket.SHUT_RDWR)
  
  # gestisci una singola connessione con un client
def gestisci_connessione(conn,addr,fd): 
  # in questo caso potrei usare direttamente conn
  # e l'uso di with serve solo a garantire che conn venga chiusa all'uscita del blocco
  # ma in generale with esegue le necessarie inzializzazione e il clean-up finale
  #inizializzato a 1 perche byte che mando per capire il tipo di connessione
  tot=1
  while True:
    with conn:  
      print(f"{threading.current_thread().name} contattato da {addr}")
      # ---- invio un byte inutile (il codice ascii di x) 
      #data = recv_all(conn,2)
      data = conn.recv(2)

      if(len(data)==1 & (data.decode()==0)):#penso(?????)
        logging.debug(f"Tipo B. Bytes {tot}")
        break

      if not data:
          print(f"{threading.current_thread().name} finito con {addr}")
          logging.debug(f"Tipo A. Bytes {tot}")
          break
      
      assert len(data)==2
      lenght  = struct.unpack("!i",data)[0]      
      assert(lenght<2048)
      tot+=(2+lenght)
      line = recv_all(conn,lenght)
      #su quale pipe devo scrivere(?????????)
      os.write(fd,line)      
      
    
    
  

# riceve esattamente n byte e li restituisce in un array di byte
# il tipo restituto è "bytes": una sequenza immutabile di valori 0-255
# analoga alla readn che abbiamo visto nel C
def recv_all(conn,n):
  chunks = b''
  bytes_recd = 0
  while bytes_recd < n:
    chunk = conn.recv(min(n - bytes_recd, 1024))
    if len(chunk) == 0:
      raise RuntimeError("socket connection broken")
    chunks += chunk
    bytes_recd = bytes_recd + len(chunk)
  return chunks
 
 


# questo codice viene eseguito solo se il file è eseguito direttamente
# e non importato come modulo con import da un altro file
if __name__ == '__main__':
  parser = argparse.ArgumentParser(description="server ",formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument("max", help="max number of thread", type=int)
  parser.add_argument('-r', help='number_read', type = int,default=3)  
  parser.add_argument('-w', help='number_write', type = int, default=3)  
  parser.add_argument('-v', '--verbose', action='store_true', help='valgrind') 
  args = parser.parse_args()

  if args.verbose:
    # esegue il main in background con valgrind
    p = subprocess.Popen(["valgrind","--leak-check=full", 
                      "--show-leak-kinds=all", 
                      "--log-file=valgrind-%p.log", 
                      "archivio", args.r, args.w])
    print("Ho lanciato il processo:", p.pid)
  else:
    # esegui main come processo in background
    p = subprocess.Popen(["./archivio", args.r, args.w])    
    print("Ho lanciato il processo:", p.pid)

  main(args.max)