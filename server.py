#! /usr/bin/env python3
# -*- coding: utf-8 -*-

import subprocess, signal
import os, struct,logging, argparse,socket
import concurrent.futures, threading

HOST = "127.0.0.1"  
PORT = 56515   
lock = threading.Lock()
Max_sequence_length = 2048

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
   #apre le FIFO in scrittura
   fd1 = os.open(Pipelet,os.O_WRONLY)
   fd2 = os.open(Pipesc,os.O_WRONLY)

   #apertura del socket  
   with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    try:
      #permette di riutilizzare la stessa porta        
      s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)            
      s.bind((HOST, PORT))
      s.listen()
      
      with concurrent.futures.ThreadPoolExecutor(max_workers=max) as executor:
        while True:
          # mi metto in attesa di una connessione
          print("In attesa di un client...")
          #il client deve scrivere il tipo di connessione         
          conn, addr = s.accept()
          data = conn.recv(1)
          tconn = data.decode()
          if tconn == "a":
            executor.submit(gestisci_connessione, conn,addr,fd1)
            print('connessione tipo A')
          elif tconn == "b":
            executor.submit(gestisci_connessione, conn,addr,fd2)
            print('connessione tipo B')
          else: print('connessione da client generico')
    except KeyboardInterrupt:
      print('chiusura del server') 
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
    while True:  
      data = conn.recv(2)
      if not data:
          logging.debug(f"Tipo A. Bytes {tot}")
          break
      
      if(struct.unpack("!h",data)[0]==0):
        line = conn.recv(1)
        if(line.decode()==""):
          logging.debug(f"Tipo B. Bytes {tot}")
          break
      
     
      lenght  = struct.unpack("!h",data)[0]      
      assert(lenght<Max_sequence_length)
      tot+=(2+lenght)
      line = recv_all(conn,lenght)
      lock.acquire()
      os.write(fd,data)
      os.write(fd,line)
      lock.release()     
    
  

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

    
  