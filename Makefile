all: proxyFilter 


CLIBS=-pthread
CC=gcc
CPPFLAGS=
CFLAGS=-g

PROXYOBJS=mtserver.o 

proxyFilter: $(PROXYOBJS)
	$(CC) -o mtserver $(PROXYOBJS)  $(CLIBS)
	$(CC) client.c -o client



clean:
	rm -f *.o
	rm -f mtserver

