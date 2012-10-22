include config.mk

DEP_AWESOME=awesome-status.c parseconfig.c config.mk
LIBS+=parseconfig.o
LD_OPTS+=`pkg-config --libs libconfuse`
CFLAGS+=`pkg-config --cflags libconfuse`

all: awesome-status config-skeleton

awesome-status: $(DEP_AWESOME) $(LIBS)
	gcc $(CFLAGS) $(PLUGINS) -Wall -o awesome-status awesome-status.c $(LIBS) $(LD_OPTS)
	strip awesome-status

parseconfig.o: parseconfig.c parseconfig.h
	gcc $(CFLAGS) -Wall -c parseconfig.c 

config-skeleton: parseconfig.c
	./extract_config > config-syntax
	./generate_skeleton > config-skeleton

install: awesome-status config-skeleton
	mkdir -p $(DESTDIR)/{bin,share/doc/awesome-status/}
	cp awesome-status $(DESTDIR)/bin
	cp Changelog COPYING LICENSE README config-skeleton $(DESTDIR)/share/doc/awesome-status/
	
clean:
	rm -f awesome-status *.o config-skeleton config-syntax

