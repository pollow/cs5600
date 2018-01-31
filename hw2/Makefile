CC=gcc

all: hello res clean

hello: hello.c
	cc hello.c -o hello -g -fno-stack-check -fstack-protector-explicit -fno-stack-protector

res: myrestart.c
	cc -static -Wl,-Ttext-segment=5000000 -Wl,-Tdata=5100000 -Wl,-Tbss=5200000 myrestart.c -g -o myrestart -fno-stack-check -fstack-protector-explicit -fno-stack-protector

test: hello res
	./myrestart myckpt

gdb: res
	gdb --args myrestart myckpt

clean:
	rm myckpt
