all: ram cache

clean:
	rm -rf ram cache

ram: ram.c
	gcc -o ram -O3 ram.c -std=c99

cache: cache.c
	gcc -o cache -O3 cache.c -std=c99

dist: clean
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar
