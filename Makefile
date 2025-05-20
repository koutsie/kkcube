avx: CFLAGS+=-O3 -march=native -funroll-loops -ffast-math -mavx -mavx2
avx: kkcube

CC=cc
CFLAGS=-lm

kkcube: cube.c
	$(CC) $(CFLAGS) $^ -o $@ 

build_optimize: CFLAGS+=-O3 -march=native -funroll-loops -ffast-math
build_optimize: kkcube

# You might be asking - why - I do not know either.
# gprof kkcube | gprof2dot | dot -Tpng -o output.png
profile: CFLAGS+=-pg -DPROFILE_BUILD
profile: kkcube

.PHONY: build_optimize avx profile clean

clean:
	rm -f kkcube gmon.out