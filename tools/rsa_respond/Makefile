# $Id: Makefile,v 1.1 2002/08/13 14:36:50 fishwaldo Exp $

all: respond

respond: respond.c
	gcc -g -O2 -Wall -o respond respond.c -lcrypto -ggdb

clean:
	rm -f respond *.core *~
