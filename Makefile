CC = gcc
CFLAGS = -Wall -pthread

OBJS = server.o logger.o 

all: server client 

server: $(OBJS)
	$(CC) $(OBJS) -o server $(CFLAGS)

server.o: server.c shared.h 
	$(CC) -c server.c $(CFLAGS)

logger.o: logger.c shared.h
	$(CC) -c logger.c $(CFLAGS)

client: client.c 
	$(CC) client.c -o client $(CFLAGS)

clean:
	rm -f *.o server client game.log
