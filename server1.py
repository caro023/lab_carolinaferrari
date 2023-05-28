#! /usr/bin/env python3
# echo-client.py


# esempio di un client che si connette a echo-server.py


import sys,socket

# valori di default verso cui connettersi 
HOST = "127.0.0.1"  
PORT = 56515   

# creazione del server socket
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    try:
        s.bind((HOST, PORT))
        s.listen()   
        while True:
            print(f"In attesa di un client su porta {PORT}...")
            # mi metto in attesa di una connessione
            conn, addr = s.accept()
            # lavoro con la connessione appena ricevuta 
            with conn:  
                print(f"Contattato da {addr}")
                while True:
                    data = conn.recv(2048) # leggo fino a 64 bytes
                    #print(f"Ricevuti {len(data)} bytes") 
                    tconn = data.decode()
                    print(f"{tconn}")

                    if not data:           # se ricevo 0 bytes 
                        break              # la connessione è terminata
                print("Connessione terminata")
    except KeyboardInterrupt:
      s.shutdown(socket.SHUT_RDWR)
