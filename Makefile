# Makefile for my_wc
CC = gcc

CFLAGS = -lpthread -lm -g

my_wc.o: my_wc.c
	 $(CC) $(CFLAGS) my_wc.c -o my_wc

clean:
	 rm -f my_wc