CC=gcc
CFLAGS=-Wall

all: server client

server: server.c
	$(CC) $< -o $@ $(CFLAGS)

client: client.c
	$(CC) $< -o $@ $(CFLAGS)

