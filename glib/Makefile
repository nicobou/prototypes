all: dirtrack

dirtrack: dir_tracking.c
	gcc -Wall -o dir_track dir_tracking.c `pkg-config --cflags --libs glib-2.0 gobject-2.0 gio-2.0`

clean:
	rm -f dir_track

