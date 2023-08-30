#! /usr/bin/env python3
# -*- coding: utf-8 -*-

import subprocess, signal
import os, struct,logging, argparse,socket,time
import concurrent.futures, threading,errno

HOST = "127.0.0.1"  
PORT = 56515   
lock = threading.Lock()

# configurazione del 
#va cambiata la configurazioen
logging.basicConfig(filename= 'server.log',
                    level=logging.DEBUG, datefmt='%d/%m/%y %H:%M:%S',
                    format='%(asctime)s - %(levelname)s - %(message)s')


# Variabili globali con i nomi delle pipe da usare
Pipesc = "caposc"
Pipelet = "capolet"
    #se non esistono crea le pipe
if not os.path.exists(Pipelet):
    os.mkfifo(Pipelet)
if not os.path.exists(Pipesc):
    os.mkfifo(Pipesc)

def main(max):
   assert max>0, "Il numero di thread deve essere maggiore di 0"
   #apre le pipe in scrittura e lettura per non rendere la scrittura bloccante
   fd1 = os.open(Pipelet,os.O_WRONLY)
   fd2 = os.open(Pipesc,os.O_WRONLY)

  
   with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    try:  
      
      s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)            
      s.bind((HOST, PORT))
      s.listen()
      
      with concurrent.futures.ThreadPoolExecutor(max_workers=max) as executor:
        while True:
          print("In attesa di un client...")
          #deve scrivere tipo di connessione e num byte
          # mi metto in attesa di una connessione
          conn, addr = s.accept()
          #data = recv_all(conn,1)
          data = conn.recv(1)
          tconn = data.decode()
          if tconn == "a":
          # l'esecuzione di submit non è bloccante
          # fino a quando ci sono thread liberi
            executor.submit(gestisci_connessione, conn,addr,fd1)
            print('connessione tipo A')
          elif tconn == "b":
            executor.submit(gestisci_connessione, conn,addr,fd2)
            print('connessione tipo B')
          else: print('connessione da client generico')
    except KeyboardInterrupt:
      print('Va bene smetto...') 
      os.close(fd1)
      os.close(fd2) 
      os.unlink(Pipesc)
      os.unlink(Pipelet)
      p.send_signal(signal.SIGTERM)
      s.shutdown(socket.SHUT_RDWR)
      s.close()
    
  # gestisci una singola connessione con un client
def gestisci_connessione(conn,addr,fd):
 
  tot=1
  with conn:  
    print(f"{threading.current_thread().name} contattato da {addr}")
     #per i numeri python utilizza precisione a 28 bit, quindi max 4 byte
    while True:  
      data = conn.recv(2)
      if not data:
          #print(f"{threading.current_thread().name} finito con {addr}")
          logging.debug(f"Tipo A. Bytes {tot}")
          break
      
      if(struct.unpack("!h",data)[0]==0):
        line = conn.recv(1)
        if(line.decode()==""):
          print(f"{threading.current_thread().name} finito con {addr}")
          logging.debug(f"Tipo B. Bytes {tot}")
          break
      
     # assert len(data)==2
      lenght  = struct.unpack("!h",data)[0]      
      assert(lenght<2048)
      tot+=(2+lenght)
      line = recv_all(conn,lenght)
      #print(f"{line.decode()}")
      lock.acquire()
      os.write(fd,data)
      os.write(fd,line)
      lock.release()
     
    
  

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
                      "./archivio", str(args.r), str(args.w)])
    print("Ho lanciato il processo:", p.pid)
  else:
    # esegui main come processo in background
    p = subprocess.Popen(["./archivio", str(args.r), str(args.w)])    
    print("Ho lanciato il processo:", p.pid)
  main(args.max)
 # output, error = p.communicate()
 # print(output)

    
  