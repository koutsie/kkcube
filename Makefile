CC=cc
CFLAGS=-lm

kkcube: cube.c
	$(CC) $(CFLAGS) $^ -o $@ 

build_optimize: CFLAGS+=-O3 -march=native -funroll-loops
build_optimize: kkcube

.PHONY: build_optimize clean

clean:
	rm -f kkcube 
