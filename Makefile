# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
#CC=gcc
CFLAGS = -r -w -v
#-std=c11 -Wall -O 
#-std=c99
LDLIBS=-lm -lrt -pthread 

EXECS= server.out archivio.out client1.out client2.out

# se si scrive solo make di default compila main.c
all: $(EXECS)

server.out = server.py $< $(CFLAGS)

clean: 
	rm -f *.o $(EXECS)
