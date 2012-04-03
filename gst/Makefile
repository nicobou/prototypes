all: sin

sin: sin.c
	gcc sin.c -o sin `pkg-config --cflags --libs gstreamer-0.10`

clean:
	rm sin