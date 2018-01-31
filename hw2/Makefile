CC=gcc
FLAGS=-g -fno-stack-check -fno-stack-protector -std=gnu11

all: hello res clean

ckpt: ckpt.c ckpt.h
	cc ckpt.c -fPIC -shared -o libckpt.so $(FLAGS)
	cc ckpt.c -c -o libckpt.o $(FLAGS)
	ar rcs libckpt.a libckpt.o

hello: hello.c
	cc hello.c -o hello $(FLAGS)

res: myrestart.c ckpt
	cc myrestart.c -L. -lckpt -static -Wl,-Ttext-segment=5000000 -Wl,-Tdata=5100000 -Wl,-Tbss=5200000 -o myrestart $(FLAGS)

restart: hello res
	./myrestart myckpt

check: hello res clean
	(sleep 3 && kill -12 `pgrep -n hello` && sleep 2 && make restart) & LD_PRELOAD=./libckpt.so ./hello & (sleep 10 && kill -9 `pgrep -n myrestart`)

gdb: res
	gdb --args myrestart myckpt

clean:
	rm -f myckpt hello myrestart libckpt.*
