# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS = -std=c11 -Wall -g -O -pthread
LDLIBS=-lm -lrt -pthread 

SRCS = archivio.c tabella.c rw.c buffer.c
OBJS = $(SRCS:.c=.o)
EXECS = archivio

.PHONY: all clean

all: $(EXECS)

$(EXECS): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< 

clean:
	rm -f *.o $(EXECS)