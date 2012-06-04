all: sin dynplay urirec

sin: sin.c
	gcc sin.c -o sin `pkg-config --cflags --libs gstreamer-0.10`

dynplay: dynplay.c
	gcc dynplay.c -o dynplay `pkg-config --cflags --libs gstreamer-0.10`

urirec: urirec.c
	gcc urirec.c -o urirec `pkg-config --cflags --libs gstreamer-0.10`

clean:
	rm sin dynplay urirec