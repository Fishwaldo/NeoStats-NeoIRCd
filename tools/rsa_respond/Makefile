# $Id: Makefile,v 1.2 2002/08/13 14:45:13 fishwaldo Exp $

all: respond

respond: respond.c
	gcc -g -O2 -Wall -o respond respond.c -lcrypto -ggdb

clean:
	rm -f respond *.core *~
