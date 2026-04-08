CC = gcc
CFLAGS = -Wall
LDFLAGS = -lpthread

all: server client worker program

server: server.c
	$(CC) $(CFLAGS) server.c -o server $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) client.c -o client

worker: worker.c
	$(CC) $(CFLAGS) worker.c -o worker

program: program.c
	$(CC) $(CFLAGS) program.c -o program

clean:
	rm -f server client program.out received_*.out output_*.txt
