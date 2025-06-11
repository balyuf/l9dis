# This is the Makefile for gcc

all : l9dis

l9dis : l9dis.o
	gcc -o l9dis l9dis.o

l9dis.o : l9dis.c
	gcc -c -Wall -o l9dis.o l9dis.c

clean :
	-rm l9dis l9dis.o
