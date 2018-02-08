#Sample Makefile for Malloc
CC=gcc
CFLAGS=-g -O0 -fPIC -Wall -std=gnu11

TESTS=t-test1 test1 tt
HEADERS= malloc.h #Add list of header files

all:	${TESTS} libmalloc.so

clean:
	rm -rf *.o *.so ${TESTS}

libmalloc.so: malloc.o #add other sources here
	$(CC) $(CFLAGS) -shared -Wl,--unresolved-symbols=ignore-all $< -o $@

%: %.c
	$(CC) $(CFLAGS) $< -o $@ -lpthread

# For every XYZ.c file, generate XYZ.o.
%.o: %.c ${HEADERS}
	$(CC) $(CFLAGS) $< -c -o $@

check1:	libmalloc.so test1 t-test1
	LD_PRELOAD=`pwd`/libmalloc.so ./t-test1

check:	libmalloc.so ${TESTS}
	LD_PRELOAD=`pwd`/libmalloc.so ./test1

dist: clean
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar

tt: t-test1.c malloc.c
	$(CC) t-test1.c malloc.c $(CFLAGS) -lpthread -o tt
