CC=clang -emit-llvm-bc -g -O0

srv: serv.o
	llvm-ld -native serv.o -o $@ -llua -lm
	-rm $@.bc
.o:
	$(CC) -c -o $@.o $@.c

clean:
	-rm *.o srv

# vim: ts=4 sw=4 st=4 sta tw=80
