# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS = -std=c11 -Wall -g -O -pthread
LDLIBS=-lm -lrt -pthread 

#EXECS= server.out archivio.out client1.out client2.out

# se si scrive solo make di default compila main.c
#all: $(EXECS)

#server.out = server.py $< $(CFLAGS)

#clean: 
#	rm -f *.o $(EXECS)


SRCS = archivio.c tabella.c rw.c
OBJS = $(SRCS:.c=.o)
TARGET = archivio

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)