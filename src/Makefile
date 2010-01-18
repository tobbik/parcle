CC=clang -g -O0
LD=llvm-ld -native

targets=utils.o wsapi.o main.o server.o http.o app.o config.o
parcle: $(targets)
	$(LD) $(targets) -o $@ -lpthread -llua -lm
	-rm $@.bc

.o:
	$(CC) -c -o $@.o $@.c

clean:
	-rm *.o parcle

# vim: ts=4 sw=4 st=4 sta tw=80 list
