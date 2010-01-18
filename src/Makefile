CC=clang                                   # modern compiler
#CC=gcc                                    # use gcc - fallback
#CC=ccc-analyzer                            # invoke clang static analyzer
LD=llvm-ld -native
#LD=gcc                                    # fallback
LDFLAGS=-lpthread -llua -lm
CFLAGS=-c -Wall -g -O0

SOURCES=utils.c wsapi.c main.c server.c http.c app.c config.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=parcle

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm *.o parcle *.bc

# vim: ts=4 sw=4 st=4 sta tw=80 list
