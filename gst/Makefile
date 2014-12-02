all: sin dynplay urirec basic decklink imagesink_dynplug rtp_codec listcolorspace

basic: basic.c
	gcc basic.c -o basic `pkg-config --cflags --libs gstreamer-0.10`

decklink: decklink.c
	gcc decklink.c -o decklink `pkg-config --cflags --libs gstreamer-0.10`

dynplay: dynplay.c
	gcc dynplay.c -o dynplay `pkg-config --cflags --libs gstreamer-0.10`

imagesink_dynplug: imagesink_dynplug.c
	gcc imagesink_dynplug.c -o imagesink_dynplug `pkg-config --cflags --libs gstreamer-0.10`

listcolorspace: listcolorspace.c
	gcc listcolorspace.c -o listcolorspace `pkg-config --cflags --libs gstreamer-0.10`

rtp_codec: rtp-codec.c
	gcc rtp-codec.c -o rtp-codec -std=gnu99 `pkg-config --cflags --libs gstreamer-0.10`

sin: sin.c
	gcc sin.c -o sin `pkg-config --cflags --libs gstreamer-0.10`

urirec: urirec.c
	gcc urirec.c -o urirec `pkg-config --cflags --libs gstreamer-0.10`

videoshm-audioplay: videoshm-audioplay.c
	gcc videoshm-audioplay.c -o videoshm-audioplay `pkg-config --cflags --libs gstreamer-0.10 shmdata-0.8`

clean:
	rm sin dynplay urirec basic decklink imagesink_dynplug rtp-codec listcolorspace
