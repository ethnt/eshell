CC=gcc
CFLAGS=-I.

eshell: main.o
	$(CC) -o eshell main.o -I.
