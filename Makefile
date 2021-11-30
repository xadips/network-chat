CC=gcc
CFLAGS=

all: server client

server: server.c
	$(CC) $< -o $@ $(CFLAGS)

client: client.c
	$(CC) $< -o $@ $(CFLAGS)

