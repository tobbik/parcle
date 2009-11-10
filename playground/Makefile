#CC=clang -Wall -g -O0
#LD=llvm-ld -native
CC=gcc -Wall -g -O0
LD=gcc

all: testserv linkedlist x

testserv: testserv.o
	$(LD) $@.o -o $@ -lpthread -llua -lm

x: x.o
	$(LD) $@.o -o $@ -lpthread

linkedlist: linkedlist.o
	$(LD) $@.o -o $@

.o:
	$(CC) -c -o $@.o $@.c

clean:
	-rm *.o *.bc testserv linkedlist

# vim: ts=4 sw=4 st=4 sta tw=80 list
