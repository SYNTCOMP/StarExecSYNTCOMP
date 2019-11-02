CC=gcc
CFLAGS=-Wall -DNLOG -g -DCOMP -DNDEBUG -O3
LFLAGS= -DSOLVER -L/root/demiurge-1.2.0/third-party/depqbf/ -lqdpll -I/root/demiurge-1.2.0/third-party/depqbf/  -L/root/demiurge-1.2.0/third-party/lingeling/code/ -llgl -I/root/demiurge-1.2.0/third-party/lingeling/code/ -lm
bloqqer: bloqqer.o bloqqer.h blqrcfg.o makefile
	$(CC) $(CFLAGS) -o $@ bloqqer.o blqrcfg.o $(LFLAGS)
bloqqer.o: bloqqer.c bloqqer.h makefile
	$(CC) $(CFLAGS) -c bloqqer.c $(LFLAGS)
libbloqqer.o: bloqqer.c bloqqer.h makefile
	$(CC) -DLIBBLOQQER $(CFLAGS) -o libbloqqer.o -c bloqqer.c $(LFLAGS)
blqrcfg.o: blqrcfg.c blqrcfg.h makefile
	$(CC) $(CLFAGS) -c blqrcfg.c
blqrcfg.h: bloqqer.c bloqqer.h mkconfig VERSION makefile
	rm -f $@; ./mkconfig > $@
libbloqqer.a: libbloqqer.o
	ar rc $@ libbloqqer.o
	ranlib $@
analyze:
	clang --analyze $(CFLAGS) $(shell ls *.c *.h)
clean:
	rm -f `cat .gitignore`
