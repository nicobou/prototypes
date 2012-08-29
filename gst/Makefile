all: sin dynplay urirec basic decklink imagesink_dynplug

imagesink_dynplug: imagesink_dynplug.c
	gcc imagesink_dynplug.c -o imagesink_dynplug `pkg-config --cflags --libs gstreamer-0.10`
decklink: decklink.c
	gcc decklink.c -o decklink `pkg-config --cflags --libs gstreamer-0.10`
basic: basic.c
	gcc basic.c -o basic `pkg-config --cflags --libs gstreamer-0.10`
sin: sin.c
	gcc sin.c -o sin `pkg-config --cflags --libs gstreamer-0.10`

dynplay: dynplay.c
	gcc dynplay.c -o dynplay `pkg-config --cflags --libs gstreamer-0.10`

urirec: urirec.c
	gcc urirec.c -o urirec `pkg-config --cflags --libs gstreamer-0.10`

clean:
	rm sin dynplay urirec basic decklink