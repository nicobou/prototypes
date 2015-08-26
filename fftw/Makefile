all: 1Dbasic

1Dbasic: 1Dbasic.c
	gcc -Wall -std=c99 -o 1Dbasic 1Dbasic.c `pkg-config --cflags --libs fftw3` -lm

clean:
	rm -f 1Dbasic

